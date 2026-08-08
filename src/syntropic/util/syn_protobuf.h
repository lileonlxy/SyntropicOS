/**
 * @file syn_protobuf.h
 * @brief Zero-Allocation Protocol Buffers Serializer & Deserializer.
 * @ingroup syn_util
 *
 * Provides varint (LEB128), fixed32/64, length-delimited string/bytes encoding and
 * decoding for compact binary serialization on microcontrollers.
 */

#ifndef SYN_PROTOBUF_H
#define SYN_PROTOBUF_H

#include "syntropic/common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Protobuf Wire Types */
#define SYN_PB_WIRE_VARINT 0U           /**< int32, int64, uint32, uint64, bool, enum */
#define SYN_PB_WIRE_FIXED64 1U          /**< fixed64, sfixed64, double */
#define SYN_PB_WIRE_LENGTH_DELIMITED 2U /**< string, bytes, embedded messages */
#define SYN_PB_WIRE_FIXED32 5U          /**< fixed32, sfixed32, float */

/** Protobuf Encoder Context */
typedef struct {
    uint8_t *buf;    /**< Destination buffer */
    size_t capacity; /**< Maximum capacity */
    size_t offset;   /**< Current write offset */
} SYN_PB_Encoder;

/** Protobuf Decoder Context */
typedef struct {
    const uint8_t *buf; /**< Source buffer */
    size_t size;        /**< Total payload size */
    size_t offset;      /**< Current read offset */
} SYN_PB_Decoder;

/** Protobuf Header Tag */
typedef struct {
    uint32_t field_number; /**< Protobuf field tag index */
    uint8_t wire_type;     /**< Protobuf wire type */
} SYN_PB_Tag;

/**
 * @brief Initialize a Protobuf encoder instance.
 * @param enc Pointer to encoder instance.
 * @param buf Output byte buffer.
 * @param capacity Maximum buffer size in bytes.
 * @return SYN_OK on success, SYN_INVALID_PARAM if NULL or zero.
 */
SYN_Status syn_pb_encoder_init(SYN_PB_Encoder *enc, uint8_t *buf, size_t capacity);

/**
 * @brief Encode field tag and wire type.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param wire_type Protobuf wire type (0, 1, 2, 5).
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_tag(SYN_PB_Encoder *enc, uint32_t field_number, uint8_t wire_type);

/**
 * @brief Encode a raw varint value.
 * @param enc Pointer to encoder instance.
 * @param value 64-bit value to encode.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_varint(SYN_PB_Encoder *enc, uint64_t value);

/**
 * @brief Encode a signed 32-bit integer field.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param value Signed 32-bit integer value.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_int32(SYN_PB_Encoder *enc, uint32_t field_number, int32_t value);

/**
 * @brief Encode an unsigned 32-bit integer field.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param value Unsigned 32-bit integer value.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_uint32(SYN_PB_Encoder *enc, uint32_t field_number, uint32_t value);

/**
 * @brief Encode a boolean field.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param value Boolean value.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_bool(SYN_PB_Encoder *enc, uint32_t field_number, bool value);

/**
 * @brief Encode a fixed 32-bit integer field.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param value Fixed 32-bit integer value.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_fixed32(SYN_PB_Encoder *enc, uint32_t field_number, uint32_t value);

/**
 * @brief Encode a UTF-8 string field.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param str Null-terminated string pointer.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_string(SYN_PB_Encoder *enc, uint32_t field_number, const char *str);

/**
 * @brief Encode a raw byte array field.
 * @param enc Pointer to encoder instance.
 * @param field_number Field tag number.
 * @param bytes Pointer to input byte array.
 * @param len Length of byte array.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on overflow.
 */
SYN_Status syn_pb_encode_bytes(SYN_PB_Encoder *enc, uint32_t field_number, const uint8_t *bytes,
                               size_t len);

/**
 * @brief Initialize a Protobuf decoder instance.
 * @param dec Pointer to decoder instance.
 * @param buf Input byte buffer pointer.
 * @param size Input byte buffer length.
 * @return SYN_OK on success, SYN_INVALID_PARAM if NULL.
 */
SYN_Status syn_pb_decoder_init(SYN_PB_Decoder *dec, const uint8_t *buf, size_t size);

/**
 * @brief Decode the next field tag and wire type.
 * @param dec Pointer to decoder instance.
 * @param tag Output tag structure pointer.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on EOF.
 */
SYN_Status syn_pb_decode_tag(SYN_PB_Decoder *dec, SYN_PB_Tag *tag);

/**
 * @brief Decode a raw varint value.
 * @param dec Pointer to decoder instance.
 * @param value Output 64-bit value pointer.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on EOF.
 */
SYN_Status syn_pb_decode_varint(SYN_PB_Decoder *dec, uint64_t *value);

/**
 * @brief Decode a fixed 32-bit integer field value.
 * @param dec Pointer to decoder instance.
 * @param value Output 32-bit value pointer.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on EOF.
 */
SYN_Status syn_pb_decode_fixed32(SYN_PB_Decoder *dec, uint32_t *value);

/**
 * @brief Decode a length-delimited byte array field.
 * @param dec Pointer to decoder instance.
 * @param bytes Output byte array pointer pointer.
 * @param len Output byte array length pointer.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on EOF.
 */
SYN_Status syn_pb_decode_bytes(SYN_PB_Decoder *dec, const uint8_t **bytes, size_t *len);

/**
 * @brief Skip unknown field based on wire type.
 * @param dec Pointer to decoder instance.
 * @param wire_type Wire type of field to skip.
 * @return SYN_OK on success, SYN_INVALID_PARAM or SYN_ERROR on failure.
 */
SYN_Status syn_pb_skip_field(SYN_PB_Decoder *dec, uint8_t wire_type);

#ifdef __cplusplus
}
#endif

#endif /* SYN_PROTOBUF_H */
