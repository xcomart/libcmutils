
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
    EVP_CIPHER          *cipher;
    char                algo[32];
    char                mode[32];
    char                padding[32];
    int                 key_bits;
} CMUTIL_BlockCrypto_Internal;

#define CHECK_ERR(x) do { if ((x) <= 0) { \
    CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL)); \
    goto FAILED; \
} } while(0)

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
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMLogError("EVP_get_cipherbyname() failed for %s", name);
    }
    return res;
}

CMUTIL_STATIC CMBool CMUTIL_BlockCryptoInit(
    CMUTIL_BlockCrypto *bc, const uint8_t *key, const uint8_t *iv,
    CMBool is_encrypt)
{
    CMUTIL_BlockCrypto_Internal *ibc = (CMUTIL_BlockCrypto_Internal*)bc;
    if (is_encrypt)
        CHECK_ERR(EVP_EncryptInit_ex(ibc->ctx, ibc->cipher, NULL, key, iv));
    else
        CHECK_ERR(EVP_DecryptInit_ex(ibc->ctx, ibc->cipher, NULL, key, iv));

    // do not use block padding in GCM
    if (strcasecmp(ibc->padding, "NoPadding") != 0 &&
            !strcasecmp(ibc->mode, "GCM")) {
        CHECK_ERR(EVP_CIPHER_CTX_set_padding(ibc->ctx, 1));
    }
    return CMTrue;
FAILED:
    CMLogError("CMUTIL_BlockCryptoInit() failed");
    return CMFalse;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_BlockCryptoEncrypt(
    CMUTIL_BlockCrypto *bc, CMUTIL_String *plain,
    const uint8_t *key, const uint8_t *iv)
{
    CMUTIL_BlockCrypto_Internal *ibc = (CMUTIL_BlockCrypto_Internal*)bc;
    if (!ibc || !plain) {
        CMLogError("CMUTIL_BlockCryptoEncrypt() invalid arguments");
        return NULL;
    }
    int olen = 0, total = 0;
    CMUTIL_String *res = CMUTIL_StringCreateInternal(
        ibc->memst, CMCall(plain, GetSize)+(ibc->key_bits/8), NULL);

    if (!CMUTIL_BlockCryptoInit(bc, key, iv, CMTrue)) {
        CMLogError("CMUTIL_BlockCryptoInit() failed");
        goto FAILED;
    }

    int rc = EVP_EncryptUpdate(ibc->ctx, (uint8_t*)CMCall(res, GetCString),
        &olen, (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));
    CHECK_ERR(rc);
    total = olen;

    rc = EVP_EncryptFinal_ex(ibc->ctx,
        (uint8_t*)CMCall(res, GetCString)+total, &olen);
    CHECK_ERR(rc);
    total += olen;
    CMUTIL_StringSetSizeInternal(res, total);

    return res;
FAILED:
    if (res) CMCall(res, Destroy);
    return NULL;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_BlockCryptoDecrypt(
    CMUTIL_BlockCrypto *bc, CMUTIL_String *plain,
    const uint8_t *key, const uint8_t *iv)
{
    CMUTIL_BlockCrypto_Internal *ibc = (CMUTIL_BlockCrypto_Internal*)bc;
    if (!ibc || !plain) {
        CMLogError("CMUTIL_BlockCryptoEncrypt() invalid arguments");
        return NULL;
    }
    int olen = 0, total = 0;
    CMUTIL_String *res = CMUTIL_StringCreateInternal(
        ibc->memst, CMCall(plain, GetSize)+(ibc->key_bits/8), NULL);

    if (!CMUTIL_BlockCryptoInit(bc, key, iv, CMFalse)) {
        CMLogError("CMUTIL_BlockCryptoInit() failed");
        goto FAILED;
    }

    int rc = EVP_DecryptUpdate(ibc->ctx, (uint8_t*)CMCall(res, GetCString),
        &olen, (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));
    CHECK_ERR(rc);
    total = olen;

    rc = EVP_DecryptFinal_ex(ibc->ctx,
        (uint8_t*)CMCall(res, GetCString)+total, &olen);
    CHECK_ERR(rc);
    total += olen;
    CMUTIL_StringSetSizeInternal(res, total);

    return res;
    FAILED:
        if (res) CMCall(res, Destroy);
    return NULL;
}

CMUTIL_STATIC void CMUTIL_BlockCryptoDestroy(CMUTIL_BlockCrypto *bc)
{
    CMUTIL_BlockCrypto_Internal *ibc = (CMUTIL_BlockCrypto_Internal*)bc;
    EVP_CIPHER_CTX_free(ibc->ctx);
    ibc->memst->Free(ibc);
}

static CMUTIL_BlockCrypto g_cmutil_block_crypto = {
    CMUTIL_BlockCryptoEncrypt,
    CMUTIL_BlockCryptoDecrypt,
    CMUTIL_BlockCryptoDestroy
};

CMUTIL_BlockCrypto *CMUTIL_BlockCryptoCreateInternal(
        CMUTIL_Mem *memst, const char *algo, const char *mode,
        const char *padding, int key_bits)
{
    CMUTIL_BlockCrypto_Internal *bc =
        memst->Alloc(sizeof(CMUTIL_BlockCrypto_Internal));
    if (!bc) {
        CMLogError("Failed to allocate memory for block crypto.");
        return NULL;
    }
    memset(bc, 0x0, sizeof(CMUTIL_BlockCrypto_Internal));
    bc->memst = memst;
    bc->key_bits = key_bits;
    strncpy(bc->algo, algo, sizeof(bc->algo));
    strncpy(bc->mode, mode, sizeof(bc->mode));
    strncpy(bc->padding, padding, sizeof(bc->padding));
    memcpy(&bc->base, &g_cmutil_block_crypto, sizeof(CMUTIL_BlockCrypto));

    bc->ctx = EVP_CIPHER_CTX_new();
    if (!bc->ctx) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMLogError("EVP_CIPHER_CTX_new() failed");
        goto FAILED;
    }
    bc->cipher = CMUTIL_BlockCryptoGetCipher(algo, mode, key_bits);
    if (!bc->cipher) {
        CMLogError("CMUTIL_BlockCryptoGetCipher() failed");
        goto FAILED;
    }

    return (CMUTIL_BlockCrypto*)bc;
FAILED:
    CMCall(&bc->base, Destroy);
    return NULL;
}

CMUTIL_BlockCrypto *CMUTIL_BlockCryptoCreate(
        const char *algo, const char *mode, const char *padding,
        int key_bits)
{
    return CMUTIL_BlockCryptoCreateInternal(
        CMUTIL_GetMem(), algo, mode, padding, key_bits);
}


typedef struct CMUTIL_RSAKey_Internal {
    CMUTIL_RSAKey  base;
    CMUTIL_Mem     *memst;
    CMBool         is_private;
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_PKEY       *key;
#else
    RSA            *key;
#endif
} CMUTIL_RSAKey_Internal;


CMUTIL_STATIC CMUTIL_String *CMUTIL_RSAKeyGetEncoded(CMUTIL_RSAKey *key) {
    CMUTIL_RSAKey_Internal *ik = (CMUTIL_RSAKey_Internal*)key;
    if (!ik) {
        CMLogError("CMUTIL_RSAKeyGetEncoded() invalid parameter");
        return NULL;
    }
    if (ik->is_private) {
        CMLogError("CMUTIL_RSAKeyGetEncoded() private key not supported");
        return NULL;
    }
    CMUTIL_String *str = NULL;
    int len = 0;
#if defined(CMUTIL_RSA_USE_EVP)
    len = i2d_PUBKEY(ik->key, NULL);
    if (len <= 0) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }
    str = CMUTIL_StringCreateInternal(ik->memst, len, NULL);
    uint8_t *buf = (uint8_t*)CMCall(str, GetCString);
    int rc = i2d_PUBKEY(ik->key, &buf);
    if (rc <= 0) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMCall(str, Destroy);
        return NULL;
    }
#else
    len = i2d_RSA_PUBKEY(ik->key, NULL);
    if (len <= 0) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }
    str = CMUTIL_StringCreateInternal(ik->memst, len, NULL);
    uint8_t *buf = (uint8_t*)CMCall(str, GetCString);
    int rc = i2d_RSA_PUBKEY(ik->key, &buf);
    if (rc <= 0) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMCall(str, Destroy);
        return NULL;
    }
#endif
    CMUTIL_StringSetSizeInternal(str, len);
    return str;
}

CMUTIL_STATIC void CMUTIL_RSAKeyDestroy(CMUTIL_RSAKey *key) {
    CMUTIL_RSAKey_Internal *ik = (CMUTIL_RSAKey_Internal*)key;
    if (ik) {
        if (ik->key) {
#if defined(CMUTIL_RSA_USE_EVP)
            EVP_PKEY_free(ik->key);
#else
            RSA_free(ik->key);
#endif
        }
        ik->memst->Free(ik);
    }
}

static CMUTIL_RSAKey g_cmutil_rsa_key = {
    CMUTIL_RSAKeyGetEncoded,
    CMUTIL_RSAKeyDestroy
};

CMUTIL_PublicKey *CMUTIL_PublicKeyCreateInternal(
        CMUTIL_Mem *memst, const char *data, CMBool is_file)
{
    if (data == NULL) {
        CMLogError("CMUTIL_PublicKeyCreateInternal() invalid parameter");
        return NULL;
    }
    CMUTIL_RSAKey_Internal *ik = memst->Alloc(sizeof(CMUTIL_RSAKey_Internal));
    if (!ik) {
        CMLogError("Failed to allocate memory for RSA key.");
        return NULL;
    }
    memset(ik, 0x0, sizeof(CMUTIL_RSAKey_Internal));
    ik->base = g_cmutil_rsa_key;
    ik->is_private = CMFalse;
    ik->memst = memst;
    CMUTIL_String *str = NULL;
    if (is_file) {
        CMUTIL_File *f = CMUTIL_FileCreateInternal(ik->memst, data);
        if (!f) {
            CMLogError("Failed to create file object for RSA public key.");
            goto FAILED;
        }
        str = CMCall(f, GetContents);
        CMCall(f, Destroy);
        if (!str) {
            CMLogError("Failed to read contents from file for RSA public key.");
            goto FAILED;
        }
    } else {
        str = CMUTIL_StringCreateInternal(ik->memst, strlen(data), data);
    }
    const char *buf = CMCall(str, GetCString);
    if (strncmp(buf, "-----BEGIN PUBLIC KEY-----", 25) != 0) {
        CMCall(str, InsertString, "-----BEGIN PUBLIC KEY-----\n", 0);
        CMCall(str, AddString, "\n-----END PUBLIC KEY-----");
    }
    BIO *bio = BIO_new_mem_buf((void*)CMCall(str, GetCString), -1);
    if (!bio) {
        CMLogError("Failed to create BIO for memory buffer.");
        goto FAILED;
    }
#if defined(CMUTIL_RSA_USE_EVP)
    ik->key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
#else
    ik->key = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
#endif
    BIO_free(bio);
    if (!ik->key) {
        CMLogError("PEM_read_PUBKEY() failed");
        goto FAILED;
    }
    if (str) CMCall(str, Destroy);
    return (CMUTIL_PublicKey*)ik;
FAILED:
    if (str) CMCall(str, Destroy);
    return NULL;
}

CMUTIL_PrivateKey *CMUTIL_PrivateKeyCreateFromPEM(
        const char *pem, const uint8_t *passphrase) {
    return CMUTIL_PrivateKeyCreateInternal(
        CMUTIL_GetMem(), pem, passphrase, CMFalse);
}

CMUTIL_PublicKey *CMUTIL_PublicKeyCreateFromPEM(
        const char *pem) {
    return CMUTIL_PublicKeyCreateInternal(
        CMUTIL_GetMem(), pem, CMFalse);
}

CMUTIL_PrivateKey *CMUTIL_PrivateKeyCreateFromFile(
        const char *file_path, const uint8_t *passphrase) {
    return CMUTIL_PrivateKeyCreateInternal(
        CMUTIL_GetMem(), file_path, passphrase, CMTrue);
}

CMUTIL_PublicKey *CMUTIL_PublicKeyCreateFromFile(
        const char *file_path) {
    return CMUTIL_PublicKeyCreateInternal(
        CMUTIL_GetMem(), file_path, CMTrue);
}



static int CMUTIL_RSAPriPwdCB(char *buf, int size, int rwflag, void *userdata) {
    CMUTIL_String *password = (CMUTIL_String *)userdata;
    int len = CMCall(password, GetSize);

    (void)rwflag;

    if (len > size) {
        len = size;
    }

    memcpy(buf, CMCall(password, GetCString), (size_t)len);
    return len;
}

CMUTIL_PrivateKey *CMUTIL_PrivateKeyCreateInternal(
        CMUTIL_Mem *memst, const char *data,
        const uint8_t *passphrase, CMBool is_file)
{
    if (data == NULL) {
        CMLogError("CMUTIL_PrivateKeyCreateInternal() invalid parameter");
        return NULL;
    }
    CMUTIL_RSAKey_Internal *ik = memst->Alloc(sizeof(CMUTIL_RSAKey_Internal));
    if (!ik) {
        CMLogError("Failed to allocate memory for RSA key.");
        return NULL;
    }
    memset(ik, 0x0, sizeof(CMUTIL_RSAKey_Internal));
    ik->base = g_cmutil_rsa_key;
    ik->is_private = CMTrue;
    ik->memst = memst;
    CMUTIL_String *pwdstr = CMUTIL_StringCreateInternal(ik->memst,
        strlen((const char*)passphrase), (const char*)passphrase);
    CMUTIL_String *str = NULL;
    if (is_file) {
        CMUTIL_File *f = CMUTIL_FileCreateInternal(ik->memst, data);
        if (!f) {
            CMLogError("Failed to create file object for RSA public key.");
            goto FAILED;
        }
        str = CMCall(f, GetContents);
        CMCall(f, Destroy);
        if (!str) {
            CMLogError("Failed to read contents from file for RSA public key.");
            goto FAILED;
        }
    } else {
        str = CMUTIL_StringCreateInternal(ik->memst, strlen(data), data);
    }
    const char *buf = CMCall(str, GetCString);
    BIO *bio = BIO_new_mem_buf((void*)CMCall(str, GetCString), -1);
    if (!bio) {
        CMLogError("Failed to create BIO for memory buffer.");
        goto FAILED;
    }
#if defined(CMUTIL_RSA_USE_EVP)
    ik->key = PEM_read_bio_PrivateKey(bio, NULL, CMUTIL_RSAPriPwdCB, pwdstr);
#else
    ik->key = PEM_read_bio_RSAPrivateKey(bio, NULL, CMUTIL_RSAPriPwdCB, pwdstr);
#endif
    BIO_free(bio);
    if (!ik->key) {
        CMLogError("PEM_read_PUBKEY() failed");
        goto FAILED;
    }
    if (pwdstr) CMCall(pwdstr, Destroy);
    if (str) CMCall(str, Destroy);
    return (CMUTIL_PublicKey*)ik;
FAILED:
    if (pwdstr) CMCall(pwdstr, Destroy);
    if (str) CMCall(str, Destroy);
    return NULL;
}

typedef struct CMUTIL_RSACrypto_Internal {
    CMUTIL_RSACrypto    base;
    CMUTIL_Mem          *memst;
} CMUTIL_RSACrypto_Internal;


static CMUTIL_String *CMUTIL_RSACryptoEncrypt(
    CMUTIL_RSACrypto *crypto,
    CMUTIL_String *plain,
    CMUTIL_RSAKey *key,
    CMBool key_is_public)
{
    CMUTIL_RSACrypto_Internal *ic = (CMUTIL_RSACrypto_Internal*)crypto;
    CMUTIL_RSAKey_Internal *ik = (CMUTIL_RSAKey_Internal*)key;
    if (!ic || !plain || !key) {
        CMLogError("CMUTIL_RSACryptoEncrypt() invalid arguments");
        return NULL;
    }
    CMUTIL_String *res = NULL;
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(ik->key, NULL);
    if (!ctx) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMLogError("EVP_PKEY_CTX_new() failed");
        goto FAILED;
    }
    CHECK_ERR(EVP_PKEY_encrypt_init(ctx));
    int rc = EVP_PKEY_CTX_set_rsa_padding(
        ctx, RSA_PKCS1_OAEP_PADDING);
    CHECK_ERR(rc);
    size_t olen = 0;
    rc = EVP_PKEY_encrypt(ctx, NULL, &olen,
        (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));
    CHECK_ERR(rc);
    res = CMUTIL_StringCreateInternal(ik->memst, olen, NULL);
    rc = EVP_PKEY_encrypt(ctx, (uint8_t*)CMCall(res, GetCString), &olen,
        (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));
    CHECK_ERR(rc);
    CMUTIL_StringSetSizeInternal(res, olen);
    EVP_PKEY_CTX_free(ctx);
#else
    size_t blen = RSA_size(ik->key);
    size_t ilen = CMCall(plain, GetSize);
    size_t modlen = ilen % blen;
    size_t olen = (ilen / blen) * blen;
    if (modlen) {
        olen = ilen + blen - modlen;
    }
    res = CMUTIL_StringCreateInternal(ik->memst, olen, NULL);
    if (key_is_public) {
        olen = RSA_public_encrypt(
            CMCall(plain, GetSize), (uint8_t*)CMCall(plain, GetCString),
            (uint8_t*)CMCall(res, GetCString), ik->key, RSA_PKCS1_OAEP_PADDING);
        CHECK_ERR(olen);
    } else {
        olen = RSA_private_encrypt(
            CMCall(plain, GetSize), (uint8_t*)CMCall(plain, GetCString),
            (uint8_t*)CMCall(res, GetCString), ik->key, RSA_PKCS1_OAEP_PADDING);
        CHECK_ERR(olen);
    }
    CMUTIL_StringSetSizeInternal(res, olen);
#endif
    return res;
    FAILED:
        if (res) CMCall(res, Destroy);
#if defined(CMUTIL_RSA_USE_EVP)
    if (ctx) EVP_PKEY_CTX_free(ctx);
#endif
    return NULL;
}

static CMUTIL_String *CMUTIL_RSACryptoDecrypt(
    CMUTIL_RSACrypto *crypto,
    CMUTIL_String *encrypted,
    CMUTIL_RSAKey *key,
    CMBool key_is_public)
{
    CMUTIL_RSACrypto_Internal *ic = (CMUTIL_RSACrypto_Internal*)crypto;
    CMUTIL_RSAKey_Internal *ik = (CMUTIL_RSAKey_Internal*)key;
    if (!ic || !encrypted || !key) {
        CMLogError("CMUTIL_RSACryptoEncrypt() invalid arguments");
        return NULL;
    }
    CMUTIL_String *res = NULL;
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(ik->key, NULL);
    if (!ctx) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMLogError("EVP_PKEY_CTX_new() failed");
        goto FAILED;
    }
    CHECK_ERR(EVP_PKEY_decrypt_init(ctx));
    int rc = EVP_PKEY_CTX_set_rsa_padding(
        ctx, RSA_PKCS1_OAEP_PADDING);
    CHECK_ERR(rc);
    size_t olen = 0;
    rc = EVP_PKEY_decrypt(ctx, NULL, &olen,
        (uint8_t*)CMCall(encrypted, GetCString), CMCall(encrypted, GetSize));
    CHECK_ERR(rc);
    res = CMUTIL_StringCreateInternal(ik->memst, olen, NULL);
    rc = EVP_PKEY_decrypt(ctx, (uint8_t*)CMCall(res, GetCString), &olen,
        (uint8_t*)CMCall(encrypted, GetCString), CMCall(encrypted, GetSize));
    CHECK_ERR(rc);
    CMUTIL_StringSetSizeInternal(res, olen);
    EVP_PKEY_CTX_free(ctx);
#else
    size_t blen = RSA_size(ik->key);
    size_t ilen = CMCall(encrypted, GetSize);
    size_t modlen = ilen % blen;
    size_t olen = (ilen / blen) * blen;
    if (modlen) {
        olen = ilen + blen - modlen;
    }
    res = CMUTIL_StringCreateInternal(ik->memst, olen, NULL);
    if (key_is_public) {
        olen = RSA_public_decrypt(
            CMCall(encrypted, GetSize), (uint8_t*)CMCall(encrypted, GetCString),
            (uint8_t*)CMCall(res, GetCString), ik->key, RSA_PKCS1_OAEP_PADDING);
        CHECK_ERR(olen);
    } else {
        olen = RSA_private_decrypt(
            CMCall(encrypted, GetSize), (uint8_t*)CMCall(encrypted, GetCString),
            (uint8_t*)CMCall(res, GetCString), ik->key, RSA_PKCS1_OAEP_PADDING);
        CHECK_ERR(olen);
    }
    CMUTIL_StringSetSizeInternal(res, olen);
#endif
    return res;
    FAILED:
        if (res) CMCall(res, Destroy);
#if defined(CMUTIL_RSA_USE_EVP)
    if (ctx) EVP_PKEY_CTX_free(ctx);
#endif
    return NULL;
}


CMUTIL_STATIC CMUTIL_String *CMUTIL_RSACryptoEncryptWithPublicKey(
        CMUTIL_RSACrypto *crypto,
        CMUTIL_String *plain, CMUTIL_PublicKey *pub_key)
{
    CMUTIL_String *res = CMUTIL_RSACryptoEncrypt(
        crypto, plain, pub_key, CMTrue);
    if (!res) {
        CMLogError("CMUTIL_RSACryptoEncrypt() failed");
        return NULL;
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_RSACryptoEncryptWithPrivateKey(
        CMUTIL_RSACrypto *crypto,
        CMUTIL_String *plain, CMUTIL_PrivateKey *pri_key)
{
    CMUTIL_String *res = CMUTIL_RSACryptoEncrypt(
        crypto, plain, pri_key, CMFalse);
    if (!res) {
        CMLogError("CMUTIL_RSACryptoEncrypt() failed");
        return NULL;
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_RSACryptoDecryptWithPublicKey(
        CMUTIL_RSACrypto *crypto,
        CMUTIL_String *plain, CMUTIL_PublicKey *pub_key)
{
    CMUTIL_String *res = CMUTIL_RSACryptoDecrypt(
        crypto, plain, pub_key, CMTrue);
    if (!res) {
        CMLogError("CMUTIL_RSACryptoDecrypt() failed");
        return NULL;
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_RSACryptoDecryptWithPrivateKey(
        CMUTIL_RSACrypto *crypto,
        CMUTIL_String *plain, CMUTIL_PrivateKey *pri_key)
{
    CMUTIL_String *res = CMUTIL_RSACryptoDecrypt(
        crypto, plain, pri_key, CMFalse);
    if (!res) {
        CMLogError("CMUTIL_RSACryptoDecrypt() failed");
        return NULL;
    }
    return res;
}

CMUTIL_STATIC CMUTIL_String *CMUTIL_RSACryptoSign(
        CMUTIL_RSACrypto *crypto,
        CMUTIL_String *plain, CMUTIL_PrivateKey *pri_key)
{
    CMUTIL_RSACrypto_Internal *ic = (CMUTIL_RSACrypto_Internal*)crypto;
    CMUTIL_RSAKey_Internal *ik = (CMUTIL_RSAKey_Internal*)pri_key;

    CMUTIL_String *res = NULL;
    int rc = 0;
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMLogError("EVP_MD_CTX_new() failed");
        return NULL;
    }
    rc = EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, ik->key);
    CHECK_ERR(rc);
    rc = EVP_DigestSignUpdate(ctx,
        (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));
    CHECK_ERR(rc);
    size_t olen = 0;

    rc = EVP_DigestSignFinal(ctx, NULL, &olen);
    CHECK_ERR(rc);
    res = CMUTIL_StringCreateInternal(ik->memst, olen, NULL);
    rc = EVP_DigestSignFinal(ctx, (uint8_t*)CMCall(res, GetCString), &olen);
    CHECK_ERR(rc);
    CMUTIL_StringSetSizeInternal(res, olen);

    EVP_MD_CTX_free(ctx);
#else
    uint32_t siglen = (uint32_t)RSA_size(ik->key);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (!SHA256((uint8_t*)CMCall(plain, GetCString),
        CMCall(plain, GetSize), digest)) {
        CMLogError("SHA256() failed");
        goto FAILED;
    }
    res = CMUTIL_StringCreateInternal(ik->memst, siglen, NULL);
    rc = RSA_sign(NID_sha256, digest, SHA256_DIGEST_LENGTH,
        (uint8_t*)CMCall(res, GetCString), &siglen, ik->key);
    CHECK_ERR(rc);
    CMUTIL_StringSetSizeInternal(res, siglen);
#endif
    return res;
FAILED:
    if (res) CMCall(res, Destroy);
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_MD_CTX_free(ctx);
#endif
    return NULL;
}

CMUTIL_STATIC CMBool CMUTIL_RSACryptoVerifySignature(
        CMUTIL_RSACrypto *crypto, CMUTIL_String *plain,
        CMUTIL_String *signature, CMUTIL_PublicKey *pub_key)
{
    CMUTIL_RSACrypto_Internal *ic = (CMUTIL_RSACrypto_Internal*)crypto;
    CMUTIL_RSAKey_Internal *ik = (CMUTIL_RSAKey_Internal*)pub_key;

    int rc = 0;
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        CMLogError("OpenSSL error: %s", ERR_error_string(ERR_get_error(), NULL));
        CMLogError("EVP_MD_CTX_new() failed");
        return CMFalse;
    }
    rc = EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, ik->key);
    CHECK_ERR(rc);
    rc = EVP_DigestVerifyUpdate(ctx,
        (uint8_t*)CMCall(plain, GetCString), CMCall(plain, GetSize));
    CHECK_ERR(rc);
    rc = EVP_DigestVerifyFinal(ctx,
        (uint8_t*)CMCall(signature, GetCString), CMCall(signature, GetSize));
    CHECK_ERR(rc);
    EVP_MD_CTX_free(ctx);
#else
    unsigned char digest[SHA256_DIGEST_LENGTH];
    if (!SHA256((uint8_t*)CMCall(plain, GetCString),
        CMCall(plain, GetSize), digest)) {
        CMLogError("SHA256() failed");
        goto FAILED;
        }
    rc = RSA_verify(NID_sha256, digest, SHA256_DIGEST_LENGTH,
        (uint8_t*)CMCall(signature, GetCString), CMCall(signature, GetSize),
        ik->key);
    CHECK_ERR(rc);
#endif
    return CMTrue;
FAILED:
#if defined(CMUTIL_RSA_USE_EVP)
    EVP_MD_CTX_free(ctx);
#endif
    return CMFalse;
}

CMUTIL_STATIC void CMUTIL_RSACryptoDestroy(CMUTIL_RSACrypto *crypto) {
    if (!crypto) return;
    CMUTIL_RSACrypto_Internal *ic = (CMUTIL_RSACrypto_Internal*)crypto;
    if (ic->memst) {
        ic->memst->Free(ic);
    }
}

static CMUTIL_RSACrypto g_cmutil_rsa_crypto = {
    CMUTIL_RSACryptoEncryptWithPublicKey,
    CMUTIL_RSACryptoEncryptWithPrivateKey,
    CMUTIL_RSACryptoDecryptWithPublicKey,
    CMUTIL_RSACryptoDecryptWithPrivateKey,
    CMUTIL_RSACryptoSign,
    CMUTIL_RSACryptoVerifySignature,
    CMUTIL_RSACryptoDestroy
};

CMUTIL_RSACrypto *CMUTIL_RSACryptoCreateInternal(CMUTIL_Mem *memst)
{
    if (!memst) {
        CMLogError("Memory allocator is NULL.");
        return NULL;
    }
    CMUTIL_RSACrypto_Internal *ic = memst->Alloc(
        sizeof(CMUTIL_RSACrypto_Internal));
    if (!ic) {
        CMLogError("Failed to allocate memory for RSA crypto.");
        return NULL;
    }
    memset(ic, 0x0, sizeof(CMUTIL_RSACrypto_Internal));
    ic->base = g_cmutil_rsa_crypto;
    ic->memst = memst;
    return (CMUTIL_RSACrypto*)ic;
}

CMUTIL_RSACrypto *CMUTIL_RSACryptoCreate(void) {
    return CMUTIL_RSACryptoCreateInternal(CMUTIL_GetMem());
}

void CMUTIL_CryptoRandom(uint8_t *buf, size_t len)
{
    RAND_bytes(buf, (int)len);
}