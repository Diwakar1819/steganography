#ifndef BMP_H
#define BMP_H

#include <stdint.h>
#include <stdio.h>

#include "common.h"

/**
 * @brief Stores BMP information required by the steganography system.
 *
 * The structure contains metadata describing the carrier image.
 * Payload-specific information is intentionally kept separate.
 */
typedef struct
{
    uint32_t file_size;
    uint32_t pixel_offset;

    int32_t width;
    int32_t height;

    uint16_t planes;
    uint16_t bits_per_pixel;

    uint32_t compression;
    uint32_t image_size;

    uint32_t row_size;

} BMPInfo;

/**
 * @brief Reads BMP header information from an open file.
 *
 * The function parses the BMP file header and the supported DIB
 * header and stores the required information in BMPInfo.
 *
 * @param fp Open file stream positioned at the beginning of the BMP.
 * @param info Pointer to the structure that receives BMP information.
 *
 * @return STEG_SUCCESS on success, otherwise an appropriate error code.
 */
StegStatus bmp_read_info(FILE *fp, BMPInfo *info);

/**
 * @brief Validates whether the BMP is supported by this application.
 *
 * The current implementation supports 24-bit uncompressed BMP files
 * with valid dimensions, offsets and pixel-data boundaries.
 *
 * @param info Pointer to parsed BMP information.
 *
 * @return STEG_SUCCESS if the BMP is supported, otherwise an error code.
 */
StegStatus bmp_validate(const BMPInfo *info);

/**
 * @brief Calculates the LSB embedding capacity of a BMP.
 *
 * The capacity is calculated using the RGB bytes only. BMP row-padding
 * bytes are deliberately excluded from the embedding area.
 *
 * @param info Pointer to validated BMP information.
 *
 * @return Number of bits available for embedding.
 */
uint64_t bmp_get_capacity(const BMPInfo *info);

#endif /* BMP_H */