#ifndef COMMON_H
#define COMMON_H

/*
 * @brief Represents the result of an operation in the
 *        steganography application.
 *
 * A value of STEG_SUCCESS indicates successful execution.
 * All other values represent a specific failure condition.
 */
typedef enum
{
    STEG_SUCCESS = 0,

    /* General errors */
    STEG_ERR_INVALID_ARGUMENT,
    STEG_ERR_MEMORY,

    /* File-related errors */
    STEG_ERR_FILE_OPEN,
    STEG_ERR_FILE_READ,
    STEG_ERR_FILE_WRITE,

    /* BMP-related errors */
    STEG_ERR_INVALID_BMP,
    STEG_ERR_UNSUPPORTED_BMP,
    STEG_ERR_INSUFFICIENT_CAPACITY,

    /* Payload-related errors */
    STEG_ERR_INVALID_PAYLOAD,
    STEG_ERR_CHECKSUM_MISMATCH,
    STEG_ERR_FILE_TOO_LARGE

} StegStatus;

#endif /* COMMON_H */