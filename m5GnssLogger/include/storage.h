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

  /**
   * @brief ファイル名をアップロードキューに追加
   * @param filename 追加するファイル名（先頭スラッシュ付き）
   * @return 成功時true
   */
  bool addFileToUploadList(const char* filename);

  /**
   * @brief アップロードキューからファイル名を削除
   * @param filename 削除するファイル名（先頭スラッシュ付き）
   * @return 成功時true
   */
  bool removeFileFromUploadList(const char* filename);

  /**
   * @brief アップロードキューから次のファイルを取得
   * @param filenameBuffer ファイル名を格納するバッファ
   * @param bufferSize バッファサイズ
   * @return ファイルが見つかった場合true、空の場合false
   */
  bool getNextFileToUpload(char* filenameBuffer, size_t bufferSize);

  /**
   * @brief アップロードキューが空か確認
   * @return 空の場合true
   */
  bool isUploadQueueEmpty();

  /**
   * @brief アップロードキューファイルを初期化（存在しない場合作成）
   */
  void initializeUploadQueue();

private:
  File _file;

  /**
   * @brief ヘッダーを書き込み
   * @param file ファイルオブジェクト
   */
  void _writeHeader(File& file);

  /**
   * @brief ファイルから1行を読み込む（プライベートヘルパー）
   * @param file ファイルオブジェクト
   * @param buffer 読み込みバッファ
   * @param bufferSize バッファサイズ
   * @return 行が読めた場合true、ファイル終了時false
   */
  bool _readLineFromFile(File& file, char* buffer, size_t bufferSize);
};

#endif  // STORAGE_H
