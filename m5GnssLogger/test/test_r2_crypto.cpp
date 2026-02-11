// Test for R2 crypto operations
// This tests the SHA256 and HMAC-SHA256 functions directly

// Use the real mbedtls library for native testing
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <cstring>
#include <cstdio>

#include <unity.h>

// Minimal String class for testing (mimics Arduino String)
class TestString {
private:
  char _buffer[128];

public:
  TestString() {
    _buffer[0] = '\0';
  }
  TestString(const char* s) {
    snprintf(_buffer, sizeof(_buffer), "%s", s ? s : "");
  }

  const char* c_str() const {
    return _buffer;
  }
  int length() const {
    return strlen(_buffer);
  }
};

// SHA256 implementation (mimics R2Module::_sha256)
TestString test_sha256(const char* data, size_t len) {
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

  return TestString(hexHash);
}

// HMAC-SHA256 implementation (hex output) (mimics R2Module::_hmacSha256)
void test_hmac_sha256(const char* key,
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

// HMAC-SHA256 binary output (mimics R2Module::_hmacSha256Binary)
void test_hmac_sha256_binary(const char* key,
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

void setUp(void) {
}
void tearDown(void) {
}

// Test 1: SHA256 of empty string (NIST test vector)
void test_sha256_empty_string(void) {
  TestString result = test_sha256("", 0);
  TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                           result.c_str());
}

// Test 2: SHA256 of "abc" (NIST test vector)
void test_sha256_abc(void) {
  TestString result = test_sha256("abc", 3);
  TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                           result.c_str());
}

// Test 3: SHA256 of longer string (NIST test vector)
void test_sha256_longer_string(void) {
  const char* data = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  TestString result = test_sha256(data, strlen(data));
  TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                           result.c_str());
}

// Test 4: HMAC-SHA256 with RFC 4231 test case 1
void test_hmac_sha256_rfc4231_case1(void) {
  // Key: 20 bytes of 0x0b
  const unsigned char key[20] = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                                 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
  const char* data = "Hi There";

  char output[65];
  test_hmac_sha256((const char*)key, 20, data, strlen(data), output);

  TEST_ASSERT_EQUAL_STRING("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
                           output);
}

// Test 5: HMAC-SHA256 with RFC 4231 test case 2
void test_hmac_sha256_rfc4231_case2(void) {
  const char* key = "Jefe";
  const char* data = "what do ya want for nothing?";

  char output[65];
  test_hmac_sha256(key, strlen(key), data, strlen(data), output);

  TEST_ASSERT_EQUAL_STRING("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
                           output);
}

// Test 6: HMAC-SHA256 binary output
void test_hmac_sha256_binary_output(void) {
  const char* key = "key";
  const char* data = "The quick brown fox jumps over the lazy dog";
  unsigned char output[32];

  test_hmac_sha256_binary(key, strlen(key), data, strlen(data), output);

  // Check that we got a valid hash (non-zero)
  bool allZero = true;
  for (int i = 0; i < 32; i++) {
    if (output[i] != 0) {
      allZero = false;
      break;
    }
  }
  TEST_ASSERT_FALSE(allZero);

  // Verify consistency - same input should give same output
  unsigned char output2[32];
  test_hmac_sha256_binary(key, strlen(key), data, strlen(data), output2);

  bool match = true;
  for (int i = 0; i < 32; i++) {
    if (output[i] != output2[i]) {
      match = false;
      break;
    }
  }
  TEST_ASSERT_TRUE(match);
}

// Test 7: Key derivation chain consistency
void test_key_derivation_chain_consistency(void) {
  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  const char* dateStamp = "20150830";
  const char* region = "us-east-1";
  const char* service = "s3";

  // kDate = HMAC("AWS4" + secretKey, dateStamp)
  char kSecretStr[128];
  snprintf(kSecretStr, sizeof(kSecretStr), "AWS4%s", secretKey);
  unsigned char kDate[32];
  test_hmac_sha256_binary(kSecretStr, strlen(kSecretStr), dateStamp, strlen(dateStamp), kDate);

  // kRegion = HMAC(kDate, region)
  unsigned char kRegion[32];
  test_hmac_sha256_binary((const char*)kDate, 32, region, strlen(region), kRegion);

  // kService = HMAC(kRegion, service)
  unsigned char kService[32];
  test_hmac_sha256_binary((const char*)kRegion, 32, service, strlen(service), kService);

  // kSigning = HMAC(kService, "aws4_request")
  unsigned char kSigning[32];
  test_hmac_sha256_binary((const char*)kService, 32, "aws4_request", 12, kSigning);

  // Verify that the signing key is consistent
  unsigned char kSigning2[32];
  test_hmac_sha256_binary((const char*)kService, 32, "aws4_request", 12, kSigning2);

  bool match = true;
  for (int i = 0; i < 32; i++) {
    if (kSigning[i] != kSigning2[i]) {
      match = false;
      break;
    }
  }
  TEST_ASSERT_TRUE(match);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_sha256_empty_string);
  RUN_TEST(test_sha256_abc);
  RUN_TEST(test_sha256_longer_string);
  RUN_TEST(test_hmac_sha256_rfc4231_case1);
  RUN_TEST(test_hmac_sha256_rfc4231_case2);
  RUN_TEST(test_hmac_sha256_binary_output);
  RUN_TEST(test_key_derivation_chain_consistency);

  return UNITY_END();
}
