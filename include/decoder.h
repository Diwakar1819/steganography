#ifndef DECODER_H
#define DECODER_H

#include "common.h"

/**
 * @brief Extracts a hidden file from a steganographic BMP image.
 *
 * The decoder reads the embedded payload metadata, extracts the
 * original filename and secret data, verifies the payload CRC32,
 * and writes the recovered file to the specified output directory.
 *
 * @param input_bmp Path to the steganographic BMP image.
 * @param output_dir Directory where the extracted file will be written.
 *
 * @return STEG_SUCCESS on success, otherwise an appropriate error code.
 */
StegStatus decode_file(const char *input_bmp,
                       const char *output_dir);

#endif /* DECODER_H */