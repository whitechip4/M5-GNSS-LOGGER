#ifndef VERSION_H
#define VERSION_H

/**
 * @brief ファームウェアのバージョン情報
 *
 * FIRMWARE_VERSION はリリース時に手動で上げる（セマンティックバージョン）。
 * FIRMWARE_GIT_REV / FIRMWARE_BUILD_DATE は tools/version.py がビルド時に git から注入する。
 * 起動ログと詳細表示に出し、R2 アップロード時に x-amz-meta-firmware としてファイルにも刻印する。
 */
#define FIRMWARE_VERSION "1.1.0"

#ifndef FIRMWARE_GIT_REV
#define FIRMWARE_GIT_REV "unknown"
#endif

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "unknown"
#endif

// 例: "1.1.0+d7160f2"（未コミット変更ありなら "1.1.0+d7160f2-dirty"）
#define FIRMWARE_VERSION_FULL FIRMWARE_VERSION "+" FIRMWARE_GIT_REV

#endif  // VERSION_H
