#include <stdint.h>

#include "payload.h"
#include <stddef.h>
static void write_le16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value & 0xFFU);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void write_le32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value & 0xFFU);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFU);
    buffer[2] = (uint8_t)((value >> 16) & 0xFFU);
    buffer[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint16_t read_le16(const uint8_t *buffer)
{
    return (uint16_t)buffer[0]
         | ((uint16_t)buffer[1] << 8);
}

static uint32_t read_le32(const uint8_t *buffer)
{
    return (uint32_t)buffer[0]
         | ((uint32_t)buffer[1] << 8)
         | ((uint32_t)buffer[2] << 16)
         | ((uint32_t)buffer[3] << 24);
}

StegStatus payload_validate(const PayloadInfo *info)
{
    const uint8_t supported_flags =
        PAYLOAD_FLAG_FILENAME |
        PAYLOAD_FLAG_CRC32;

    if (info == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (info->version != PAYLOAD_VERSION)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    if (info->type != PAYLOAD_TYPE_TEXT &&
        info->type != PAYLOAD_TYPE_FILE)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    if ((info->flags & ~supported_flags) != 0U)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    if (info->payload_size == 0U)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    if ((info->flags & PAYLOAD_FLAG_FILENAME) != 0U)
    {
        if (info->filename_length == 0U)
        {
            return STEG_ERR_INVALID_PAYLOAD;
        }
    }
    else
    {
        if (info->filename_length != 0U)
        {
            return STEG_ERR_INVALID_PAYLOAD;
        }
    }

    return STEG_SUCCESS;
}

StegStatus payload_serialize_header(const PayloadInfo *info,
                                    uint8_t *buffer)
{
    if (info == NULL || buffer == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (payload_validate(info) != STEG_SUCCESS)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    buffer[0] = 'S';
    buffer[1] = 'T';
    buffer[2] = 'E';
    buffer[3] = 'G';

    buffer[4] = info->version;
    buffer[5] = info->flags;
    buffer[6] = info->type;

    write_le16(&buffer[7], info->filename_length);
    write_le32(&buffer[9], info->payload_size);
    write_le32(&buffer[13], info->crc32);

    return STEG_SUCCESS;
}

StegStatus payload_parse_header(const uint8_t *buffer,
                                PayloadInfo *info)
{
    if (buffer == NULL || info == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (buffer[0] != 'S' ||
        buffer[1] != 'T' ||
        buffer[2] != 'E' ||
        buffer[3] != 'G')
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    info->version = buffer[4];
    info->flags = buffer[5];
    info->type = buffer[6];

    info->filename_length =
        read_le16(&buffer[7]);

    info->payload_size =
        read_le32(&buffer[9]);

    info->crc32 =
        read_le32(&buffer[13]);

    return payload_validate(info);
}