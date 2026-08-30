#include <stddef.h>
#include <stdint.h>

#include "crc32.h"

#define CRC32_POLYNOMIAL 0xEDB88320U
#define CRC32_INITIAL    0xFFFFFFFFU
#define CRC32_FINAL_XOR  0xFFFFFFFFU

void crc32_init(CRC32Context *context)
{
    if (context == NULL)
    {
        return;
    }

    context->value = CRC32_INITIAL;
}

void crc32_update(CRC32Context *context,
                  const uint8_t *data,
                  size_t length)
{
    if (context == NULL)
    {
        return;
    }

    if (data == NULL && length != 0U)
    {
        return;
    }

    for (size_t i = 0; i < length; i++)
    {
        context->value ^= data[i];

        for (unsigned int bit = 0; bit < 8U; bit++)
        {
            if ((context->value & 1U) != 0U)
            {
                context->value =
                    (context->value >> 1) ^
                    CRC32_POLYNOMIAL;
            }
            else
            {
                context->value >>= 1;
            }
        }
    }
}

uint32_t crc32_finalize(const CRC32Context *context)
{
    if (context == NULL)
    {
        return 0U;
    }

    return context->value ^ CRC32_FINAL_XOR;
}

uint32_t crc32_calculate(const uint8_t *data, size_t length)
{
    CRC32Context context;

    if (data == NULL && length != 0U)
    {
        return 0U;
    }

    crc32_init(&context);

    crc32_update(&context, data, length);

    return crc32_finalize(&context);
}