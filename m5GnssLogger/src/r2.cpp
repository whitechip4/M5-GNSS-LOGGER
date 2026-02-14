#include "r2.h"
#include "display.h"
#include "util.h"
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <Arduino.h>
#include <SD.h>

// Global display module instance (declared in main.cpp)
extern DisplayModule displayModule;

R2Module::R2Module() {
  _accountId[0] = '\0';
  _bucketName[0] = '\0';
  _accessKey[0] = '\0';
  _secretKey[0] = '\0';
  _region[0] = '\0';
  _endpoint[0] = '\0';
}

void R2Module::begin(const char* accountId,
                     const char* bucketName,
                     const char* accessKey,
                     const char* secretKey,
                     const char* region) {
  strncpy(_accountId, accountId, sizeof(_accountId) - 1);
  _accountId[sizeof(_accountId) - 1] = '\0';

  strncpy(_bucketName, bucketName, sizeof(_bucketName) - 1);
  _bucketName[sizeof(_bucketName) - 1] = '\0';

  strncpy(_accessKey, accessKey, sizeof(_accessKey) - 1);
  _accessKey[sizeof(_accessKey) - 1] = '\0';

  strncpy(_secretKey, secretKey, sizeof(_secretKey) - 1);
  _secretKey[sizeof(_secretKey) - 1] = '\0';

  strncpy(_region, region, sizeof(_region) - 1);
  _region[sizeof(_region) - 1] = '\0';

  _buildEndpoint();
}

void R2Module::_buildEndpoint() {
  snprintf(_endpoint, sizeof(_endpoint), "https://%s.r2.cloudflarestorage.com", _accountId);
}

bool R2Module::uploadFile(const char* localFilePath, const char* remoteKey) {
  displayModule.logMessage("Reading file...");
  debug_print("R2", " Opening file: %s", localFilePath);

  // Read file from SD card
  File file = SD.open(localFilePath, FILE_READ);
  if (!file) {
    displayModule.logMessage("File not found");
    debug_print("R2", " File not found!");
    return false;
  }

  size_t fileSize = file.size();
  debug_print("R2", " File size: %u bytes", fileSize);
  if (fileSize == 0) {
    displayModule.logMessage("Empty file");
    debug_print("R2", " Empty file!");
    file.close();
    return false;
  }

  // Allocate buffer for file content
  char* buffer = (char*)malloc(fileSize + 1);
  debug_print("R2", " Memory allocated: %u bytes", fileSize + 1);
  if (!buffer) {
    displayModule.logMessage("Memory error");
    debug_print("R2", " Memory allocation failed!");
    file.close();
    return false;
  }

  // Read file content
  size_t bytesRead = file.read((uint8_t*)buffer, fileSize);
  file.close();

  debug_print("R2", " Bytes read: %u / %u", bytesRead, fileSize);
  if (bytesRead != fileSize) {
    displayModule.logMessage("Read error");
    debug_print("R2", " Read error!");
    free(buffer);
    return false;
  }

  buffer[bytesRead] = '\0';

  // Upload the data
  bool result = uploadData(buffer, bytesRead, remoteKey);

  free(buffer);
  return result;
}

bool R2Module::uploadFileStream(const char* localFilePath,
                                const char* remoteKey,
                                int8_t timezoneOffset) {
  displayModule.logMessage("Preparing upload...");
  debug_print("R2", " Opening file: %s", localFilePath);

  File file = SD.open(localFilePath, FILE_READ);
  if (!file) {
    displayModule.logMessage("File not found");
    debug_print("R2", " File not found!");
    return false;
  }

  size_t fileSize = file.size();
  debug_print("R2", " File size: %u bytes", fileSize);
  if (fileSize == 0) {
    displayModule.logMessage("Empty file");
    debug_print("R2", " Empty file!");
    file.close();
    return false;
  }

  displayModule.logMessage("Uploading to R2...");
  bool result = _uploadStreamData(file, fileSize, remoteKey, timezoneOffset);

  file.close();
  return result;
}

bool R2Module::_uploadStreamData(Stream& stream,
                                 size_t dataSize,
                                 const char* remoteKey,
                                 int8_t timezoneOffset) {
  WiFiClientSecure client;
  client.setInsecure();

  // Parse host from endpoint
  String endpoint = String(_endpoint);
  String host = endpoint.substring(8);  // Remove "https://"
  int pathStart = host.indexOf('/');
  if (pathStart >= 0) {
    host = host.substring(0, pathStart);
  }

  // Build full URI (path only, without host)
  String uri = "/" + String(_bucketName) + "/" + String(remoteKey);

  // Create HTTP request
  HTTPClient http;
  http.begin(client, host, 443, uri, true);

  // Use UNSIGNED-PAYLOAD for streaming (no pre-calculated hash needed)
  String payloadHash = "UNSIGNED-PAYLOAD";
  debug_print("R2", " Using UNSIGNED-PAYLOAD for streaming");

  // Get current timestamp
  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  char timestamp[17];
  strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &timeinfo);
  char dateStamp[9];
  strftime(dateStamp, sizeof(dateStamp), "%Y%m%d", &timeinfo);
  debug_print("R2", " Timestamp: %s", timestamp);

  // Build timezone metadata header value
  char timezoneHeader[32];
  snprintf(timezoneHeader, sizeof(timezoneHeader), "%d", timezoneOffset);
  debug_print("R2", " Timezone offset: %s hours", timezoneHeader);

  // Generate authorization header with UNSIGNED-PAYLOAD
  String authHeader = _generateSignature("PUT",
                                         remoteKey,
                                         host.c_str(),
                                         _region,
                                         "s3",
                                         timestamp,
                                         payloadHash.c_str(),
                                         timezoneHeader);

  // Set headers
  http.addHeader("Host", host);
  http.addHeader("Content-Type", "text/csv");
  http.addHeader("x-amz-content-sha256", payloadHash);
  http.addHeader("x-amz-date", timestamp);
  http.addHeader("x-amz-meta-timezone", timezoneHeader);
  http.addHeader("Authorization", authHeader);

  // Send PUT request with streaming - HTTPClient will read in chunks
  debug_print("R2", " Starting streaming upload of %u bytes", dataSize);
  int httpCode = http.sendRequest("PUT", &stream, dataSize);
  debug_print("R2", " HTTP response code: %d", httpCode);

  // Check response
  bool success = (httpCode == 200);

  if (success) {
    displayModule.logMessage("Upload successful!");
    debug_print("R2", " Upload successful!");
  } else {
    String response = http.getString();
    displayModule.logMessage("Upload failed");
    debug_print("R2", " Upload failed - Code: %d, Response: %s", httpCode, response.c_str());
  }

  http.end();
  delay(1000);

  return success;
}

bool R2Module::uploadData(const char* data,
                          size_t dataSize,
                          const char* remoteKey,
                          int8_t timezoneOffset) {
  displayModule.logMessage("Uploading to R2...");

  WiFiClientSecure client;
  client.setInsecure();  // For HTTPS

  // Parse host from endpoint
  String endpoint = String(_endpoint);
  String host = endpoint.substring(8);  // Remove "https://"
  int pathStart = host.indexOf('/');
  if (pathStart >= 0) {
    host = host.substring(0, pathStart);
  }

  // Build full URI (path only, without host)
  String uri = "/" + String(_bucketName) + "/" + String(remoteKey);

  // Create HTTP request
  HTTPClient http;
  http.begin(client, host, 443, uri, true);

  // Calculate payload hash
  String payloadHash = _sha256(data, dataSize);
  debug_print("R2", " Payload hash: %s", payloadHash.c_str());

  // Get current timestamp
  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  char timestamp[17];
  strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &timeinfo);
  char dateStamp[9];
  strftime(dateStamp, sizeof(dateStamp), "%Y%m%d", &timeinfo);
  debug_print("R2", " Timestamp: %s", timestamp);

  // Build timezone metadata header value
  char timezoneHeader[32];
  snprintf(timezoneHeader, sizeof(timezoneHeader), "%d", timezoneOffset);
  debug_print("R2", " Timezone offset: %s hours", timezoneHeader);

  // Generate authorization header
  String authHeader = _generateSignature("PUT",
                                         remoteKey,
                                         host.c_str(),
                                         _region,
                                         "s3",
                                         timestamp,
                                         payloadHash.c_str(),
                                         timezoneHeader);

  // Set headers
  http.addHeader("Host", host);
  http.addHeader("Content-Type", "text/csv");
  http.addHeader("x-amz-content-sha256", payloadHash);
  http.addHeader("x-amz-date", timestamp);
  http.addHeader("x-amz-meta-timezone", timezoneHeader);
  http.addHeader("Authorization", authHeader);

  // Send PUT request
  debug_print("R2", " Connecting to: %s", host.c_str());
  debug_print("R2", " URI: %s", uri.c_str());
  int httpCode = http.PUT((uint8_t*)data, dataSize);
  debug_print("R2", " HTTP response code: %d", httpCode);

  // Check response
  bool success = (httpCode == 200);

  if (success) {
    displayModule.logMessage("Upload successful!");
    debug_print("R2", " Upload successful!");
  } else {
    String response = http.getString();
    displayModule.logMessage("Upload failed");
    debug_print("R2", " Upload failed - Code: %d, Response: %s", httpCode, response.c_str());
  }

  http.end();
  delay(1000);

  return success;
}

String R2Module::_generateSignature(const char* method,
                                    const char* key,
                                    const char* host,
                                    const char* region,
                                    const char* service,
                                    const char* timestamp,
                                    const char* payloadHash,
                                    const char* timezoneHeader) {
  // Create canonical request
  String canonicalUri = "/" + String(_bucketName) + "/" + String(key);
  debug_print("R2", " Canonical URI: %s", canonicalUri.c_str());

  String canonicalQueryString = "";
  String canonicalHeaders = "host:" + String(host) + "\n" +
                            "x-amz-content-sha256:" + String(payloadHash) + "\n" +
                            "x-amz-date:" + String(timestamp) + "\n";
  String signedHeaders = "host;x-amz-content-sha256;x-amz-date";

  // Add timezone metadata header if provided
  if (timezoneHeader != nullptr && strlen(timezoneHeader) > 0) {
    canonicalHeaders += "x-amz-meta-timezone:" + String(timezoneHeader) + "\n";
    signedHeaders += ";x-amz-meta-timezone";
  }

  debug_print("R2", " Canonical Headers: %s", canonicalHeaders.c_str());

  String canonicalRequest = String(method) + "\n" + canonicalUri + "\n" + canonicalQueryString +
                            "\n" + canonicalHeaders + "\n" + signedHeaders + "\n" + payloadHash;
  debug_print("R2", "Canonical Request: %s", canonicalRequest.c_str());

  // Create string to sign
  String hashedCanonicalRequest = _sha256(canonicalRequest.c_str(), canonicalRequest.length());
  debug_print("R2", " Hashed Canonical Request: %s", hashedCanonicalRequest.c_str());

  // Get date stamp from timestamp
  char dateStamp[9];
  strncpy(dateStamp, timestamp, 8);
  dateStamp[8] = '\0';

  String credentialScope =
      String(dateStamp) + "/" + String(region) + "/" + String(service) + "/aws4_request";
  debug_print("R2", " Credential Scope: %s", credentialScope.c_str());

  String stringToSign = "AWS4-HMAC-SHA256\n" + String(timestamp) + "\n" + credentialScope + "\n" +
                        hashedCanonicalRequest;
  debug_print(
      "R2", " String to Sign (first 100 chars): %s", stringToSign.substring(0, 100).c_str());

  // Calculate signature
  // Derive signing key using binary HMAC
  String secretKeyStr = "AWS4" + String(_secretKey);
  const char* kSecret = secretKeyStr.c_str();

  unsigned char kDate[32];
  _hmacSha256Binary(kSecret, strlen(kSecret), dateStamp, strlen(dateStamp), kDate);

  unsigned char kRegion[32];
  _hmacSha256Binary((const char*)kDate, 32, region, strlen(region), kRegion);

  unsigned char kService[32];
  _hmacSha256Binary((const char*)kRegion, 32, service, strlen(service), kService);

  unsigned char kSigning[32];
  _hmacSha256Binary((const char*)kService, 32, "aws4_request", 12, kSigning);

  // Final signature - compute binary then hex-encode
  unsigned char signatureBinary[32];
  _hmacSha256Binary(
      (const char*)kSigning, 32, stringToSign.c_str(), stringToSign.length(), signatureBinary);

  char signatureHex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(signatureHex + (i * 2), "%02x", signatureBinary[i]);
  }
  signatureHex[64] = '\0';
  debug_print("R2", " Calculated Signature: %s", signatureHex);

  // Build authorization header
  String authorizationHeader = "AWS4-HMAC-SHA256 Credential=" + String(_accessKey) + "/" +
                               credentialScope + ", " + "SignedHeaders=" + signedHeaders + ", " +
                               "Signature=" + String(signatureHex);
  debug_print("R2", " Auth Header length: %d", authorizationHeader.length());

  return authorizationHeader;
}

String R2Module::_sha256(const char* data, size_t len) {
  unsigned char hash[32];
  mbedtls_sha256_context ctx;

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)data, len);
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  char hexHash[65];
  for (int i = 0; i < 32; i++) {
    sprintf(hexHash + (i * 2), "%02x", hash[i]);
  }
  hexHash[64] = '\0';

  return String(hexHash);
}

void R2Module::_hmacSha256(const char* key,
                           size_t keyLen,
                           const char* data,
                           size_t dataLen,
                           char* output) {
  unsigned char hash[32];

  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  (const unsigned char*)key,
                  keyLen,
                  (const unsigned char*)data,
                  dataLen,
                  hash);

  for (int i = 0; i < 32; i++) {
    sprintf(output + (i * 2), "%02x", hash[i]);
  }
  output[64] = '\0';
}

void R2Module::_hmacSha256Binary(const char* key,
                                 size_t keyLen,
                                 const char* data,
                                 size_t dataLen,
                                 unsigned char* output) {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  (const unsigned char*)key,
                  keyLen,
                  (const unsigned char*)data,
                  dataLen,
                  output);
}