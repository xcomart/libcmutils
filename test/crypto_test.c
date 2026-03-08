#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include "functions.h"
#include "test.h"

CMUTIL_LogDefine("crypto_test")

int main() {
    OpenSSL_add_all_algorithms();

    EVP_CIPHER *cipher;

    cipher = CMUTIL_BlockCryptoGetCipher("AES", "CBC", "PKCS5Padding");
    if (cipher == NULL) return 1;
    if (strcasecmp(EVP_CIPHER_name(cipher), "AES-256-CBC") != 0) return 2;

    cipher = CMUTIL_BlockCryptoGetCipher("AES-128", "GCM", "NoPadding");
    if (cipher == NULL) return 3;
    if (!(strcasestr(EVP_CIPHER_name(cipher), "aes128-GCM") != NULL || strcasecmp(EVP_CIPHER_name(cipher), "aes-128-gcm") == 0)) return 4;

    cipher = CMUTIL_BlockCryptoGetCipher("DESede", "ECB", "PKCS5Padding");
    if (cipher == NULL) return 5;
    if (strcasecmp(EVP_CIPHER_name(cipher), "DES-EDE3") != 0) return 6;

    cipher = CMUTIL_BlockCryptoGetCipher("TripleDES", "CBC", "PKCS5Padding");
    if (cipher == NULL) return 7;
    if (strcasecmp(EVP_CIPHER_name(cipher), "DES-EDE3-CBC") != 0) return 8;

    cipher = CMUTIL_BlockCryptoGetCipher("AES", "CTR", "NoPadding");
    if (cipher == NULL) return 9;
    if (strcasecmp(EVP_CIPHER_name(cipher), "AES-256-CTR") != 0) return 10;

    cipher = CMUTIL_BlockCryptoGetCipher("AES-192", "CFB", "NoPadding");
    if (cipher == NULL) return 11;
    if (strcasecmp(EVP_CIPHER_name(cipher), "AES-192-CFB") != 0) return 12;

    cipher = CMUTIL_BlockCryptoGetCipher("DES", "ECB", "NoPadding");
    if (cipher == NULL) return 13;
    if (strcasecmp(EVP_CIPHER_name(cipher), "DES-ECB") != 0) return 14;

    printf("All crypto tests passed.\n");
    return 0;
}
