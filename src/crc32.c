#include <stddef.h>
#include <stdint.h>

#include "crc32.h"

uint32_t crc32_calculate(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFU;

    if (data == NULL && length != 0U)
    {
        return 0U;
    }

    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (unsigned int bit = 0; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}