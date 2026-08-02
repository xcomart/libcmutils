/*
 * 19 - Cryptography
 *
 * Shows: CMUTIL_BlockCrypto (AES-CBC and AES-GCM), CMUTIL_RSACrypto with PEM
 *        keys, signing and verification, Base64 and secure random bytes.
 *
 * The key pair in samples/conf is a throwaway generated for this sample.
 * Never reuse it for anything real.
 */

#include "sample_common.h"

CMUTIL_LogDefine("sample.crypto")

/* A 256 bit key and a 128 bit IV. Real code derives these, it does not
 * hard-code them. */
static const uint8_t AES_KEY[32] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50
};
static const uint8_t AES_IV[16] = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70
};

static void log_hex(const char *label, CMUTIL_String *data)
{
    const uint8_t *bytes = (const uint8_t*)CMCall(data, GetCString);
    size_t size = CMCall(data, GetSize);
    CMUTIL_String *hex = CMUTIL_StringCreate();
    size_t i;
    size_t shown = size > 24 ? 24 : size;

    for (i = 0; i < shown; i++)
        CMCall(hex, AddPrint, "%02x", bytes[i]);
    if (shown < size)
        CMCall(hex, AddString, "...");

    CMLogInfo("%s: %u bytes [%s]",
              label, (unsigned)size, CMCall(hex, GetCString));
    CMCall(hex, Destroy);
}

static void round_trip(const char *algo, const char *mode, const char *padding,
                       int key_bits)
{
    CMUTIL_BlockCrypto *crypto;
    CMUTIL_String *plain;
    CMUTIL_String *encrypted;
    CMUTIL_String *decrypted;

    crypto = CMUTIL_BlockCryptoCreate(algo, mode, padding, key_bits);
    if (crypto == NULL) {
        CMLogWarn("%s/%s/%s is not available", algo, mode, padding);
        return;
    }

    plain = CMUTIL_StringCreateEx(0, "a message worth protecting");

    /* The object is reusable; the key and IV are per call. */
    encrypted = CMCall(crypto, Encrypt, plain, AES_KEY, AES_IV);
    decrypted = CMCall(crypto, Decrypt, encrypted, AES_KEY, AES_IV);

    CMLogInfo("%s/%s/%s", algo, mode, padding);
    log_hex("  ciphertext", encrypted);
    CMLogInfo("  decrypted : %s", CMCall(decrypted, GetCString));

    CMCall(decrypted, Destroy);
    CMCall(encrypted, Destroy);
    CMCall(plain, Destroy);
    CMCall(crypto, Destroy);
}

static void sample_block_ciphers(void)
{
    SAMPLE_SECTION("block ciphers");

    /*
     * Supported: AES (CBC, GCM, ECB, CFB/CFB128, OFB, CTR), DES (CBC, ECB,
     * CFB, OFB), DESede/TripleDES (same modes) and SEED (same modes).
     * "NoPadding" disables block padding; any other name enables it.
     */
    round_trip("AES", "CBC", "PKCS5Padding", 256);
    round_trip("AES", "CTR", "NoPadding", 256);

    /*
     * GCM is AEAD and this library frames it self-contained: Encrypt appends
     * the 16 byte authentication tag right after the ciphertext, and Decrypt
     * expects it there, verifies it and strips it. Do not carry the tag
     * separately. Block padding is always off in GCM.
     */
    round_trip("AES", "GCM", "NoPadding", 256);
}

static void sample_rsa(void)
{
    CMUTIL_PrivateKey *priv;
    CMUTIL_PublicKey *pub;
    CMUTIL_RSACrypto *rsa;
    CMUTIL_String *plain;
    CMUTIL_String *encrypted;
    CMUTIL_String *decrypted;
    CMUTIL_String *signature;
    CMUTIL_String *der;

    SAMPLE_SECTION("RSA");

    /* CMUTIL_PrivateKeyCreateFromPEM(pem_text, passphrase) reads the same
     * formats from memory. The passphrase may be "" or NULL when the key is
     * not encrypted. */
    priv = CMUTIL_PrivateKeyCreateFromFile(
            SAMPLE_DATA("sample_rsa_private.pem"), (const uint8_t*)"");
    pub = CMUTIL_PublicKeyCreateFromFile(
            SAMPLE_DATA("sample_rsa_public.pem"));

    if (priv == NULL || pub == NULL) {
        CMLogError("could not load the sample key pair from %s",
                   SAMPLE_DATA_DIR);
        if (priv) CMCall(priv, Destroy);
        if (pub) CMCall(pub, Destroy);
        return;
    }

    der = CMCall(pub, GetEncoded);
    log_hex("public key DER", der);
    CMCall(der, Destroy);

    rsa = CMUTIL_RSACryptoCreate();
    plain = CMUTIL_StringCreateEx(0, "hello rsa");

    /* Public encrypt / private decrypt uses OAEP padding. The reverse pair
     * (EncryptWithPrivateKey / DecryptWithPublicKey) uses PKCS#1 v1.5,
     * because OAEP does not work in that direction. */
    encrypted = CMCall(rsa, EncryptWithPublicKey, plain, pub);
    decrypted = CMCall(rsa, DecryptWithPrivateKey, encrypted, priv);
    log_hex("  ciphertext", encrypted);
    CMLogInfo("  decrypted : %s", CMCall(decrypted, GetCString));

    /* Sign / VerifySignature use SHA-256. */
    signature = CMCall(rsa, Sign, plain, priv);
    log_hex("  signature ", signature);
    CMLogInfo("  verify    : %s",
              CMCall(rsa, VerifySignature, plain, signature, pub)
                  ? "valid" : "INVALID");

    /* Tamper with the message and the signature stops matching. */
    CMCall(plain, AddString, "!");
    CMLogInfo("  verify after tampering: %s",
              CMCall(rsa, VerifySignature, plain, signature, pub)
                  ? "valid" : "INVALID (as expected)");

    CMCall(signature, Destroy);
    CMCall(decrypted, Destroy);
    CMCall(encrypted, Destroy);
    CMCall(plain, Destroy);
    CMCall(rsa, Destroy);
    CMCall(pub, Destroy);
    CMCall(priv, Destroy);
}

static void sample_helpers(void)
{
    uint8_t random[16];
    CMUTIL_String *encoded;
    CMUTIL_String *decoded;
    CMUTIL_String *hex;
    size_t i;

    SAMPLE_SECTION("Base64 and secure random");

    encoded = CMUTIL_CryptoToBase64((const uint8_t*)"hello", 5);
    CMLogInfo("Base64(\"hello\") = %s", CMCall(encoded, GetCString));

    decoded = CMUTIL_CryptoFromBase64(CMCall(encoded, GetCString));
    CMLogInfo("decoded back    = %s", CMCall(decoded, GetCString));

    CMCall(decoded, Destroy);
    CMCall(encoded, Destroy);

    /* Cryptographically strong bytes. */
    CMUTIL_CryptoRandom(random, sizeof(random));
    hex = CMUTIL_StringCreate();
    for (i = 0; i < sizeof(random); i++)
        CMCall(hex, AddPrint, "%02x", random[i]);
    CMLogInfo("16 random bytes = %s", CMCall(hex, GetCString));
    CMCall(hex, Destroy);
}

int main(void)
{
    sample_init();

    sample_block_ciphers();
    sample_rsa();
    sample_helpers();

    return sample_exit(0);
}
