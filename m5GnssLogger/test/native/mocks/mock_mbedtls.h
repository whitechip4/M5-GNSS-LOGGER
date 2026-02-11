#ifndef MOCK_MBEDTLS_H
#define MOCK_MBEDTLS_H

// Mock mbedtls types and functions for native testing
// This allows testing crypto functions on host machine without ESP32

#include <cstddef>

typedef struct {
  int dummy;
} mbedtls_sha256_context;

typedef struct {
  int dummy;
} mbedtls_md_type_t;

// Mock constants
#define MBEDTLS_MD_SHA256 0

// Mock SHA256 functions (do-nothing implementations for testing structure)
inline void mbedtls_sha256_init(mbedtls_sha256_context* ctx) {
  (void)ctx;
}

inline void mbedtls_sha256_free(mbedtls_sha256_context* ctx) {
  (void)ctx;
}

inline void mbedtls_sha256_starts(mbedtls_sha256_context* ctx, int dummy) {
  (void)ctx;
  (void)dummy;
}

inline void
mbedtls_sha256_update(mbedtls_sha256_context* ctx, const unsigned char* data, size_t len) {
  (void)ctx;
  (void)data;
  (void)len;
}

inline void mbedtls_sha256_finish(mbedtls_sha256_context* ctx, unsigned char* output) {
  (void)ctx;
  (void)output;
}

// Mock HMAC functions
inline mbedtls_md_type_t* mbedtls_md_info_from_type(int type) {
  static mbedtls_md_type_t info;
  (void)type;
  return &info;
}

inline void mbedtls_md_hmac(mbedtls_md_type_t* info,
                            const unsigned char* key,
                            size_t key_len,
                            const unsigned char* data,
                            size_t data_len,
                            unsigned char* output) {
  (void)info;
  (void)key;
  (void)key_len;
  (void)data;
  (void)data_len;
  (void)output;
}

#endif  // MOCK_MBEDTLS_H
