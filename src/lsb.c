#include <stdint.h>
#include <stddef.h>
#include "lsb.h"

StegStatus lsb_embed_bit(uint8_t *byte, uint8_t bit)
{
    if (byte == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (bit > 1)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    *byte = (*byte & 0xFEU) | bit;

    return STEG_SUCCESS;
}

StegStatus lsb_extract_bit(uint8_t byte, uint8_t *bit)
{
    if (bit == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    *bit = byte & 0x01U;

    return STEG_SUCCESS;
}