# CLAUDE.md

このファイルは、このリポジトリでコードを操作する際にClaude Code（claude.ai/code）へのガイダンスを提供します。

## Claude Code使用時の重要な指示

- **回答言語**: 必ず日本語で回答すること
- **出力スタイル**: Explanatory（説明的）モードを使用
  - 実行する操作の理由を明確に説明
  - コードの変更内容を詳細に解説
  - 各ステップの目的と結果を記述

## プロジェクト概要

* M5スタックでGNSSデータをCSVで記録し、CloudflareのR2に記録したCSVデータをPutでアップロードする。
* アップロードされたR2のCSVデータをPages FunctionsでGPXに変換する
* Pages Functionsの変換は、Scheduler WorkerによってCronで定期的にトリガ、またはHttpアクセスによってトリガされる。

M5-GNSS-LOGGERは、3つの主要コンポーネントからなるGNSS（GPS）ロギングシステムです：
1. **M5Stack Core2ファームウェア** ([m5GnssLogger/](m5GnssLogger/)) - GPSデータを取得しCloudflare R2にアップロード
2. **Cloudflare Pages Functions** ([cloud/pages-functions/gpx-converter/](cloud/pages-functions/gpx-converter/)) - R2にあるCSVをGPX形式に変換しR2に出力する
3. **Cloudflare Scheduler Worker** ([cloud/workers/gpx-converter-scheduler/](cloud/workers/gpx-converter-scheduler/)) - 2.のPages Functionsを定期的または手動でトリガする

**データフロー**: M5StackがGNSSデータを取得 → SDカードに保存 → 対象WiFi検出時にR2にアップロード → Workerが自動的にトリガー → FunctionsがCSVをGPXに変換

### M5Stackファームウェア (PlatformIO)

```bash
cd m5GnssLogger

# 依存関係をインストール
pio pkg install

# ビルド
pio run --environment m5stack-core2

# アップロード
pio run --target upload --environment m5stack-core2

# シリアル出力をモニター（デバッグ用）
pio device monitor
```

### Cloudflare Scheduler Worker

```bash
cd cloud/workers/gpx-converter-scheduler

# 依存関係をインストール
npm install

# Cloudflareにデプロイ
bash deploy.sh

```

### Cloudflare Pages Functions

```bash
cd cloud/pages-functions/gpx-converter

# Cloudflareにデプロイ
bash deploy.sh

```

### コードフォーマット（C++）

```bash
# リポジトリルートから - フォーマットチェック（読み取り専用、CI用）
docker build -t clang-format-check ./tools
docker run --rm -e DRY_RUN=true -v "$(pwd):/workspace" clang-format-check

# ファイルをフォーマット（その場で修正）
docker run --rm -v "$(pwd):/workspace" clang-format-check
```

フォーマットは、main/masterブランチへのpush/PR時にGitHub Actionsで強制適用されます。プロジェクトルートの[`.clang-format`](.clang-format)を使用（Googleベースのスタイル、2スペースインデント、100文字行制限）。

## アーキテクチャ

### M5Stackファームウェアモジュール ([m5GnssLogger/src/](m5GnssLogger/src/))

| モジュール | ヘッダー | ソース | 目的 |
|--------|--------|---------|---------|
| GNSS | [gnss.h](m5GnssLogger/include/gnss.h) | [gnss.cpp](m5GnssLogger/src/gnss.cpp) | GPSデータ取得（u-bloxライブラリ）、データフィルタリング |
| Display | [display.h](m5GnssLogger/include/display.h) | [display.cpp](m5GnssLogger/src/display.cpp) | 画面描画（LovyanGFX）、ダブルバッファリング |
| Storage | [storage.h](m5GnssLogger/include/storage.h) | [storage.cpp](m5GnssLogger/src/storage.cpp) | SDカードへのCSV書き込み、ファイル名生成 |
| WiFi | [my_wifi.h](m5GnssLogger/include/my_wifi.h) | [my_wifi.cpp](m5GnssLogger/src/my_wifi.cpp) | WiFi接続管理、SSID検出 |
| R2 | [r2.h](m5GnssLogger/include/r2.h) | [r2.cpp](m5GnssLogger/src/r2.cpp) | Cloudflare R2アップロード（AWS Signature V4） |
| Upload Manager | [upload_manager.h](m5GnssLogger/include/upload_manager.h) | [upload_manager.cpp](m5GnssLogger/src/upload_manager.cpp) | WiFi接続とR2アップロードの統合管理 |

**主要な型**: [`GNSS_DATA`](m5GnssLogger/include/config.h:10)構造体は、全てのGNSS読み取り値（緯度/経度/高度/速度/時刻/fix品質）を含みます。

**環境設定**: `.env`ファイルから設定を読み込みます（gitignore対象）。テンプレートは[`.env.example`](m5GnssLogger/.env.example)を参照。

**しきい値** ([config.h](m5GnssLogger/include/config.h:42)): GNSSデータはHDOP（< 6.0）、最小衛星数（≥ 5）、位置変化（> 0.001°）でフィルタリングされます。

### Cloudflare Scheduler Worker ([cloud/workers/gpx-converter-scheduler/src/scheduler.ts](cloud/workers/gpx-converter-scheduler/src/scheduler.ts))

- **トリガー方式**:
  - Cronトリガー: 毎時0分に自動実行（`"0 * * * *"`）
  - HTTPトリガー: `POST /trigger` で手動実行可能
- **機能**: Pages FunctionsのGPX変換エンドポイントを呼び出し
- **環境変数**: `PAGES_FUNCTION_URL` でPages FunctionsのURLを指定

### Cloudflare Pages Functions ([cloud/pages-functions/gpx-converter/functions/gpx-converter.ts](cloud/pages-functions/gpx-converter/functions/gpx-converter.ts))

- **トリガー**: Scheduler WorkerからのHTTPリクエスト、または直接HTTPアクセス
- **R2バインディング**: `BUCKET` 経由でR2バケットにアクセス
- **機能**:
  - `gnss-data/YYYYMMDD/` ディレクトリをスキャン
  - CSVファイルをGPX 1.1形式に変換
  - 4MB超のファイルは自動分割（Google My Maps対応）
  - 出力先: `gnss-data/YYYYMMDD/gpx/filename.gpx`
- **共通処理**: [shared/gnss-utils.ts](cloud/pages-functions/gpx-converter/shared/gnss-utils.ts) に変換ロジックを記述

**バインディング**: R2バケットは `BUCKET` として [wrangler.toml](cloud/pages-functions/gpx-converter/wrangler.toml:6) でバインドされます。

## セキュリティ

機密ファイルをコミットしないでください。これらはgitignoreされています：
- `m5GnssLogger/.env` - WiFi認証情報、R2キーが含まれます
- `cloud/.config/.dev.vars` - Cloudflare認証情報が含まれます

代わりにテンプレートファイルを使用してください：
- `m5GnssLogger/.env.example`
- `cloud/.config/.dev.vars.example`

## ファイル命名規則

- **CSVファイル**: `gnss_csv_data_YYYYMMDD_HHMMSS.csv` （処理済みデータ）
- **CSV生ファイル**: `gnss_csv_data_YYYYMMDD_HHMMSS_raw.csv` （全GNSSメッセージ）
- **GPXファイル**: `gpx/`サブフォルダ内の同じ基本名に`.gpx`拡張子

## CSVデータフォーマット

### 処理済みデータ（通常CSV）
品質しきい値を満たしたフィルタリング済みGNSSデータ（HDOP < 6.0、衛星数 ≥ 5、位置変化 > 0.001°）
```
date,time,lat,lng,alt,spd,siv,hdop
2024-01-01,12:00:00,35.6895000,139.6917000,50.0,0.0,12,1.2
```

### 生データ（Raw CSV）
フィルタリング前の全GNSSメッセージ
```
date,time,lat,lng,alt,spd,siv,hdop,fixType,pdop,...
```

## デバイスボタン操作

| ボタン | 機能 |
|--------|----------|
| ボタンA | 表示モード切替（詳細/簡易） |
| ボタンB | 記録停止（確認ダイアログあり） |
| ボタンC | 停止確認のキャンセル |

## GPX変換トリガー方法

### 自動トリガー（Cron）
Scheduler Workerは毎時0分に自動的に実行され、Pages Functionsを呼び出します。

```bash
# Schedulerのステータスを確認
curl https://gpx-converter-scheduler.<account>.workers.dev/test
```

### 手動トリガー（HTTP）
```bash
# Scheduler経由でトリガー
curl -X POST https://gpx-converter-scheduler.<account>.workers.dev/trigger

# Pages Functionsを直接トリガー
curl -X POST https://gpx-converter.<account>.pages.dev/gpx-converter
```
