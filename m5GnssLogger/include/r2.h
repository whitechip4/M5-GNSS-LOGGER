#ifndef R2_H
#define R2_H

#ifdef TESTING
#include "mock_httpclient.h"
#else
#include <HTTPClient.h>
#endif
#include "config.h"

/**
 * @brief R2設定構造体
 */
typedef struct {
  char accountId[64];
  char bucketName[64];
  char accessKey[128];
  char secretKey[128];
  char region[32];
} R2_CONFIG;

/**
 * @brief R2アップロードモジュールクラス
 */
class R2Module {
public:
  /**
   * @brief コンストラクタ
   */
  R2Module();

  /**
   * @brief 初期化
   * @param accountId CloudflareアカウントID
   * @param bucketName バケット名
   * @param accessKey アクセスキー
   * @param secretKey シークレットキー
   * @param region リージョン（デフォルト: "auto"）
   */
  void begin(const char* accountId,
             const char* bucketName,
             const char* accessKey,
             const char* secretKey,
             const char* region = "auto");

  /**
   * @brief ファイルをR2にアップロード
   * @param localFilePath SDカード上のローカルファイルパス
   * @param remoteKey R2上のキー（パス）
   * @return アップロード成功時true
   */
  bool uploadFile(const char* localFilePath, const char* remoteKey);

  /**
   * @brief ファイルをR2にアップロード（文字列データ）
   * @param data アップロードするデータ
   * @param dataSize データサイズ
   * @param remoteKey R2上のキー（パス）
   * @return アップロード成功時true
   */
  bool uploadData(const char* data, size_t dataSize, const char* remoteKey);

  /**
   * @brief 日付ベースのR2キーを生成
   * @param baseName ベースファイル名
   * @param buffer 出力バッファ
   * @param bufferSize バッファサイズ
   * @param data 日時データ
   */
  static void
  generateKey(const char* baseName, char* buffer, size_t bufferSize, const GNSS_DATA& data);

#ifdef TESTING
  // Make private methods accessible for testing
public:
  String _sha256(const char* data, size_t len);
  void _hmacSha256(const char* key, size_t keyLen, const char* data, size_t dataLen, char* output);
  void _hmacSha256Binary(const char* key,
                         size_t keyLen,
                         const char* data,
                         size_t dataLen,
                         unsigned char* output);
  String _generateSignature(const char* method,
                            const char* key,
                            const char* host,
                            const char* region,
                            const char* service,
                            const char* timestamp,
                            const char* payloadHash);

private:
#endif

private:
  char _accountId[64];
  char _bucketName[64];
  char _accessKey[128];
  char _secretKey[128];
  char _region[32];
  char _endpoint[256];

  /**
   * @brief R2エンドポイントURLを生成
   */
  void _buildEndpoint();

#ifndef TESTING
  /**
   * @brief AWS Signature V4 認証ヘッダーを生成
   * @param method HTTPメソッド
   * @param key R2キー
   * @param host ホスト名
   * @param region リージョン
   * @param service サービス名（"s3"）
   * @param timestamp タイムスタンプ
   * @param payloadHash ペイロードハッシュ
   * @return 認証ヘッダー
   */
  String _generateSignature(const char* method,
                            const char* key,
                            const char* host,
                            const char* region,
                            const char* service,
                            const char* timestamp,
                            const char* payloadHash);

  /**
   * @brief SHA256ハッシュを計算
   * @param data データ
   * @param len データ長
   * @return ハッシュ値（16進数文字列）
   */
  String _sha256(const char* data, size_t len);

  /**
   * @brief HMAC-SHA256を計算
   * @param key 鍵
   * @param keyLen 鍵の長さ
   * @param data データ
   * @param dataLen データの長さ
   * @param output 出力バッファ（65バイト以上必要）
   */
  void _hmacSha256(const char* key, size_t keyLen, const char* data, size_t dataLen, char* output);

  /**
   * @brief HMAC-SHA256 computation returning binary data
   * @param key HMAC key
   * @param keyLen Length of key
   * @param data Data to HMAC
   * @param dataLen Length of data
   * @param output Output buffer (must be 32 bytes)
   */
  void _hmacSha256Binary(const char* key,
                         size_t keyLen,
                         const char* data,
                         size_t dataLen,
                         unsigned char* output);
#endif
};

#endif  // R2_H