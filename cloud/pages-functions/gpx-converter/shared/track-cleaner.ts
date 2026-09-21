// GNSSトラックの位置飛び（マルチパス発散・屋内ドリフト）を除去・補正する
//
// R2上の全記録（2025-12〜2026-08、34ファイル）を解析して設計した。観測された異常は3種類:
//  1. 高速発散: 数点で数十kmジャンプし高度が数千mに暴走（報告速度200km/h超、垂直速度10m/s超）
//  2. ゆっくりドリフト: 静止中に高度が-800〜+5800mへ沈降・上昇しつつ水平にも数百m〜10km流れる
//     （HDOP・衛星数・報告速度はすべて正常値のまま。1点ごとの比較では検出できない）
//  3. 単発スパイク: 1点だけ数十〜数百m飛ぶ
//
// 方針: 「最後に採用した点（anchor）から物理的に到達可能か」を判定し、
//  - 水平: 200km/h x 経過時間 + マージン
//  - 垂直: マージン + 勾配0.3 x 水平移動距離（時間項なし。静止中の沈降を経過時間で正当化しない）
//  - 30s/120s/600s前の採用点に対しても垂直整合を要求（ドリフトを長い基線で捕まえる）
//  - 棄却後・時間ギャップ後の再採用は、続く60秒間が自己整合している場合のみ（ゴミによるanchor乗っ取り防止）
//  - 高度が-150m未満だが水平は整合する点（トンネル明け等）は位置を残し標高だけ直前値で補正

export interface TrackPoint {
  date: string;
  time: string;
  lat: number;
  lng: number;
  alt: number;
  spd: number;
  siv: number;
  hdop: number;
  hacc?: number;
  /** 補正後の標高（未補正なら alt と同じ） */
  ele?: number;
}

export interface CleanResult<T extends TrackPoint> {
  points: T[];
  dropped: number;
  altitudeCorrected: number;
  /** 長期棄却からの強制復帰回数（デバッグ用） */
  escapes: number;
}

export const CLEANER_PARAMS = {
  /** 想定最大水平速度 [m/s]（実走行で149km/hを観測しているため200km/h） */
  maxSpeedMps: 200 / 3.6,
  /** 報告速度の上限 [km/h] */
  maxReportedSpeedKmh: 200,
  /** 水平ジャンプ判定のマージン [m] */
  horizontalMarginM: 20,
  /** 垂直判定のマージン [m]（屋外静止で±10m、半屋内で±60m程度のノイズ） */
  verticalMarginM: 80,
  /** 許容勾配（トンネル明けの水平4.2km/高度差570m=0.14を通す） */
  maxGrade: 0.3,
  /** 30秒基線での持続的垂直速度の上限 [m/s]（山岳道路でも2〜3m/s） */
  maxSustainedVerticalMps: 5,
  /** 垂直整合を要求する基線の長さ [s] */
  anchorAgesSec: [30, 120, 600],
  /** 経路長に加算する最低報告速度 [km/h]（静止中のふらつきを経路長に数えない） */
  movingSpeedKmh: 10,
  /** 再採用に必要な自己整合の継続時間 [s] */
  confirmWindowSec: 60,
  /** 自己整合判定での1点ごとの垂直速度上限 [m/s] */
  confirmVerticalMps: 8,
  /** 自己整合判定に必要な最低点数 */
  confirmMinPoints: 3,
  /** これを超える時間ギャップ後は再採用に自己整合を要求 [s] */
  gapSec: 30,
  /** これ未満の標高は異常とみなし直前の採用点の標高で補正 [m] */
  altitudeFloorM: -150,
  /** 補正中はこの値以上に戻るまで補正を続ける [m]（ヒステリシス） */
  altitudeRecoverM: -50,
  /** 棄却が続いた場合に自己整合を条件として強制復帰するまでの時間 [s] */
  escapeSec: 900,
  /** u-blox hAcc の上限 [m]（列が存在する場合のみ） */
  maxHaccM: 20,
};

const EARTH_RADIUS_M = 6371000;

function toRadians(deg: number): number {
  return (deg * Math.PI) / 180;
}

/** 2点間の水平距離（ハーバサイン公式） [m] */
export function distanceM(lat1: number, lng1: number, lat2: number, lng2: number): number {
  const p1 = toRadians(lat1);
  const p2 = toRadians(lat2);
  const dp = toRadians(lat2 - lat1);
  const dl = toRadians(lng2 - lng1);
  const a =
    Math.sin(dp / 2) * Math.sin(dp / 2) + Math.cos(p1) * Math.cos(p2) * Math.sin(dl / 2) ** 2;
  return 2 * EARTH_RADIUS_M * Math.asin(Math.sqrt(Math.min(1, a)));
}

/** "YYYY/MM/DD" + "HH:MM:SS" を秒数に変換（タイムゾーンは差分計算にのみ使うため無視） */
export function toEpochSec(date: string, time: string): number {
  const [y, mo, d] = date.split("/").map(Number);
  const [h, mi, s] = time.split(":").map(Number);
  return Date.UTC(y, mo - 1, d, h, mi, s) / 1000;
}

interface Accepted<T extends TrackPoint> {
  point: T;
  t: number;
  ele: number;
  /** 移動中の累積経路長 [m] */
  cum: number;
}

interface Timed<T extends TrackPoint> {
  point: T;
  t: number;
}

function isInvalid(p: TrackPoint): boolean {
  return (
    Math.abs(p.lat) < 0.001 ||
    Math.abs(p.lng) < 0.001 ||
    p.hdop >= 50 ||
    p.siv === 0 ||
    (p.hacc !== undefined && p.hacc > CLEANER_PARAMS.maxHaccM)
  );
}

function isVerticalOk(fromAlt: number, toAlt: number, horizontalM: number): boolean {
  return (
    Math.abs(toAlt - fromAlt) <=
    CLEANER_PARAMS.verticalMarginM + CLEANER_PARAMS.maxGrade * horizontalM
  );
}

function isWithinEnvelope(from: TrackPoint, fromT: number, to: TrackPoint, toT: number): boolean {
  const dt = Math.max(1, toT - fromT);
  const d = distanceM(from.lat, from.lng, to.lat, to.lng);
  return (
    d <= CLEANER_PARAMS.maxSpeedMps * dt + CLEANER_PARAMS.horizontalMarginM &&
    isVerticalOk(from.alt, to.alt, d)
  );
}

/**
 * i番目以降 confirmWindowSec 秒間の点列が自己整合しているか
 * （ゴミは高度が暴れるので、実在の位置だけがこの条件を満たす）
 */
function isConfirmed<T extends TrackPoint>(rows: Timed<T>[], i: number): boolean {
  const start = rows[i];
  let prev = start;
  let n = 0;
  for (let j = i + 1; j < rows.length; j++) {
    const q = rows[j];
    if (q.t - start.t > CLEANER_PARAMS.confirmWindowSec) break;
    if (isInvalid(q.point)) continue;
    const dt = Math.max(1, q.t - prev.t);
    if (
      q.point.spd > CLEANER_PARAMS.maxReportedSpeedKmh ||
      !isWithinEnvelope(prev.point, prev.t, q.point, q.t) ||
      Math.abs(q.point.alt - prev.point.alt) / dt > CLEANER_PARAMS.confirmVerticalMps
    ) {
      return false;
    }
    prev = q;
    n++;
  }
  return n >= CLEANER_PARAMS.confirmMinPoints;
}

/** age秒以上前の採用点のうち最新のもの */
function findAnchorOlderThan<T extends TrackPoint>(
  hist: Accepted<T>[],
  t: number,
  age: number
): Accepted<T> | undefined {
  for (let k = hist.length - 1; k >= 0; k--) {
    if (t - hist[k].t >= age) return hist[k];
  }
  return undefined;
}

/**
 * トラックから位置飛びを除去し、異常標高を補正する
 * 入力順は時系列であること。返却点の ele に補正後標高が入る
 */
export function cleanTrack<T extends TrackPoint>(points: T[]): CleanResult<T> {
  const rows: Timed<T>[] = points.map((p) => ({ point: p, t: toEpochSec(p.date, p.time) }));
  const kept: T[] = [];
  let dropped = 0;
  let altitudeCorrected = 0;
  let escapes = 0;

  let anchor: Accepted<T> | undefined;
  let hist: Accepted<T>[] = [];
  let rejectedSinceT: number | undefined;
  const ages = CLEANER_PARAMS.anchorAgesSec;

  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];
    const p = row.point;
    if (isInvalid(p)) {
      dropped++;
      continue;
    }

    if (anchor === undefined) {
      // 最初の点はゴミの可能性があるので自己整合を確認してから採用
      if (isConfirmed(rows, i)) {
        anchor = { point: p, t: row.t, ele: p.alt, cum: 0 };
        hist = [anchor];
        kept.push(Object.assign(p, { ele: p.alt }));
      } else {
        dropped++;
      }
      continue;
    }

    let ok =
      p.spd <= CLEANER_PARAMS.maxReportedSpeedKmh &&
      isWithinEnvelope(anchor.point, anchor.t, p, row.t);
    const step = distanceM(anchor.point.lat, anchor.point.lng, p.lat, p.lng);
    let cum = anchor.cum + (p.spd >= CLEANER_PARAMS.movingSpeedKmh ? step : 0);

    if (ok) {
      // 長い基線での垂直整合（静止中の沈降・上昇ドリフト対策）
      // 許容量は「移動中の経路長」と「変位」の大きい方で決める（山道のスイッチバック対策）
      for (const age of ages) {
        const h = findAnchorOlderThan(hist, row.t, age);
        if (h === undefined) continue;
        const d = Math.max(cum - h.cum, distanceM(h.point.lat, h.point.lng, p.lat, p.lng));
        if (!isVerticalOk(h.point.alt, p.alt, d)) {
          ok = false;
          break;
        }
        if (
          age === ages[0] &&
          Math.abs(p.alt - h.point.alt) / (row.t - h.t) > CLEANER_PARAMS.maxSustainedVerticalMps
        ) {
          ok = false;
          break;
        }
      }
    }

    const gap = row.t - anchor.t;
    if (ok && rejectedSinceT === undefined && gap <= CLEANER_PARAMS.gapSec) {
      // 通常の連続点
    } else if (ok && isConfirmed(rows, i)) {
      // 棄却やギャップの後: 実在の位置であることを後続点で確認できた
      rejectedSinceT = undefined;
    } else if (
      rejectedSinceT !== undefined &&
      row.t - rejectedSinceT >= CLEANER_PARAMS.escapeSec &&
      isConfirmed(rows, i)
    ) {
      // 長時間棄却が続いた（本当に移動した可能性）ので基準をリセットして復帰
      rejectedSinceT = undefined;
      hist = [];
      cum = 0;
      escapes++;
    } else {
      if (rejectedSinceT === undefined) rejectedSinceT = row.t;
      dropped++;
      continue;
    }

    // 標高補正はヒステリシス付き: floor未満で開始し、recover以上に戻るまで直前の補正値を維持する
    // （トンネル明けで-150前後を数分うろつくと、補正の有無が点ごとに切り替わって階段状になるため）
    let ele = p.alt;
    const correcting = anchor.ele !== anchor.point.alt;
    if (
      p.alt < CLEANER_PARAMS.altitudeFloorM ||
      (correcting && p.alt < CLEANER_PARAMS.altitudeRecoverM)
    ) {
      ele = anchor.ele;
      altitudeCorrected++;
    }
    const accepted: Accepted<T> = { point: p, t: row.t, ele, cum };
    kept.push(Object.assign(p, { ele }));
    anchor = accepted;
    hist.push(accepted);
    while (hist.length > 1 && row.t - hist[0].t > ages[ages.length - 1] + 60) {
      hist.shift();
    }
  }

  return { points: kept, dropped, altitudeCorrected, escapes };
}
