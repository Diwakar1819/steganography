#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Calculates the CRC32 checksum of a byte sequence.
 *
 * The implementation uses the standard CRC-32 polynomial
 * and produces the conventional CRC32 result.
 *
 * @param data Pointer to the input byte sequence.
 * @param length Number of bytes in the input sequence.
 *
 * @return CRC32 checksum of the supplied data.
 */
uint32_t crc32_calculate(const uint8_t *data, size_t length);

#endif /* CRC32_H */