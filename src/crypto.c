
#include "functions.h"

#include <string.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/aes.h>


CMUTIL_LogDefine("cmutils.crypto")

typedef struct CMUTIL_BlockCrypto_Internal {
    CMUTIL_BlockCrypto  base;
    CMUTIL_Mem          *memst;
    EVP_CIPHER_CTX      *ctx;
} CMUTIL_BlockCrypto_Internal;


static EVP_CIPHER *CMUTIL_BlockCryptoGetCipher(
    const char *algo, const char *mode, int bits)
{
    char name[128];

    if (strncasecmp(algo, "AES", 3) == 0) {
        if (strcasecmp(mode, "CBC") == 0) {
            snprintf(name, sizeof(name), "aes-%d-cbc", bits);
        } else if (strcasecmp(mode, "GCM") == 0) {
            snprintf(name, sizeof(name), "aes-%d-gcm", bits);
        } else if (strcasecmp(mode, "ECB") == 0) {
            snprintf(name, sizeof(name), "aes-%d-ecb", bits);
        } else if (strcasecmp(mode, "CFB") == 0 || strcasecmp(mode, "CFB128") == 0) {
            snprintf(name, sizeof(name), "aes-%d-cfb", bits);
        } else if (strcasecmp(mode, "OFB") == 0) {
            snprintf(name, sizeof(name), "aes-%d-ofb", bits);
        } else if (strcasecmp(mode, "CTR") == 0) {
            snprintf(name, sizeof(name), "aes-%d-ctr", bits);
        } else {
            CMLogError("unsupported cipher mode: %s", mode);
            return NULL;
        }
    } else if (strcasecmp(algo, "DES") == 0) {
        if (strcasecmp(mode, "CBC") == 0) {
            strcpy(name, "des-cbc");
        } else if (strcasecmp(mode, "ECB") == 0) {
            strcpy(name, "des-ecb");
        } else if (strcasecmp(mode, "CFB") == 0) {
            strcpy(name, "des-cfb");
        } else if (strcasecmp(mode, "OFB") == 0) {
            strcpy(name, "des-ofb");
        } else {
            CMLogError("unsupported cipher mode: %s", mode);
            return NULL;
        }
    } else if (strcasecmp(algo, "DESede") == 0 || strcasecmp(algo, "TripleDES") == 0) {
        if (strcasecmp(mode, "CBC") == 0) {
            strcpy(name, "des-ede3-cbc");
        } else if (strcasecmp(mode, "ECB") == 0) {
            strcpy(name, "des-ede3-ecb");
        } else if (strcasecmp(mode, "CFB") == 0) {
            strcpy(name, "des-ede3-cfb");
        } else if (strcasecmp(mode, "OFB") == 0) {
            strcpy(name, "des-ede3-ofb");
        } else {
            CMLogError("unsupported cipher mode: %s", mode);
            return NULL;
        }
    } else {
        CMLogError("unsupported cipher algorithm: %s", algo);
        return NULL;
    }

    EVP_CIPHER *res = (EVP_CIPHER *)EVP_get_cipherbyname(name);
    if (!res) {
        CMLogError("EVP_get_cipherbyname() failed for %s", name);
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_BlockCryptoEncrypt(
    CMUTIL_BlockCrypto *bc, CMUTIL_String *plain)
{
    CMUTIL_BlockCrypto_Internal *ibc = (CMUTIL_BlockCrypto_Internal*)bc;
    if (!ibc || !plain) {
        CMLogError("CMUTIL_BlockCryptoEncrypt() invalid arguments");
        return NULL;
    }
    int olen = CMCall(plain, GetSize)+32;
    CMUTIL_String *res = CMUTIL_StringCreateEx(olen, NULL);
    int rc = EVP_EncryptUpdate(ibc->ctx, (uint8_t*)CMCall(res, GetCString),
        &olen, (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));

    return NULL;
}

static CMUTIL_BlockCrypto g_cmutil_block_crypto = {
    CMUTIL_BlockCryptoEncrypt,
};

CMUTIL_BlockCrypto *CMUTIL_BlockCryptoCreateInternal(
        CMUTIL_Mem *memst, const char *algo, const char *mode,
        const char *padding, int key_bits,
        const uint8_t *key, const uint8_t *iv)
{
    CMUTIL_BlockCrypto_Internal *bc =
        memst->Alloc(sizeof(CMUTIL_BlockCrypto_Internal));
    if (!bc) {
        CMLogError("Failed to allocate memory for block crypto.");
        return NULL;
    }
    memset(bc, 0x0, sizeof(CMUTIL_BlockCrypto_Internal));
    memcpy(&bc->base, &g_cmutil_block_crypto, sizeof(CMUTIL_BlockCrypto));

    bc->ctx = EVP_CIPHER_CTX_new();
    if (!bc->ctx) {
        CMLogError("EVP_CIPHER_CTX_new() failed");
        goto FAILED;
    }
    EVP_CIPHER *cipher = CMUTIL_BlockCryptoGetCipher(algo, mode, key_bits);
    if (!cipher) {
        CMLogError("CMUTIL_BlockCryptoGetCipher() failed");
        goto FAILED;
    }
    EVP_EncryptInit_ex(bc->ctx, cipher, NULL, key, iv);
    if (!bc->ctx) {
        CMLogError("EVP_EncryptInit_ex() failed");
        goto FAILED;
    }

    // do not use block padding in GCM
    if (padding && !strcasecmp(mode, "GCM")) {
        EVP_CIPHER_CTX_set_padding(bc->ctx, 1);
    }

    return (CMUTIL_BlockCrypto*)bc;
FAILED:
    CMCall(&bc->base, Destroy);
    return NULL;
}


CMUTIL_PublicKey *CMUTIL_PublicKeyCreateInternal(
        CMUTIL_Mem *memst, const char *data, CMBool is_file)
{
    return NULL;
}

CMUTIL_PrivateKey *CMUTIL_PrivateKeyCreateInternal(
        CMUTIL_Mem *memst, const char *data, CMBool is_file)
{
    return NULL;
}

CMUTIL_RSACrypto *CMUTIL_RSACryptoCreateInternal(CMUTIL_Mem *memst)
{
    return NULL;
}

CMUTIL_RSACrypto *CMUTIL_RSACryptoCreate(void) {
    return CMUTIL_RSACryptoCreateInternal(CMUTIL_GetMem());
}

void CMUTIL_CryptoRandom(uint8_t *buf, size_t len)
{
    RAND_bytes(buf, len);
}