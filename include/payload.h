#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <stdint.h>

#include "common.h"

/*
 * Payload identification.
 */
#define PAYLOAD_MAGIC      "STEG"
#define PAYLOAD_MAGIC_SIZE 4U

/*
 * Current payload format version.
 */
#define PAYLOAD_VERSION 1U

/*
 * Fixed serialized header size.
 *
 * 4  bytes : MAGIC
 * 1  byte  : VERSION
 * 1  byte  : FLAGS
 * 1  byte  : TYPE
 * 2  bytes : FILENAME_LENGTH
 * 4  bytes : PAYLOAD_SIZE
 * 4  bytes : CRC32
 */
#define PAYLOAD_HEADER_SIZE 17U

/*
 * Payload flags.
 */
#define PAYLOAD_FLAG_NONE      0x00U
#define PAYLOAD_FLAG_FILENAME  0x01U
#define PAYLOAD_FLAG_CRC32     0x02U

/*
 * Supported payload types.
 */
typedef enum
{
    PAYLOAD_TYPE_TEXT = 0,
    PAYLOAD_TYPE_FILE = 1

} PayloadType;

/**
 * @brief Describes metadata associated with a hidden payload.
 *
 * This structure represents logical payload metadata.
 * It is not written directly to the carrier file as a C structure.
 */
typedef struct
{
    uint8_t version;
    uint8_t flags;
    uint8_t type;

    uint16_t filename_length;

    uint32_t payload_size;
    uint32_t crc32;

} PayloadInfo;

/**
 * @brief Validates payload metadata.
 *
 * @param info Payload metadata to validate.
 *
 * @return STEG_SUCCESS if valid, otherwise an error code.
 */
StegStatus payload_validate(const PayloadInfo *info);

/**
 * @brief Serializes payload metadata into a fixed-size byte buffer.
 *
 * @param info Payload metadata to serialize.
 * @param buffer Destination buffer of PAYLOAD_HEADER_SIZE bytes.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
StegStatus payload_serialize_header(const PayloadInfo *info,
                                    uint8_t *buffer);

/**
 * @brief Parses payload metadata from a serialized header.
 *
 * @param buffer Serialized payload header.
 * @param info Destination structure.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
StegStatus payload_parse_header(const uint8_t *buffer,
                                PayloadInfo *info);

#endif /* PAYLOAD_H */