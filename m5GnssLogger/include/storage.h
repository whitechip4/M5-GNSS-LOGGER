#ifndef STORAGE_H
#define STORAGE_H

#include <SD.h>
#include "config.h"

/**
 * @brief ストレージモジュールクラス
 */
class StorageModule {
public:
  /**
   * @brief コンストラクタ
   */
  StorageModule();

  /**
   * @brief 初期化
   * @return 成功時true
   */
  bool begin();

  /**
   * @brief SDカードが準備できているかチェック
   * @return 準備できている場合true
   */
  bool isReady();

  /**
   * @brief ヘッダーを書き込み
   * @param fileName ファイル名
   * @return 成功時true
   */
  bool writeHeader(const char* fileName);

  /**
   * @brief GNSSデータを書き込み
   * @param data 書き込むGNSSデータ
   * @param fileName ファイル名
   * @return 成功時true
   */
  bool writeData(const GNSS_DATA& data, const char* fileName);

  /**
   * @brief 生データを書き込み
   * @param data 書き込むGNSSデータ
   * @param fileName ファイル名
   * @return 成功時true
   */
  bool writeRawData(const GNSS_DATA& data, const char* fileName);

  /**
   * @brief ファイル名を生成（日時ベース）
   * @param baseName ベース名
   * @param buffer バッファ
   * @param bufferSize バッファサイズ
   * @param data 日時データ
   * @param isRaw 生データ用かどうか
   */
  static void generateFileName(const char* baseName,
                               char* buffer,
                               size_t bufferSize,
                               const GNSS_DATA& data,
                               bool isRaw = false);

private:
  File _file;

  /**
   * @brief ヘッダーを書き込み
   * @param file ファイルオブジェクト
   */
  void _writeHeader(File& file);
};

#endif  // STORAGE_H
