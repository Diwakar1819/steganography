#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Maintains the intermediate state of a CRC32 calculation.
 */
typedef struct
{
    uint32_t value;

} CRC32Context;

/**
 * @brief Initializes a CRC32 calculation context.
 *
 * @param context Pointer to the CRC32 context.
 */
void crc32_init(CRC32Context *context);

/**
 * @brief Updates a CRC32 calculation with a block of data.
 *
 * This function may be called repeatedly with consecutive
 * blocks of data.
 *
 * @param context Pointer to the CRC32 context.
 * @param data Pointer to the input bytes.
 * @param length Number of bytes in the input block.
 */
void crc32_update(CRC32Context *context,
                  const uint8_t *data,
                  size_t length);

/**
 * @brief Finalizes a CRC32 calculation.
 *
 * @param context Pointer to the CRC32 context.
 *
 * @return Final CRC32 checksum.
 */
uint32_t crc32_finalize(const CRC32Context *context);

/**
 * @brief Calculates CRC32 for a complete byte sequence.
 *
 * This is a convenience wrapper around init, update and finalize.
 *
 * @param data Pointer to the input bytes.
 * @param length Number of bytes.
 *
 * @return CRC32 checksum.
 */
uint32_t crc32_calculate(const uint8_t *data, size_t length);

#endif /* CRC32_H */