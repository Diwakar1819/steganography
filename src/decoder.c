#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "crc32.h"
#include "decoder.h"
#include "lsb.h"
#include "payload.h"

#define IO_BUFFER_SIZE 4096U
#define MAX_OUTPUT_PATH 4096U


/*
 * Maintains the current position inside the BMP pixel array.
 *
 * pixel_bytes_processed counts only actual RGB bytes.
 * Row-padding bytes are deliberately excluded because they
 * are never used to store steganographic data.
 */
typedef struct
{
    FILE *input;

    const BMPInfo *bmp;

    uint64_t pixel_bytes_processed;

} ExtractContext;


/**
 * @brief Skips BMP row-padding bytes.
 *
 * A 24-bit BMP row may contain padding bytes after its RGB
 * pixel data. These bytes are not part of the steganographic
 * carrier and must therefore be skipped during extraction.
 *
 * @param context Extraction context.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus skip_row_padding(ExtractContext *context)
{
    uint32_t pixel_bytes;
    uint32_t padding_bytes;

    if (context == NULL || context->bmp == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    pixel_bytes = (uint32_t)context->bmp->width * 3U;

    if (context->bmp->row_size < pixel_bytes)
    {
        return STEG_ERR_INVALID_BMP;
    }

    padding_bytes =
        context->bmp->row_size - pixel_bytes;

    if (padding_bytes == 0U)
    {
        return STEG_SUCCESS;
    }

    if (fseek(context->input,
              (long)padding_bytes,
              SEEK_CUR) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Prepares the input stream for the next pixel byte.
 *
 * When the current row has been completely processed, the
 * padding belonging to that row is skipped before continuing.
 *
 * @param context Extraction context.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus prepare_next_pixel_byte(ExtractContext *context)
{
    uint64_t row_pixel_bytes;
    uint64_t position_in_row;

    if (context == NULL || context->bmp == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    row_pixel_bytes =
        (uint64_t)(uint32_t)context->bmp->width * 3U;

    if (row_pixel_bytes == 0U)
    {
        return STEG_ERR_INVALID_BMP;
    }

    position_in_row =
        context->pixel_bytes_processed %
        row_pixel_bytes;

    /*
     * We have reached the beginning of a new row.
     * Skip the padding belonging to the previous row.
     */
    if (position_in_row == 0U &&
        context->pixel_bytes_processed != 0U)
    {
        return skip_row_padding(context);
    }

    return STEG_SUCCESS;
}


/**
 * @brief Extracts one hidden byte from eight BMP pixel bytes.
 *
 * The encoder stores the most-significant bit first.
 * Therefore the decoder reconstructs the byte by repeatedly
 * shifting the result left and appending the extracted LSB.
 *
 * @param context Extraction context.
 * @param data Destination for extracted byte.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus extract_byte(ExtractContext *context,
                               uint8_t *data)
{
    uint8_t result = 0U;

    if (context == NULL ||
        context->input == NULL ||
        data == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    for (unsigned int bit_position = 0U;
         bit_position < 8U;
         bit_position++)
    {
        uint8_t carrier;
        uint8_t bit;
        StegStatus status;

        status = prepare_next_pixel_byte(context);

        if (status != STEG_SUCCESS)
        {
            return status;
        }

        if (fread(&carrier, 1U, 1U, context->input) != 1U)
        {
            return STEG_ERR_FILE_READ;
        }

        status = lsb_extract_bit(carrier, &bit);

        if (status != STEG_SUCCESS)
        {
            return status;
        }

        /*
         * Reconstruct the hidden byte.
         *
         * Example:
         *
         * result = 00000000
         *
         * after reading bits 1,0,1:
         *
         * result = 00000101
         */
        result =
            (uint8_t)((result << 1U) | bit);

        context->pixel_bytes_processed++;
    }

    *data = result;

    return STEG_SUCCESS;
}


/**
 * @brief Extracts multiple hidden bytes from the BMP.
 *
 * @param context Extraction context.
 * @param data Destination buffer.
 * @param length Number of bytes to extract.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus extract_buffer(ExtractContext *context,
                                 uint8_t *data,
                                 size_t length)
{
    if (context == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (data == NULL && length != 0U)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    for (size_t i = 0U; i < length; i++)
    {
        StegStatus status;

        status = extract_byte(context, &data[i]);

        if (status != STEG_SUCCESS)
        {
            return status;
        }
    }

    return STEG_SUCCESS;
}


/**
 * @brief Builds the output path for the recovered file.
 *
 * The embedded filename is treated only as a filename.
 * Directory traversal components are rejected.
 *
 * @param output_dir Destination directory.
 * @param filename Embedded filename.
 * @param output_path Destination path buffer.
 * @param output_path_size Size of destination buffer.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus build_output_path(const char *output_dir,
                                    const char *filename,
                                    char *output_path,
                                    size_t output_path_size)
{
    size_t directory_length;
    size_t filename_length;
    size_t required_length;

    if (output_dir == NULL ||
        filename == NULL ||
        output_path == NULL ||
        output_path_size == 0U)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Reject path separators in the embedded filename.
     *
     * This prevents values such as:
     *
     *     ../../secret.txt
     *
     * from escaping the requested output directory.
     */
    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    directory_length = strlen(output_dir);
    filename_length = strlen(filename);

    if (filename_length == 0U)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * Check the addition safely before calculating the
     * required length.
     *
     * Required space:
     *
     *     directory
     *     separator
     *     filename
     *     terminating '\0'
     */
    if (directory_length >
        output_path_size - 1U)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (filename_length >
        output_path_size - directory_length - 1U)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    required_length =
        directory_length +
        filename_length +
        2U;

    if (required_length > output_path_size)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Empty output directory means the filename itself
     * becomes the output path.
     */
    if (directory_length == 0U)
    {
        if (snprintf(output_path,
                     output_path_size,
                     "%s",
                     filename) < 0)
        {
            return STEG_ERR_INVALID_ARGUMENT;
        }

        return STEG_SUCCESS;
    }

    /*
     * Avoid adding a second separator if the directory
     * already ends with one.
     */
    if (output_dir[directory_length - 1U] == '/' ||
        output_dir[directory_length - 1U] == '\\')
    {
        if (snprintf(output_path,
                     output_path_size,
                     "%s%s",
                     output_dir,
                     filename) < 0)
        {
            return STEG_ERR_INVALID_ARGUMENT;
        }
    }
    else
    {
        if (snprintf(output_path,
                     output_path_size,
                     "%s/%s",
                     output_dir,
                     filename) < 0)
        {
            return STEG_ERR_INVALID_ARGUMENT;
        }
    }

    return STEG_SUCCESS;
}


/**
 * @brief Extracts and validates the payload header and filename.
 *
 * This function performs metadata validation before any output
 * file is created.
 *
 * @param context Extraction context.
 * @param payload Destination payload metadata.
 * @param filename Destination filename buffer.
 * @param filename_size Size of filename buffer.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus extract_payload(ExtractContext *context,
                                  PayloadInfo *payload,
                                  char *filename,
                                  size_t filename_size)
{
    uint8_t header[PAYLOAD_HEADER_SIZE];
    uint64_t total_payload_bytes;
    uint64_t required_bits;
    uint64_t capacity;

    StegStatus status;

    if (context == NULL ||
        payload == NULL ||
        filename == NULL ||
        context->bmp == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Make sure the BMP has enough capacity even for
     * the minimum payload header.
     */
    capacity = bmp_get_capacity(context->bmp);

    if (capacity <
        (uint64_t)PAYLOAD_HEADER_SIZE * 8U)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * Extract the fixed-size payload header.
     */
    status =
        extract_buffer(context,
                       header,
                       PAYLOAD_HEADER_SIZE);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    /*
     * Convert serialized bytes into PayloadInfo.
     *
     * payload_parse_header() also checks the "STEG" magic
     * and validates the basic payload fields.
     */
    status =
        payload_parse_header(header,
                             payload);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    /*
     * Validate payload metadata explicitly.
     */
    status = payload_validate(payload);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    /*
     * This decoder extracts files, not generic text payloads.
     */
    if (payload->type != PAYLOAD_TYPE_FILE)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * This decoder always performs CRC verification.
     * Therefore the CRC32 flag must be present.
     */
    if ((payload->flags & PAYLOAD_FLAG_CRC32) == 0U)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * Make sure filename fits in the local buffer,
     * including the terminating '\0'.
     */
    if ((size_t)payload->filename_length + 1U >
        filename_size)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * Before extracting any more data, verify that the
     * complete declared payload fits inside the carrier.
     *
     * Required:
     *
     *     header
     *       +
     *     filename
     *       +
     *     payload
     *
     * Each payload byte consumes eight carrier bytes.
     */
    total_payload_bytes =
        (uint64_t)PAYLOAD_HEADER_SIZE +
        (uint64_t)payload->filename_length +
        (uint64_t)payload->payload_size;

    /*
     * Protect the multiplication from overflowing uint64_t.
     */
    if (total_payload_bytes >
        UINT64_MAX / 8U)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    required_bits =
        total_payload_bytes * 8U;

    if (required_bits > capacity)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * Extract embedded filename.
     */
    status =
        extract_buffer(context,
                       (uint8_t *)filename,
                       payload->filename_length);

    if (status != STEG_SUCCESS)
    {
        return status;
    }

    filename[payload->filename_length] = '\0';

    /*
     * Reject path traversal and path separators.
     */
    if (strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    /*
     * Reject special path components.
     */
    if (strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0)
    {
        return STEG_ERR_INVALID_PAYLOAD;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Extracts the secret file data and calculates CRC32.
 *
 * Data is processed in fixed-size chunks. The complete secret
 * file is never loaded into memory.
 *
 * @param context Extraction context.
 * @param payload Payload metadata.
 * @param output_file Destination file stream.
 * @param calculated_crc Destination for calculated CRC32.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus extract_file_data(ExtractContext *context,
                                    const PayloadInfo *payload,
                                    FILE *output_file,
                                    uint32_t *calculated_crc)
{
    uint8_t buffer[IO_BUFFER_SIZE];
    uint32_t remaining;
    CRC32Context crc_context;

    if (context == NULL ||
        payload == NULL ||
        output_file == NULL ||
        calculated_crc == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    remaining = payload->payload_size;

    crc32_init(&crc_context);

    while (remaining > 0U)
    {
        size_t chunk_size;
        StegStatus status;

        chunk_size =
            (remaining < IO_BUFFER_SIZE)
                ? (size_t)remaining
                : IO_BUFFER_SIZE;

        /*
         * Extract one chunk from the BMP.
         */
        status =
            extract_buffer(context,
                           buffer,
                           chunk_size);

        if (status != STEG_SUCCESS)
        {
            return status;
        }

        /*
         * Write extracted bytes to the destination file.
         */
        if (fwrite(buffer,
                   1U,
                   chunk_size,
                   output_file) != chunk_size)
        {
            return STEG_ERR_FILE_WRITE;
        }

        /*
         * Update CRC using the extracted bytes.
         */
        crc32_update(&crc_context,
                     buffer,
                     chunk_size);

        remaining -= (uint32_t)chunk_size;
    }

    *calculated_crc =
        crc32_finalize(&crc_context);

    return STEG_SUCCESS;
}


/**
 * @brief Extracts a hidden file from a steganographic BMP.
 *
 * Processing flow:
 *
 *     BMP
 *      ↓
 *     BMP validation
 *      ↓
 *     payload header
 *      ↓
 *     payload validation
 *      ↓
 *     filename extraction
 *      ↓
 *     payload extraction
 *      ↓
 *     CRC32 verification
 *      ↓
 *     recovered file
 *
 * The recovered file is deleted if any stage fails.
 *
 * @param input_bmp Path to the steganographic BMP.
 * @param output_dir Directory where the recovered file is written.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
StegStatus decode_file(const char *input_bmp,
                       const char *output_dir)
{
    FILE *bmp_input = NULL;
    FILE *output_file = NULL;

    BMPInfo bmp;
    PayloadInfo payload;
    ExtractContext context;

    char filename[UINT16_MAX + 1U];
    char output_path[MAX_OUTPUT_PATH] = {0};

    uint32_t calculated_crc;

    StegStatus status;

    /*
     * Validate public arguments.
     */
    if (input_bmp == NULL ||
        output_dir == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Open steganographic BMP.
     */
    bmp_input = fopen(input_bmp, "rb");

    if (bmp_input == NULL)
    {
        return STEG_ERR_FILE_OPEN;
    }

    /*
     * Read BMP metadata.
     */
    status = bmp_read_info(bmp_input, &bmp);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Validate supported BMP format.
     */
    status = bmp_validate(&bmp);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Move to the beginning of the BMP pixel array.
     */
    if (fseek(bmp_input,
              (long)bmp.pixel_offset,
              SEEK_SET) != 0)
    {
        status = STEG_ERR_FILE_READ;
        goto cleanup;
    }

    /*
     * Initialize extraction context.
     */
    context.input = bmp_input;
    context.bmp = &bmp;
    context.pixel_bytes_processed = 0U;

    /*
     * Extract and validate payload metadata.
     *
     * No output file is created until the metadata and
     * filename have been validated.
     */
    status =
        extract_payload(&context,
                        &payload,
                        filename,
                        sizeof(filename));

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Construct safe output path.
     */
    status =
        build_output_path(output_dir,
                          filename,
                          output_path,
                          sizeof(output_path));

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Create the recovered file.
     */
    output_file = fopen(output_path, "wb");

    if (output_file == NULL)
    {
        status = STEG_ERR_FILE_OPEN;
        goto cleanup;
    }

    /*
     * Extract payload data and calculate CRC32.
     */
    status =
        extract_file_data(&context,
                          &payload,
                          output_file,
                          &calculated_crc);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Flush all buffered output.
     */
    if (fflush(output_file) != 0)
    {
        status = STEG_ERR_FILE_WRITE;
        goto cleanup;
    }

    /*
     * The file is considered valid only if its calculated
     * CRC32 matches the CRC32 stored in the payload header.
     */
    if (calculated_crc != payload.crc32)
    {
        status = STEG_ERR_CRC_MISMATCH;
        goto cleanup;
    }

    status = STEG_SUCCESS;

cleanup:

    if (bmp_input != NULL)
    {
        fclose(bmp_input);
    }

    if (output_file != NULL)
    {
        fclose(output_file);
    }

    /*
     * Never leave a partial/corrupted recovered file.
     *
     * output_file != NULL guarantees that the output file
     * was actually created.
     */
    if (status != STEG_SUCCESS &&
        output_file != NULL)
    {
        remove(output_path);
    }

    return status;
}