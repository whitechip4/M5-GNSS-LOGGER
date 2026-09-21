// 座標から国を推定する（外部APIなし・矩形判定）
//
// 用途はログの「国別フィルタ」なので、国境付近の厳密さより「オフラインで確実に動く」ことを優先する。
// 主な記録地（日本・台湾）は複数の矩形で島ごとに囲い、韓国・中国と重なる海域はできるだけ避けている。
// 判定は配列の先頭から順に行い、最初に一致した国を返す。

export interface CountryInfo {
  /** ISO 3166-1 alpha-2 */
  code: string;
  /** 日本語名（ビューア表示用） */
  name: string;
}

interface BBox {
  minLat: number;
  maxLat: number;
  minLng: number;
  maxLng: number;
}

interface CountryRule extends CountryInfo {
  boxes: BBox[];
}

const box = (minLat: number, maxLat: number, minLng: number, maxLng: number): BBox => ({
  minLat,
  maxLat,
  minLng,
  maxLng,
});

// 小さい国・重なりやすい国を先に置く
const RULES: CountryRule[] = [
  { code: "TW", name: "台湾", boxes: [box(21.8, 25.4, 119.9, 122.1)] },
  { code: "HK", name: "香港", boxes: [box(22.15, 22.6, 113.8, 114.45)] },
  { code: "MO", name: "マカオ", boxes: [box(22.1, 22.22, 113.52, 113.62)] },
  { code: "SG", name: "シンガポール", boxes: [box(1.15, 1.48, 103.6, 104.1)] },
  {
    code: "KR",
    name: "韓国",
    // 対馬（129.2〜129.5E）を含めないよう東端を129.1に留め、釜山側は別枝で囲う
    boxes: [box(33.1, 38.65, 125.5, 129.1), box(34.8, 38.65, 129.1, 129.6)],
  },
  {
    code: "JP",
    name: "日本",
    boxes: [
      box(41.3, 45.6, 139.3, 146.0), // 北海道
      box(30.9, 41.6, 129.3, 142.3), // 本州・四国・九州・対馬
      box(24.0, 30.9, 122.9, 131.4), // 沖縄・先島・奄美
      box(24.2, 27.9, 141.0, 154.0), // 小笠原・南鳥島
    ],
  },
  { code: "TH", name: "タイ", boxes: [box(5.6, 20.5, 97.3, 105.7)] },
  { code: "VN", name: "ベトナム", boxes: [box(8.4, 23.4, 102.1, 109.5)] },
  { code: "MY", name: "マレーシア", boxes: [box(0.8, 7.4, 99.6, 119.3)] },
  { code: "PH", name: "フィリピン", boxes: [box(4.6, 21.2, 116.9, 126.6)] },
  { code: "ID", name: "インドネシア", boxes: [box(-11.0, 6.1, 95.0, 141.0)] },
  { code: "CN", name: "中国", boxes: [box(18.1, 53.6, 73.5, 135.1)] },
  { code: "AU", name: "オーストラリア", boxes: [box(-43.7, -10.6, 113.1, 153.7)] },
  { code: "NZ", name: "ニュージーランド", boxes: [box(-47.3, -34.3, 166.4, 178.6)] },
  { code: "GB", name: "イギリス", boxes: [box(49.9, 60.9, -8.2, 1.8)] },
  { code: "FR", name: "フランス", boxes: [box(42.3, 51.1, -5.2, 8.3)] },
  { code: "DE", name: "ドイツ", boxes: [box(47.2, 55.1, 5.8, 15.1)] },
  { code: "IT", name: "イタリア", boxes: [box(36.6, 47.1, 6.6, 18.6)] },
  { code: "ES", name: "スペイン", boxes: [box(36.0, 43.8, -9.3, 3.4)] },
  {
    code: "US",
    name: "アメリカ",
    boxes: [
      box(24.4, 49.4, -125.0, -66.9),
      box(51.2, 71.4, -179.2, -129.9),
      box(18.9, 22.3, -160.3, -154.8),
    ],
  },
  { code: "CA", name: "カナダ", boxes: [box(41.7, 83.2, -141.1, -52.6)] },
];

export const UNKNOWN_COUNTRY: CountryInfo = { code: "XX", name: "不明" };

/** 座標から国を推定する。該当なしは UNKNOWN_COUNTRY */
export function lookupCountry(lat: number, lng: number): CountryInfo {
  if (!Number.isFinite(lat) || !Number.isFinite(lng)) {
    return UNKNOWN_COUNTRY;
  }
  for (const rule of RULES) {
    for (const b of rule.boxes) {
      if (lat >= b.minLat && lat <= b.maxLat && lng >= b.minLng && lng <= b.maxLng) {
        return { code: rule.code, name: rule.name };
      }
    }
  }
  return UNKNOWN_COUNTRY;
}
