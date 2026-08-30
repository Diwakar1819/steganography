#ifndef ENCODER_H
#define ENCODER_H

#include "common.h"

/**
 * @brief Embeds a secret file into a BMP carrier image.
 *
 * The encoder reads the carrier BMP, validates its format and capacity,
 * constructs the payload metadata, calculates the payload CRC32 and
 * embeds the resulting payload using LSB steganography.
 *
 * @param input_bmp Path to the source BMP carrier image.
 * @param secret_file Path to the file that will be hidden.
 * @param output_bmp Path where the resulting stego BMP will be written.
 *
 * @return STEG_SUCCESS on success, otherwise an appropriate error code.
 */
StegStatus encode_file(const char *input_bmp,
                       const char *secret_file,
                       const char *output_bmp);

#endif /* ENCODER_H */