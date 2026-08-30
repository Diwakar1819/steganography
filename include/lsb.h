#ifndef LSB_H
#define LSB_H

#include <stdint.h>

#include "common.h"

/**
 * @brief Embeds a single bit into the least significant bit
 *        of a carrier byte.
 *
 * @param byte Pointer to the carrier byte.
 * @param bit Bit value to embed. Must be 0 or 1.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
StegStatus lsb_embed_bit(uint8_t *byte, uint8_t bit);

/**
 * @brief Extracts the least significant bit from a carrier byte.
 *
 * @param byte Carrier byte.
 * @param bit Pointer where the extracted bit is stored.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
StegStatus lsb_extract_bit(uint8_t byte, uint8_t *bit);

#endif /* LSB_H */