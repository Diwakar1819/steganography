#include <stdint.h>
#include <stdio.h>

#include "bmp.h"

static StegStatus read_le16(FILE *fp, uint16_t *value)
{
    uint8_t buffer[2];

    if (fp == NULL || value == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (fread(buffer, 1, sizeof(buffer), fp) != sizeof(buffer))
    {
        return STEG_ERR_FILE_READ;
    }

    *value = (uint16_t)buffer[0]
           | ((uint16_t)buffer[1] << 8);

    return STEG_SUCCESS;
}

static StegStatus read_le32(FILE *fp, uint32_t *value)
{
    uint8_t buffer[4];

    if (fp == NULL || value == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (fread(buffer, 1, sizeof(buffer), fp) != sizeof(buffer))
    {
        return STEG_ERR_FILE_READ;
    }

    *value = (uint32_t)buffer[0]
           | ((uint32_t)buffer[1] << 8)
           | ((uint32_t)buffer[2] << 16)
           | ((uint32_t)buffer[3] << 24);

    return STEG_SUCCESS;
}

static StegStatus read_le32_signed(FILE *fp, int32_t *value)
{
    uint32_t raw_value;
    StegStatus status;

    if (value == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    status = read_le32(fp, &raw_value);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    *value = (int32_t)raw_value;

    return STEG_SUCCESS;
}

StegStatus bmp_read_info(FILE *fp, BMPInfo *info)
{
    uint16_t signature;
    uint32_t dib_header_size;
    StegStatus status;

    if (fp == NULL || info == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    status = read_le16(fp, &signature);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    if (signature != 0x4D42)
    {
        return STEG_ERR_INVALID_BMP;
    }

    status = read_le32(fp, &info->file_size);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    if (fseek(fp, 4, SEEK_CUR) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    status = read_le32(fp, &info->pixel_offset);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    status = read_le32(fp, &dib_header_size);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    if (dib_header_size != 40)
    {
        return STEG_ERR_UNSUPPORTED_BMP;
    }

    status = read_le32_signed(fp, &info->width);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    status = read_le32_signed(fp, &info->height);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    status = read_le16(fp, &info->planes);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    status = read_le16(fp, &info->bits_per_pixel);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    status = read_le32(fp, &info->compression);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    status = read_le32(fp, &info->image_size);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    if (fseek(fp, 16, SEEK_CUR) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    {
        uint64_t raw_row_size;

        raw_row_size = (uint64_t)(uint32_t)info->width * 3U;

        info->row_size =
            (uint32_t)((raw_row_size + 3U) & ~3U);
    }

    return STEG_SUCCESS;
}

StegStatus bmp_validate(const BMPInfo *info)
{
    uint64_t expected_pixel_size;

    if (info == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (info->width <= 0 || info->height <= 0)
    {
        return STEG_ERR_INVALID_BMP;
    }

    if (info->planes != 1)
    {
        return STEG_ERR_UNSUPPORTED_BMP;
    }

    if (info->bits_per_pixel != 24)
    {
        return STEG_ERR_UNSUPPORTED_BMP;
    }

    if (info->compression != 0)
    {
        return STEG_ERR_UNSUPPORTED_BMP;
    }

    if (info->pixel_offset < 54)
    {
        return STEG_ERR_INVALID_BMP;
    }

    expected_pixel_size =
        (uint64_t)info->row_size *
        (uint64_t)info->height;

    if ((uint64_t)info->pixel_offset + expected_pixel_size >
        (uint64_t)info->file_size)
    {
        return STEG_ERR_INVALID_BMP;
    }

    return STEG_SUCCESS;
}

uint64_t bmp_get_capacity(const BMPInfo *info)
{
    uint64_t rgb_bytes;

    if (info == NULL)
    {
        return 0;
    }

    rgb_bytes =
        (uint64_t)info->width *
        (uint64_t)info->height *
        3U;

    return rgb_bytes;
}