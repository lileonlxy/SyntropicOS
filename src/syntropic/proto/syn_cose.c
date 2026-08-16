/**
 * @file syn_cose.c
 * @brief Zero-Heap CBOR Object Signing and Encryption (COSE - RFC 9052) implementation.
 */

#include "syntropic/proto/syn_cose.h"

#if !defined(SYN_USE_COSE) || SYN_USE_COSE

#include "syntropic/util/syn_random.h"

#include <string.h>

/** @cond INTERNAL */

/* Build serialized Sig_structure per RFC 9052 Section 4.4 */
static bool build_sig_structure(const uint8_t *body_protected, size_t body_protected_len,
                                const uint8_t *external_aad, size_t external_aad_len,
                                const uint8_t *payload, size_t payload_len, uint8_t *out_buf,
                                size_t out_buf_size, size_t *out_len)
{
    SYN_CborWriter w;
    syn_cbor_writer_init(&w, out_buf, out_buf_size);

    /* Sig_structure is an array of 4 elements */
    syn_cbor_write_array_begin(&w, 4);
    syn_cbor_write_text_cstr(&w, "Signature1");
    syn_cbor_write_bytes(&w, body_protected, body_protected_len);
    syn_cbor_write_bytes(&w, (external_aad != NULL) ? external_aad : (const uint8_t *)"",
                         external_aad_len);
    syn_cbor_write_bytes(&w, (payload != NULL) ? payload : (const uint8_t *)"", payload_len);

    if (!syn_cbor_writer_ok(&w)) {
        return false;
    }

    *out_len = syn_cbor_writer_len(&w);
    return true;
}

/* Build serialized Enc_structure per RFC 9052 Section 5.3 */
static bool build_enc_structure(const uint8_t *body_protected, size_t body_protected_len,
                                const uint8_t *external_aad, size_t external_aad_len,
                                uint8_t *out_buf, size_t out_buf_size, size_t *out_len)
{
    SYN_CborWriter w;
    syn_cbor_writer_init(&w, out_buf, out_buf_size);

    /* Enc_structure is an array of 3 elements */
    syn_cbor_write_array_begin(&w, 3);
    syn_cbor_write_text_cstr(&w, "Encrypt0");
    syn_cbor_write_bytes(&w, body_protected, body_protected_len);
    syn_cbor_write_bytes(&w, (external_aad != NULL) ? external_aad : (const uint8_t *)"",
                         external_aad_len);

    if (!syn_cbor_writer_ok(&w)) {
        return false;
    }

    *out_len = syn_cbor_writer_len(&w);
    return true;
}

/** @endcond */

SYN_Status syn_cose_sign1_create(SYN_COSE_Algorithm alg, const uint8_t *secret_key,
                                 const uint8_t *public_key, const uint8_t *kid, size_t kid_len,
                                 const uint8_t *payload, size_t payload_len,
                                 const uint8_t *external_aad, size_t external_aad_len,
                                 uint8_t *out_buf, size_t out_buf_size, size_t *out_len)
{
    if (secret_key == NULL || out_buf == NULL || out_len == NULL ||
        (payload == NULL && payload_len > 0U)) {
        return SYN_ERROR;
    }

    if (alg != SYN_COSE_ALGO_EDDSA && alg != SYN_COSE_ALGO_ES256) {
        return SYN_ERROR;
    }

    /* 1. Build protected header: { 1: alg } */
    uint8_t protected_raw[16];
    SYN_CborWriter pw;
    syn_cbor_writer_init(&pw, protected_raw, sizeof(protected_raw));
    syn_cbor_write_map_begin(&pw, 1);
    syn_cbor_write_uint(&pw, SYN_COSE_HEADER_ALG);
    syn_cbor_write_int(&pw, (int64_t)alg);
    if (!syn_cbor_writer_ok(&pw)) {
        return SYN_ERROR;
    }
    size_t protected_raw_len = syn_cbor_writer_len(&pw);

    /* 2. Build Sig_structure for signature generation */
    uint8_t sig_structure[1024];
    size_t sig_struct_len = 0;
    if (!build_sig_structure(protected_raw, protected_raw_len, external_aad, external_aad_len,
                             payload, payload_len, sig_structure, sizeof(sig_structure),
                             &sig_struct_len)) {
        return SYN_ERROR;
    }

    /* 3. Compute signature */
    uint8_t signature[SYN_COSE_MAX_SIG_LEN];
    size_t sig_len = 64U;

    if (alg == SYN_COSE_ALGO_EDDSA) {
        if (!syn_ed25519_sign(sig_structure, sig_struct_len, secret_key, public_key, signature)) {
            return SYN_ERROR;
        }
    } else {
        /* ES256: Hash Sig_structure with SHA-256 then sign with P-256 ECDSA */
        uint8_t hash[SYN_SHA256_DIGEST_SIZE];
        syn_sha256(sig_structure, sig_struct_len, hash);

        uint8_t nonce_k[32];
        (void)syn_random_fill(nonce_k, sizeof(nonce_k));
        if (!syn_p256_sign_ecdsa(secret_key, nonce_k, hash, signature, signature + 32)) {
            return SYN_ERROR;
        }
    }

    /* 4. Encode final COSE_Sign1 message */
    SYN_CborWriter w;
    syn_cbor_writer_init(&w, out_buf, out_buf_size);

    syn_cbor_write_array_begin(&w, 4);
    syn_cbor_write_bytes(&w, protected_raw, protected_raw_len);

    /* Unprotected header map */
    if (kid != NULL && kid_len > 0U) {
        syn_cbor_write_map_begin(&w, 1);
        syn_cbor_write_uint(&w, SYN_COSE_HEADER_KID);
        syn_cbor_write_bytes(&w, kid, kid_len);
    } else {
        syn_cbor_write_map_begin(&w, 0);
    }

    /* Payload */
    syn_cbor_write_bytes(&w, (payload != NULL) ? payload : (const uint8_t *)"", payload_len);

    /* Signature */
    syn_cbor_write_bytes(&w, signature, sig_len);

    if (!syn_cbor_writer_ok(&w)) {
        return SYN_ERROR;
    }

    *out_len = syn_cbor_writer_len(&w);
    return SYN_OK;
}

SYN_Status syn_cose_sign1_verify(const uint8_t *msg, size_t msg_len, const uint8_t *public_key,
                                 size_t public_key_len, const uint8_t *external_aad,
                                 size_t external_aad_len, SYN_COSE_Sign1Message *parsed_out)
{
    if (msg == NULL || msg_len == 0U || public_key == NULL) {
        return SYN_ERROR;
    }

    SYN_CborReader r;
    syn_cbor_reader_init(&r, msg, msg_len);

    size_t array_items = syn_cbor_read_array_begin(&r);
    if (!syn_cbor_reader_ok(&r) || array_items != 4U) {
        return SYN_ERROR;
    }

    /* 1. Read protected header bstr */
    uint8_t protected_buf[SYN_COSE_MAX_PROTECTED_LEN];
    size_t protected_len = syn_cbor_read_bytes(&r, protected_buf, sizeof(protected_buf));
    if (!syn_cbor_reader_ok(&r)) {
        return SYN_ERROR;
    }

    /* Parse protected header map */
    SYN_COSE_Algorithm parsed_alg = SYN_COSE_ALGO_UNKNOWN;
    if (protected_len > 0U) {
        SYN_CborReader pr;
        syn_cbor_reader_init(&pr, protected_buf, protected_len);
        size_t pairs = syn_cbor_read_map_begin(&pr);
        for (size_t i = 0; i < pairs; i++) {
            uint64_t label = syn_cbor_read_uint(&pr);
            if (label == SYN_COSE_HEADER_ALG) {
                parsed_alg = (SYN_COSE_Algorithm)syn_cbor_read_int(&pr);
            } else {
                syn_cbor_skip(&pr);
            }
        }
    }

    if (parsed_alg != SYN_COSE_ALGO_EDDSA && parsed_alg != SYN_COSE_ALGO_ES256) {
        return SYN_ERROR;
    }

    /* 2. Read unprotected header map */
    uint8_t parsed_kid[SYN_COSE_MAX_KID_LEN] = {0};
    size_t parsed_kid_len = 0;

    size_t unprot_pairs = syn_cbor_read_map_begin(&r);
    if (!syn_cbor_reader_ok(&r)) {
        return SYN_ERROR;
    }
    for (size_t i = 0; i < unprot_pairs; i++) {
        uint64_t label = syn_cbor_read_uint(&r);
        if (label == SYN_COSE_HEADER_KID) {
            parsed_kid_len = syn_cbor_read_bytes(&r, parsed_kid, sizeof(parsed_kid));
        } else {
            syn_cbor_skip(&r);
        }
    }

    /* 3. Read payload */
    uint8_t payload_buf[1024];
    size_t payload_len = syn_cbor_read_bytes(&r, payload_buf, sizeof(payload_buf));
    if (!syn_cbor_reader_ok(&r)) {
        return SYN_ERROR;
    }

    /* 4. Read signature */
    uint8_t sig_buf[SYN_COSE_MAX_SIG_LEN];
    size_t sig_len = syn_cbor_read_bytes(&r, sig_buf, sizeof(sig_buf));
    if (!syn_cbor_reader_ok(&r) || sig_len != 64U) {
        return SYN_ERROR;
    }

    /* Populate optional output struct */
    if (parsed_out != NULL) {
        parsed_out->alg = parsed_alg;
        (void)memcpy(parsed_out->kid, parsed_kid, parsed_kid_len);
        parsed_out->kid_len = parsed_kid_len;
        parsed_out->payload = payload_buf;
        parsed_out->payload_len = payload_len;
        parsed_out->signature = sig_buf;
        parsed_out->signature_len = sig_len;
        (void)memcpy(parsed_out->protected_hdr, protected_buf, protected_len);
        parsed_out->protected_hdr_len = protected_len;
    }

    /* 5. Reconstruct Sig_structure */
    uint8_t sig_structure[1024];
    size_t sig_struct_len = 0;
    if (!build_sig_structure(protected_buf, protected_len, external_aad, external_aad_len,
                             payload_buf, payload_len, sig_structure, sizeof(sig_structure),
                             &sig_struct_len)) {
        return SYN_ERROR;
    }

    /* 6. Verify signature */
    if (parsed_alg == SYN_COSE_ALGO_EDDSA) {
        if (public_key_len != SYN_ED25519_PUBLIC_KEY_SIZE) {
            return SYN_ERROR;
        }
        if (!syn_ed25519_verify(sig_buf, sig_structure, sig_struct_len, public_key)) {
            return SYN_ERROR;
        }
    } else {
        /* ES256 */
        const uint8_t *px = public_key;
        const uint8_t *py = public_key + 32;
        if (public_key_len == 65U && public_key[0] == 0x04U) {
            px = public_key + 1;
            py = public_key + 33;
        } else if (public_key_len != 64U) {
            return SYN_ERROR;
        }

        uint8_t hash[SYN_SHA256_DIGEST_SIZE];
        syn_sha256(sig_structure, sig_struct_len, hash);

        if (!syn_p256_verify_ecdsa(hash, sig_buf, sig_buf + 32, px, py)) {
            return SYN_ERROR;
        }
    }

    return SYN_OK;
}

SYN_Status syn_cose_encrypt0_create(SYN_COSE_Algorithm alg, const uint8_t *key, const uint8_t *iv,
                                    size_t iv_len, const uint8_t *kid, size_t kid_len,
                                    const uint8_t *plaintext, size_t plaintext_len,
                                    const uint8_t *external_aad, size_t external_aad_len,
                                    uint8_t *out_buf, size_t out_buf_size, size_t *out_len)
{
    if (key == NULL || iv == NULL || out_buf == NULL || out_len == NULL ||
        (plaintext == NULL && plaintext_len > 0U)) {
        return SYN_ERROR;
    }

    if (alg != SYN_COSE_ALGO_CHACHA20_POLY1305 || iv_len != 12U) {
        return SYN_ERROR;
    }

    /* 1. Build protected header: { 1: 24 } */
    uint8_t protected_raw[16];
    SYN_CborWriter pw;
    syn_cbor_writer_init(&pw, protected_raw, sizeof(protected_raw));
    syn_cbor_write_map_begin(&pw, 1);
    syn_cbor_write_uint(&pw, SYN_COSE_HEADER_ALG);
    syn_cbor_write_uint(&pw, (uint64_t)alg);
    if (!syn_cbor_writer_ok(&pw)) {
        return SYN_ERROR;
    }
    size_t protected_raw_len = syn_cbor_writer_len(&pw);

    /* 2. Build Enc_structure for AAD */
    uint8_t enc_structure[512];
    size_t enc_struct_len = 0;
    if (!build_enc_structure(protected_raw, protected_raw_len, external_aad, external_aad_len,
                             enc_structure, sizeof(enc_structure), &enc_struct_len)) {
        return SYN_ERROR;
    }

    /* 3. Encrypt plaintext with ChaCha20-Poly1305 (Payload || Tag) */
    uint8_t ct_tag_buf[1024];
    if (plaintext_len + 16U > sizeof(ct_tag_buf)) {
        return SYN_ERROR;
    }

    uint8_t tag[16];
    if (plaintext_len > 0U && plaintext != NULL) {
        syn_aead_encrypt(key, iv, enc_structure, enc_struct_len, plaintext, plaintext_len,
                         ct_tag_buf, tag);
    } else {
        syn_aead_encrypt(key, iv, enc_structure, enc_struct_len, NULL, 0U, NULL, tag);
    }
    (void)memcpy(ct_tag_buf + plaintext_len, tag, 16U);
    size_t total_ct_len = plaintext_len + 16U;

    /* 4. Encode final COSE_Encrypt0 message */
    SYN_CborWriter w;
    syn_cbor_writer_init(&w, out_buf, out_buf_size);

    syn_cbor_write_array_begin(&w, 3);
    syn_cbor_write_bytes(&w, protected_raw, protected_raw_len);

    /* Unprotected header map: { 5: IV, [4: kid] } */
    size_t unprot_count = 1U + (size_t)(kid != NULL && kid_len > 0U);
    syn_cbor_write_map_begin(&w, unprot_count);
    syn_cbor_write_uint(&w, SYN_COSE_HEADER_IV);
    syn_cbor_write_bytes(&w, iv, iv_len);
    if (kid != NULL && kid_len > 0U) {
        syn_cbor_write_uint(&w, SYN_COSE_HEADER_KID);
        syn_cbor_write_bytes(&w, kid, kid_len);
    }

    /* Ciphertext + Tag */
    syn_cbor_write_bytes(&w, ct_tag_buf, total_ct_len);

    if (!syn_cbor_writer_ok(&w)) {
        return SYN_ERROR;
    }

    *out_len = syn_cbor_writer_len(&w);
    return SYN_OK;
}

SYN_Status syn_cose_encrypt0_decrypt(const uint8_t *msg, size_t msg_len, const uint8_t *key,
                                     const uint8_t *external_aad, size_t external_aad_len,
                                     uint8_t *out_plaintext, size_t out_plaintext_size,
                                     size_t *out_plaintext_len,
                                     SYN_COSE_Encrypt0Message *parsed_out)
{
    if (msg == NULL || msg_len == 0U || key == NULL || out_plaintext_len == NULL) {
        return SYN_ERROR;
    }

    SYN_CborReader r;
    syn_cbor_reader_init(&r, msg, msg_len);

    size_t array_items = syn_cbor_read_array_begin(&r);
    if (!syn_cbor_reader_ok(&r) || array_items != 3U) {
        return SYN_ERROR;
    }

    /* 1. Read protected header */
    uint8_t protected_buf[SYN_COSE_MAX_PROTECTED_LEN];
    size_t protected_len = syn_cbor_read_bytes(&r, protected_buf, sizeof(protected_buf));
    if (!syn_cbor_reader_ok(&r)) {
        return SYN_ERROR;
    }

    SYN_COSE_Algorithm parsed_alg = SYN_COSE_ALGO_UNKNOWN;
    if (protected_len > 0U) {
        SYN_CborReader pr;
        syn_cbor_reader_init(&pr, protected_buf, protected_len);
        size_t pairs = syn_cbor_read_map_begin(&pr);
        for (size_t i = 0; i < pairs; i++) {
            uint64_t label = syn_cbor_read_uint(&pr);
            if (label == SYN_COSE_HEADER_ALG) {
                parsed_alg = (SYN_COSE_Algorithm)syn_cbor_read_uint(&pr);
            } else {
                syn_cbor_skip(&pr);
            }
        }
    }

    if (parsed_alg != SYN_COSE_ALGO_CHACHA20_POLY1305) {
        return SYN_ERROR;
    }

    /* 2. Read unprotected header map */
    uint8_t parsed_iv[SYN_COSE_MAX_IV_LEN] = {0};
    size_t parsed_iv_len = 0;
    uint8_t parsed_kid[SYN_COSE_MAX_KID_LEN] = {0};
    size_t parsed_kid_len = 0;

    size_t unprot_pairs = syn_cbor_read_map_begin(&r);
    if (!syn_cbor_reader_ok(&r)) {
        return SYN_ERROR;
    }
    for (size_t i = 0; i < unprot_pairs; i++) {
        uint64_t label = syn_cbor_read_uint(&r);
        if (label == SYN_COSE_HEADER_IV) {
            parsed_iv_len = syn_cbor_read_bytes(&r, parsed_iv, sizeof(parsed_iv));
        } else if (label == SYN_COSE_HEADER_KID) {
            parsed_kid_len = syn_cbor_read_bytes(&r, parsed_kid, sizeof(parsed_kid));
        } else {
            syn_cbor_skip(&r);
        }
    }

    if (parsed_iv_len != 12U) {
        return SYN_ERROR;
    }

    /* 3. Read ciphertext + tag */
    uint8_t ct_tag_buf[1024];
    size_t ct_tag_len = syn_cbor_read_bytes(&r, ct_tag_buf, sizeof(ct_tag_buf));
    if (!syn_cbor_reader_ok(&r) || ct_tag_len < 16U) {
        return SYN_ERROR;
    }

    size_t pt_len = ct_tag_len - 16U;
    if (out_plaintext != NULL && pt_len > out_plaintext_size) {
        return SYN_ERROR;
    }

    /* Populate optional output descriptor */
    if (parsed_out != NULL) {
        parsed_out->alg = parsed_alg;
        (void)memcpy(parsed_out->iv, parsed_iv, parsed_iv_len);
        parsed_out->iv_len = parsed_iv_len;
        (void)memcpy(parsed_out->kid, parsed_kid, parsed_kid_len);
        parsed_out->kid_len = parsed_kid_len;
        parsed_out->ciphertext = ct_tag_buf;
        parsed_out->ciphertext_len = ct_tag_len;
        (void)memcpy(parsed_out->protected_hdr, protected_buf, protected_len);
        parsed_out->protected_hdr_len = protected_len;
    }

    /* 4. Reconstruct Enc_structure */
    uint8_t enc_structure[512];
    size_t enc_struct_len = 0;
    if (!build_enc_structure(protected_buf, protected_len, external_aad, external_aad_len,
                             enc_structure, sizeof(enc_structure), &enc_struct_len)) {
        return SYN_ERROR;
    }

    /* 5. Decrypt and verify Poly1305 tag */
    const uint8_t *tag = ct_tag_buf + pt_len;
    if (!syn_aead_decrypt(key, parsed_iv, enc_structure, enc_struct_len, ct_tag_buf, pt_len, tag,
                          out_plaintext)) {
        return SYN_ERROR;
    }

    *out_plaintext_len = pt_len;
    return SYN_OK;
}

#endif /* SYN_USE_COSE */
