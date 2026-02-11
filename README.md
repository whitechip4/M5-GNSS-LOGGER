# M5-GNSS-LOGGER

M5Stack Core2を使用してGNSSデータを記録し、Cloudflare R2へ自動アップロードしてGPX形式に変換するシステム

## 機能

### M5Stack Core2 (m5GnssLogger)
- ✅ GNSSデータの1秒ごとの記録
- ✅ SDカードへのCSV保存
- ✅ ボタンBで記録停止（確認ダイアログ付き）
- ✅ 指定WiFiアクセスポイント検出時の自動R2アップロード
- ✅ 有効データと生データの両方を保存
- ✅ 画面表示モード切り替え（ボタンA）

### Cloudflare Workers (cloud/workers/gpx-converter)
- ✅ R2への新規CSVファイル追加を検知
- ✅ CSVからGPX形式への自動変換
- ✅ 変換後のGPXファイルをR2へ保存
- ✅ 日付ごとのフォルダ構成

## プロジェクト構成

```
M5-GNSS-LOGGER/
├── m5GnssLogger/          # M5Stackファームウェア
│   ├── include/
│   │   ├── config.h       # 設定定義
│   │   ├── display.h      # 表示モジュール
│   │   ├── gnss.h         # GNSSモジュール
│   │   ├── storage.h      # SDカードモジュール
│   │   ├── wifi.h         # WiFiモジュール（新規）
│   │   └── r2.h          # R2アップロードモジュール（新規）
│   ├── src/
│   │   ├── main.cpp       # メイン処理（更新）
│   │   ├── display.cpp
│   │   ├── gnss.cpp
│   │   ├── storage.cpp
│   │   ├── wifi.cpp        # WiFi実装（新規）
│   │   └── r2.cpp         # R2実装（新規）
│   ├── platformio.ini     # PlatformIO設定（更新）
│   └── .env.example      # 設定テンプレート（新規）
└── cloud/                # Cloudflare Workers（新規）
    └── workers/
        └── gpx-converter/
            ├── src/
            │   └── index.ts       # Worker実装
            ├── wrangler.toml       # Wrangler設定
            ├── package.json
            ├── tsconfig.json
            ├── .dev.vars.example # 環境変数テンプレート
            └── README.md
```

## セキュリティについて

このプロジェクトは公開レポジトリです。機密情報を含むファイルは`.gitignore`によってバージョン管理から除外されています：

- `m5GnssLogger/.env` - WiFi/R2設定ファイル
- `cloud/workers/gpx-converter/.dev.vars` - Workers環境変数

これらのファイルは**絶対にコミットしないでください**。代わりに以下のテンプレートファイルを使用してください：

- `m5GnssLogger/.env.example` - M5Stack設定テンプレート
- `cloud/workers/gpx-converter/.dev.vars.example` - Workers設定テンプレート

### 実際の設定方法

1. テンプレートファイルをコピー
   ```bash
   # M5Stack用
   cp m5GnssLogger/.env.example m5GnssLogger/.env
   
   # Workers用
   cp cloud/workers/gpx-converter/.dev.vars.example cloud/workers/gpx-converter/.dev.vars
   ```

2. 実際の設定値を入力
3. 実ファイル（`.env`や`.dev.vars`）は`.gitignore`によって保護されます

### 機密情報の一覧

以下の情報は機密情報として扱われます：
- WiFi SSIDとパスワード
- Cloudflare Account ID
- R2 Access KeyとSecret Key
- APIトークン

これらの値を誤ってGitHubなどに公開しないよう、十分に注意してください。

## セットアップ

### 1. Cloudflare R2の設定

1. Cloudflare DashboardでR2バケットを作成
   - バケット名: `hobby-data`（または任意の名前）
   - リージョン: 任意

2. R2 APIトークンを作成
   - R2 → Manage R2 API Tokens → Create API Token
   - パーミッション: Object Read & Write
   - 作成されたAccess KeyとSecret Keyを保存

### 2. Cloudflare Workersのセットアップ

```bash
cd cloud/workers/gpx-converter

# 依存パッケージのインストール
npm install

# 環境変数の設定（テンプレートからコピー）
cp .dev.vars.example .dev.vars
# .dev.vars を編集してCloudflare Account IDを入力

# Cloudflareにログイン
npx wrangler login

# Workerをデプロイ
npm run deploy
```

### 3. R2バインディングの設定

Cloudflare DashboardでWorkers設定を確認:
1. Workers & Pages → gpx-converter
2. Settings → Variables & Secrets
3. R2 Bucket Bindingsを確認（自動で設定されているはず）

### 4. M5Stackの設定

```bash
cd m5GnssLogger

# 設定ファイルを作成（テンプレートからコピー）
cp .env.example .env

# .envを編集（機密情報を入力）
```

`.env`ファイルの内容（機密情報を含みます）:
```env
WIFI_SSID=your_wifi_ssid
WIFI_PASSWORD=your_wifi_password
R2_ACCOUNT_ID=your_account_id_here
R2_BUCKET_NAME=hobby-data
R2_ACCESS_KEY=your_access_key_here
R2_SECRET_KEY=your_secret_key_here
R2_REGION=auto
```

### 5. M5Stackファームウェアのビルド

```bash
cd m5GnssLogger

# 依存関係のインストール
pio pkg install

# ビルドとアップロード
pio run --target upload
```

## 使用方法

### M5Stackの操作

| ボタン | 機能 |
|--------|--------|
| 電源ON | 自動で記録開始 |
| ボタンA | 表示モード切り替え（詳細/シンプル） |
| ボタンB | 記録停止（確認ダイアログ表示） |
| ボタンB (確認時) | 記録停止を確定 |
| ボタンC (確認時) | キャンセルして記録継続 |

### アップロードの流れ

1. 記録停止時、設定されたWiFi SSIDをスキャン
2. 該当SSIDが見つかった場合、WiFiに接続
3. R2へCSVファイルをアップロード:
   - `gnss-data/YYYYMMDD/gnss_csv_data_YYYYMMDD_HHMMSS.csv`
   - `gnss-data/YYYYMMDD/gnss_csv_data_YYYYMMDD_HHMMSS_raw.csv`
4. WiFiを切断
5. SDカードにはファイルを残す

### GPX変換の流れ

1. R2へ新しいCSVファイルがアップロードされる
2. Cloudflare Workerが自動的にトリガー
3. CSVを解析してGPX形式に変換
4. GPXファイルをR2へ保存:
   - `gnss-data/YYYYMMDD/gpx/gnss_csv_data_YYYYMMDD_HHMMSS.gpx`

## データフォーマット

### CSV形式

ヘッダー行:
```
date,time,lat,lng,alt,spd,siv,hdop
```

データ行例:
```
2024/01/01,12:00:00,35.6895,139.6917,50.0,5.5,8,1.2
2024/01/01,12:00:01,35.6896,139.6918,50.1,5.6,8,1.1
```

### GPX形式

標準的なGPX 1.1形式で以下の情報を含みます:
- トラックポイント（緯度・経度・高度）
- タイムスタンプ
- 速度
- 衛星数
- HDOP（精度指標）

## 開発

### M5Stackのデバッグ

```bash
cd m5GnssLogger
pio run --target upload
pio device monitor
```

### Workersのローカルテスト

```bash
cd cloud/workers/gpx-converter
npm run dev
```

### Workersのログ確認

```bash
npm run tail
```


