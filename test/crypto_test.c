#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include "functions.h"
#include "test.h"

CMUTIL_LogDefine("test.crypto")

static CMBool prepare_openssl_env(CMUTIL_Map **out_env, CMUTIL_String **out_pathstr) {
    CMUTIL_Map *env = NULL;
    CMUTIL_String *pathstr = NULL;
    const char *path = getenv("PATH");

    env = CMUTIL_MapCreateEx(
        CMUTIL_MAP_DEFAULT, CMFalse, CMFree, 0.75f);
    if (!env) {
        return CMFalse;
    }

    pathstr = CMUTIL_StringCreateEx(0, path ? path : "");
    if (!pathstr) {
        CMCall(env, Destroy);
        return CMFalse;
    }

#if defined(MSWIN)
    CMCall(pathstr, AddString, ";/opt/homebrew/bin");
#else
    CMCall(pathstr, AddString, ":/opt/homebrew/bin");
#endif

    path = CMCall(pathstr, GetCString);
    CMCall(env, Put, "PATH", CMStrdup(path), NULL);

    *out_env = env;
    *out_pathstr = pathstr;
    return CMTrue;
}

static CMBool run_process_wait_ok(
        const char *cwd,
        CMUTIL_Map *env,
        const char *arg1,
        const char *arg2,
        const char *arg3,
        const char *arg4,
        const char *arg5,
        const char *arg6,
        const char *arg7)
{
    CMBool ok = CMFalse;
    CMUTIL_Process *proc = NULL;

    proc = CMUTIL_ProcessCreate(
        cwd, env, "openssl", arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    if (!proc) {
        CMLogError("CMUTIL_ProcessCreate failed");
        goto END_POINT;
    }

    if (!CMCall(proc, Start, CMProcStreamNone)) {
        CMLogError("process start failed");
        goto END_POINT;
    }

    if (CMCall(proc, Wait, 10000) != 0) {
        CMLogError("process wait failed");
        goto END_POINT;
    }

    ok = CMTrue;

END_POINT:
    if (proc) CMCall(proc, Destroy);
    return ok;
}

int main() {
    int ir = -1;
    OpenSSL_add_all_algorithms();
    CMUTIL_Init(CMUTIL_MEM_TYPE);

    EVP_CIPHER *cipher;

    CMUTIL_BlockCrypto *block = NULL;
    CMUTIL_Map *env = NULL;
    CMUTIL_String *pathstr = NULL;
    CMUTIL_PrivateKey *priv = NULL;
    CMUTIL_PublicKey *pub = NULL;
    CMUTIL_RSACrypto *rsa = NULL;
    CMUTIL_String *plain = NULL;
    CMUTIL_String *encrypted = NULL;
    CMUTIL_String *decrypted = NULL;
    CMUTIL_String *signature = NULL;
    CMUTIL_String *pub_der = NULL;
    CMUTIL_File *priv_file = NULL;
    CMUTIL_File *pub_file = NULL;
    CMUTIL_String *b64 = NULL;
    CMUTIL_String *b64_decoded = NULL;

    uint8_t aes_key[32] = {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50
    };
    uint8_t aes_iv[16] = {
        0x61, 0x62, 0x63, 0x64,
        0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x6b, 0x6c,
        0x6d, 0x6e, 0x6f, 0x70
    };
    uint8_t rnd1[32] = {0,};
    uint8_t rnd2[32] = {0,};

    block = CMUTIL_BlockCryptoCreate("AES", "CBC", "PKCS5Padding", 256);
    ASSERT(block != NULL, "CMUTIL_BlockCryptoCreate AES/CBC/PKCS5Padding");

    plain = CMUTIL_StringCreateEx(0, "hello block crypto");
    ASSERT(plain != NULL, "create block plain text");

    encrypted = CMCall(block, Encrypt, plain, aes_key, aes_iv);
    ASSERT(encrypted != NULL, "CMUTIL_BlockCrypto.Encrypt");
    ASSERT(CMCall(encrypted, GetSize) > 0, "block encrypted size");

    decrypted = CMCall(block, Decrypt, encrypted, aes_key, aes_iv);
    ASSERT(decrypted != NULL, "CMUTIL_BlockCrypto.Decrypt");
    ASSERT(CMCall(decrypted, GetSize) == CMCall(plain, GetSize),
        "block decrypted size matches plain");
    ASSERT(memcmp(
        CMCall(decrypted, GetCString),
        CMCall(plain, GetCString),
        CMCall(plain, GetSize)) == 0,
        "block encrypt/decrypt round trip");

    CMCall(plain, Destroy); plain = NULL;
    CMCall(encrypted, Destroy); encrypted = NULL;
    CMCall(decrypted, Destroy); decrypted = NULL;
    CMCall(block, Destroy); block = NULL;

    // ASSERT(prepare_openssl_env(&env, &pathstr), "prepare openssl env");
    //
    // ASSERT(run_process_wait_ok(
    //     ".", env,
    //     "genpkey", "-algorithm", "RSA", "-out", "test_rsa_private.pem",
    //     "-pkeyopt", "rsa_keygen_bits:2048"),
    //     "generate RSA private key");
    //
    // ASSERT(run_process_wait_ok(
    //     ".", env,
    //     "pkey", "-in", "test_rsa_private.pem", "-pubout",
    //     "-out", "test_rsa_public.pem", NULL),
    //     "extract RSA public key");
    //
    // priv_file = CMUTIL_FileCreate("test_rsa_private.pem");
    // pub_file = CMUTIL_FileCreate("test_rsa_public.pem");
    // ASSERT(priv_file != NULL, "create private key file object");
    // ASSERT(pub_file != NULL, "create public key file object");
    // ASSERT(CMCall(priv_file, IsExists), "private key file exists");
    // ASSERT(CMCall(pub_file, IsExists), "public key file exists");

    const char *priv_pem = "-----BEGIN PRIVATE KEY-----\n"
        "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDO62tdNgxXPtM7\n"
        "QXmSUIQx1SEA/0YCg5vz6sS/y526la7Ivycz+OId6WPuZENTQvYP36qzyzCghYZ+\n"
        "imU+4ZjXDOi/s3mR9lK8O9H1ZGKMrXPDbOpk2cseWlQu+utivOyGxtqiO7mWsKra\n"
        "89zavyNPa9zjzR2nSkxw1Y8OfaCHN0rW0e2jCXURMEM7iyn2VXdnq2TDc2dQ3Yzs\n"
        "iRVRT6Nm4JNC2Q0eTGpg5Y7lcMLs9KsZdYTreDtPd29j93gvlMELbyLXk2D6j4lY\n"
        "m/DfZqmrJa1LzM4YDClSS4C8CNv4m1f/k17uJZl1n5KJz6ccj+tajophQHCVriDw\n"
        "4J0HTFbrAgMBAAECggEAIFfkHFnKbAjB8800N46mWzO5SPKd+t6fjezwfBpTuUqU\n"
        "W4SlmBYbIhnNKQI7E9SvqaOIhzoHi8/TSgxP7xhMT7EQ4IYpRot+OzYOh5D9Zofm\n"
        "RHHN4Z25hjEQX5eRG4n0XszOd/Vl+Qv8aOUdbIPb/RobGPCC5hdTA4AeMGGqqffO\n"
        "UP8FpPKeeCpiMgO1lGWPBzIOGBWw1SPVUv/AMMernLE3zx/KjkoEzSiFPFQnfUqn\n"
        "QT4XkPXO12ivEpac3K3HAO80en8cRo/raXIHz98atdBatkSL5szz+WBfab1+W8Y8\n"
        "2VlequieAiFjG0HZGuL06SCaEwvN8B+WtaRAbLJh4QKBgQD899h6z2vbjKJ5VXig\n"
        "0+F2Gm1sI6m9/+3knGVuF9DaidmBWY/z3iHw7Yn0eYhDzdzx+vFz9tcgaelmwwGc\n"
        "wPlA9oilVf7okWkHmKVtzhvjLu8P+dTUAUx7hnB9vL/g0IVd4LBaXtrIyuYKmWYC\n"
        "1nM6WCavl3iaZI03D4Fcm8gf5QKBgQDRZknATA/enwWuzKJ9VieMoOHf+vUfGXDy\n"
        "A3E+QJiQk4kbTI6HjjiXdZ+3fSqs3hCDJJRwirx/06x1tE+JLpEDKaMUzfXc7f5f\n"
        "h3Luk/fX2v/sOUv9Z2VYRX/T3OA4M3dsYuedl86KbPbGstAXtd3ryMpWDIHwe8Lh\n"
        "Jyu4WXQOjwKBgHqTbeY0U8MSpje83rKveHRN4e+gaEOUm6VG0y9BgwYh/a7bwxEb\n"
        "9b5Um6BGs2yHeCQYwJ68YQlnyJzuqP1X1G9SBmx2+wmeTBJOOPq4AHNY7qYDHMmN\n"
        "x42jWybSKve4kqcmlM11YPYius0v/2ool7URuc0jzGCjkHm5nalOIu6xAoGBAKrB\n"
        "8hR5MlvFa/nub/crgZbhPzDKUoeo1HzWp8svG/qSBTWYyYXrSWThi3wy+tD7SZXY\n"
        "B6B60lqe/70Aa/efXKuqkY2OoxclkNDQAL5jNbH1qcs1si0T2wYPrrzTGplaBuIB\n"
        "CYVu3QPu9ZJ1ENK1ASKcm7tt+axlEDdRHQr5D3YrAoGBAI+B8SeqpUFyVhqal3Ai\n"
        "LNJBFMV2V9fbg6d6C0G/7ueRvch3cv0Nbj0d907js+cJnI9qJAhWZtn183u9kbQn\n"
        "uSi/+st9wHSGhqXZ9Lor1skZdxGCdXkWiEl5wf574cugsgwQmhZ6s+BMGfixS382\n"
        "DsLI/WgiI0qQm1MH7h3ILtCO\n"
        "-----END PRIVATE KEY-----";
    const char *pub_pem = "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzutrXTYMVz7TO0F5klCE\n"
        "MdUhAP9GAoOb8+rEv8udupWuyL8nM/jiHelj7mRDU0L2D9+qs8swoIWGfoplPuGY\n"
        "1wzov7N5kfZSvDvR9WRijK1zw2zqZNnLHlpULvrrYrzshsbaoju5lrCq2vPc2r8j\n"
        "T2vc480dp0pMcNWPDn2ghzdK1tHtowl1ETBDO4sp9lV3Z6tkw3NnUN2M7IkVUU+j\n"
        "ZuCTQtkNHkxqYOWO5XDC7PSrGXWE63g7T3dvY/d4L5TBC28i15Ng+o+JWJvw32ap\n"
        "qyWtS8zOGAwpUkuAvAjb+JtX/5Ne7iWZdZ+Sic+nHI/rWo6KYUBwla4g8OCdB0xW\n"
        "6wIDAQAB\n"
        "-----END PUBLIC KEY-----";

    priv = CMUTIL_PrivateKeyCreateFromPEM(priv_pem, "");
    ASSERT(priv != NULL, "CMUTIL_PrivateKeyCreateFromPEM");

    pub = CMUTIL_PublicKeyCreateFromPEM(pub_pem);
    ASSERT(pub != NULL, "CMUTIL_PublicKeyCreateFromPEM");

    pub_der = CMCall(pub, GetEncoded);
    ASSERT(pub_der != NULL, "CMUTIL_RSAKey.GetEncoded");
    ASSERT(CMCall(pub_der, GetSize) > 0, "public key DER size");

    rsa = CMUTIL_RSACryptoCreate();
    ASSERT(rsa != NULL, "CMUTIL_RSACryptoCreate");

    plain = CMUTIL_StringCreateEx(0, "hello rsa sign and encrypt");
    ASSERT(plain != NULL, "create RSA plain text");

    encrypted = CMCall(rsa, EncryptWithPublicKey, plain, pub);
    ASSERT(encrypted != NULL, "EncryptWithPublicKey");
    ASSERT(CMCall(encrypted, GetSize) > 0, "RSA encrypted size");

    decrypted = CMCall(rsa, DecryptWithPrivateKey, encrypted, priv);
    ASSERT(decrypted != NULL, "DecryptWithPrivateKey");
    ASSERT(CMCall(decrypted, GetSize) == CMCall(plain, GetSize),
        "RSA decrypted size matches plain");
    ASSERT(memcmp(
        CMCall(decrypted, GetCString),
        CMCall(plain, GetCString),
        CMCall(plain, GetSize)) == 0,
        "public encrypt/private decrypt round trip");

    signature = CMCall(rsa, Sign, plain, priv);
    ASSERT(signature != NULL, "Sign");
    ASSERT(CMCall(signature, GetSize) > 0, "signature size");

    ASSERT(CMCall(rsa, VerifySignature, plain, signature, pub) == CMTrue,
        "VerifySignature valid data");

    CMCall(plain, AddString, "!");
    ASSERT(CMCall(rsa, VerifySignature, plain, signature, pub) == CMFalse,
        "VerifySignature fails on modified plain");

    CMUTIL_CryptoRandom(rnd1, sizeof(rnd1));
    CMUTIL_CryptoRandom(rnd2, sizeof(rnd2));
    ASSERT(memcmp(rnd1, rnd2, sizeof(rnd1)) != 0,
        "CMUTIL_CryptoRandom returns different buffers");

    b64 = CMUTIL_CryptoToBase64((const uint8_t*)"hello", 5);
    ASSERT(b64 != NULL, "CMUTIL_CryptoToBase64");
    ASSERT(strcmp(CMCall(b64, GetCString), "aGVsbG8=") == 0, "Base64 encoding correct");
    CMLogInfo("Base64 encoding: %s", CMCall(b64, GetCString));

    b64_decoded = CMUTIL_CryptoFromBase64(CMCall(b64, GetCString));
    ASSERT(b64_decoded != NULL, "CMUTIL_CryptoFromBase64");
    ASSERT(strcmp(CMCall(b64_decoded, GetCString), "hello") == 0, "Base64 decoding correct");
    CMLogInfo("Base64 decoding: %s", CMCall(b64_decoded, GetCString));


    ir = 0;
END_POINT:
    if (priv_file) CMCall(priv_file, Destroy);
    if (pub_file) CMCall(pub_file, Destroy);
    if (pub_der) CMCall(pub_der, Destroy);
    if (signature) CMCall(signature, Destroy);
    if (decrypted) CMCall(decrypted, Destroy);
    if (encrypted) CMCall(encrypted, Destroy);
    if (plain) CMCall(plain, Destroy);
    if (rsa) CMCall(rsa, Destroy);
    if (pub) CMCall(pub, Destroy);
    if (priv) CMCall(priv, Destroy);
    if (block) CMCall(block, Destroy);
    if (pathstr) CMCall(pathstr, Destroy);
    if (env) CMCall(env, Destroy);
    if (b64) CMCall(b64, Destroy);
    if (b64_decoded) CMCall(b64_decoded, Destroy);
    if (!CMUTIL_Clear()) ir = -1;
    return ir;
}