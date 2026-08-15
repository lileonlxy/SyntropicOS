#if __has_include("syn_config.h")
#include "syn_config.h"
#endif

#if !defined(SYN_USE_BASE64) || SYN_USE_BASE64

/**
 * @file syn_base64.c
 * @brief RFC 4648 Base64 and Base64URL codec implementation.
 */

#include "syn_base64.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** @brief Standard RFC 4648 Base64 character encoding table. */
static const char B64_STD_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/** @brief URL and filename-safe RFC 4648 Base64URL character encoding table. */
static const char B64_URL_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/** @brief Marker value for non-base64 character. */
#define B64_VAL_INVALID 0xFFU
/** @brief Marker value for base64 padding '=' character. */
#define B64_VAL_PAD 0xFEU
/** @brief Marker value for whitespace character. */
#define B64_VAL_WS 0xFDU

/**
 * @brief Decode single ASCII character into 6-bit value or control marker.
 * @param c        ASCII character to decode.
 * @param url_mode True if decoding in URL-safe mode (- and _).
 * @return 6-bit value (0-63) or marker constant (B64_VAL_PAD, B64_VAL_WS, B64_VAL_INVALID).
 */
static uint8_t decode_char(char c, bool url_mode)
{
    if (c >= 'A' && c <= 'Z') {
        return (uint8_t)(c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return (uint8_t)(c - 'a' + 26);
    }
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0' + 52);
    }
    if (c == '+' && !url_mode) {
        return 62U;
    }
    if (c == '-' && url_mode) {
        return 62U;
    }
    if (c == '/' && !url_mode) {
        return 63U;
    }
    if (c == '_' && url_mode) {
        return 63U;
    }
    if (c == '=') {
        return B64_VAL_PAD;
    }
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        return B64_VAL_WS;
    }
    return B64_VAL_INVALID;
}

bool syn_base64_encode(const uint8_t *src, size_t src_len, char *dst, size_t dst_size,
                       size_t *out_len)
{
    if (dst == NULL || (src == NULL && src_len > 0)) {
        return false;
    }

    size_t req_len = syn_base64_calc_encoded_size(src_len);
    if (dst_size < req_len) {
        return false;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 2U < src_len) {
        uint32_t triple = ((uint32_t)src[in_pos] << 16U) | ((uint32_t)src[in_pos + 1U] << 8U) |
                          (uint32_t)src[in_pos + 2U];
        dst[out_pos++] = B64_STD_TABLE[(triple >> 18U) & 0x3FU];
        dst[out_pos++] = B64_STD_TABLE[(triple >> 12U) & 0x3FU];
        dst[out_pos++] = B64_STD_TABLE[(triple >> 6U) & 0x3FU];
        dst[out_pos++] = B64_STD_TABLE[triple & 0x3FU];
        in_pos += 3U;
    }

    if (in_pos < src_len) {
        size_t rem = src_len - in_pos;
        if (rem == 1U) {
            uint32_t triple = (uint32_t)src[in_pos] << 16U;
            dst[out_pos++] = B64_STD_TABLE[(triple >> 18U) & 0x3FU];
            dst[out_pos++] = B64_STD_TABLE[(triple >> 12U) & 0x3FU];
            dst[out_pos++] = '=';
            dst[out_pos++] = '=';
        } else { /* rem == 2 */
            uint32_t triple = ((uint32_t)src[in_pos] << 16U) | ((uint32_t)src[in_pos + 1U] << 8U);
            dst[out_pos++] = B64_STD_TABLE[(triple >> 18U) & 0x3FU];
            dst[out_pos++] = B64_STD_TABLE[(triple >> 12U) & 0x3FU];
            dst[out_pos++] = B64_STD_TABLE[(triple >> 6U) & 0x3FU];
            dst[out_pos++] = '=';
        }
    }

    dst[out_pos] = '\0';
    if (out_len != NULL) {
        *out_len = out_pos;
    }
    return true;
}

bool syn_base64url_encode(const uint8_t *src, size_t src_len, char *dst, size_t dst_size,
                          bool with_padding, size_t *out_len)
{
    if (dst == NULL || (src == NULL && src_len > 0)) {
        return false;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    size_t req_len =
        with_padding ? syn_base64_calc_encoded_size(src_len) : (((src_len * 4U) + 2U) / 3U) + 1U;
    if (dst_size < req_len) {
        return false;
    }

    while (in_pos + 2U < src_len) {
        uint32_t triple = ((uint32_t)src[in_pos] << 16U) | ((uint32_t)src[in_pos + 1U] << 8U) |
                          (uint32_t)src[in_pos + 2U];
        dst[out_pos++] = B64_URL_TABLE[(triple >> 18U) & 0x3FU];
        dst[out_pos++] = B64_URL_TABLE[(triple >> 12U) & 0x3FU];
        dst[out_pos++] = B64_URL_TABLE[(triple >> 6U) & 0x3FU];
        dst[out_pos++] = B64_URL_TABLE[triple & 0x3FU];
        in_pos += 3U;
    }

    if (in_pos < src_len) {
        size_t rem = src_len - in_pos;
        if (rem == 1U) {
            uint32_t triple = (uint32_t)src[in_pos] << 16U;
            dst[out_pos++] = B64_URL_TABLE[(triple >> 18U) & 0x3FU];
            dst[out_pos++] = B64_URL_TABLE[(triple >> 12U) & 0x3FU];
            if (with_padding) {
                dst[out_pos++] = '=';
                dst[out_pos++] = '=';
            }
        } else { /* rem == 2 */
            uint32_t triple = ((uint32_t)src[in_pos] << 16U) | ((uint32_t)src[in_pos + 1U] << 8U);
            dst[out_pos++] = B64_URL_TABLE[(triple >> 18U) & 0x3FU];
            dst[out_pos++] = B64_URL_TABLE[(triple >> 12U) & 0x3FU];
            dst[out_pos++] = B64_URL_TABLE[(triple >> 6U) & 0x3FU];
            if (with_padding) {
                dst[out_pos++] = '=';
            }
        }
    }

    dst[out_pos] = '\0';
    if (out_len != NULL) {
        *out_len = out_pos;
    }
    return true;
}

/**
 * @brief Internal shared decoder for standard and URL-safe Base64.
 * @param src      Input Base64 string buffer.
 * @param src_len  Length of input string (or 0 for strlen).
 * @param dst      [out] Output destination binary buffer.
 * @param dst_size Capacity of destination buffer in bytes.
 * @param out_len  [out] Pointer to receive decoded byte count.
 * @param url_mode True for URL-safe alphabet, false for standard RFC 4648.
 * @return True on success, false on decode error or buffer overflow.
 */
static bool base64_decode_internal(const char *src, size_t src_len, uint8_t *dst, size_t dst_size,
                                   size_t *out_len, bool url_mode)
{
    if (dst == NULL || src == NULL) {
        return false;
    }

    if (src_len == 0U) {
        src_len = strlen(src);
    }

    uint32_t block = 0U;
    int bits = 0;
    size_t out_pos = 0;
    bool pad_seen = false;

    for (size_t i = 0; i < src_len; i++) {
        char c = src[i];
        uint8_t val = decode_char(c, url_mode);

        if (val == B64_VAL_WS) {
            continue;
        }

        if (val == B64_VAL_INVALID) {
            return false;
        }

        if (val == B64_VAL_PAD) {
            pad_seen = true;
            continue;
        }

        if (pad_seen) {
            /* Non-padding character encountered after padding '=' */
            return false;
        }

        block = (block << 6U) | (uint32_t)val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            if (out_pos >= dst_size) {
                return false;
            }
            dst[out_pos++] = (uint8_t)((block >> bits) & 0xFFU);
        }
    }

    if (out_len != NULL) {
        *out_len = out_pos;
    }
    return true;
}

bool syn_base64_decode(const char *src, size_t src_len, uint8_t *dst, size_t dst_size,
                       size_t *out_len)
{
    return base64_decode_internal(src, src_len, dst, dst_size, out_len, false);
}

bool syn_base64url_decode(const char *src, size_t src_len, uint8_t *dst, size_t dst_size,
                          size_t *out_len)
{
    return base64_decode_internal(src, src_len, dst, dst_size, out_len, true);
}

#endif /* SYN_USE_BASE64 */
