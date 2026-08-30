#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "crc32.h"
#include "encoder.h"
#include "lsb.h"
#include "payload.h"

#define IO_BUFFER_SIZE 4096U


/*
 * Stores the current position within the BMP pixel array.
 *
 * pixel_bytes_processed:
 * Number of actual RGB pixel bytes already consumed.
 *
 * Row-padding bytes are deliberately excluded.
 */
typedef struct
{
    FILE *input;
    FILE *output;

    const BMPInfo *bmp;

    uint64_t pixel_bytes_processed;

} EmbedContext;


/**
 * @brief Determines the size of an opened file.
 *
 * The original file position is restored before returning.
 *
 * @param fp Open file stream.
 * @param size Destination for the file size.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus get_file_size(FILE *fp, uint32_t *size)
{
    long current_position;
    long end_position;

    if (fp == NULL || size == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    current_position = ftell(fp);

    if (current_position < 0)
    {
        return STEG_ERR_FILE_READ;
    }

    if (fseek(fp, 0L, SEEK_END) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    end_position = ftell(fp);

    if (end_position < 0)
    {
        return STEG_ERR_FILE_READ;
    }

    if ((unsigned long)end_position > UINT32_MAX)
    {
        return STEG_ERR_FILE_TOO_LARGE;
    }

    *size = (uint32_t)end_position;

    if (fseek(fp, current_position, SEEK_SET) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Calculates CRC32 for an entire file.
 *
 * The file is processed in fixed-size chunks so the entire
 * secret file does not need to be loaded into memory.
 *
 * @param fp Open file stream.
 * @param crc Destination for calculated CRC32.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus calculate_file_crc32(FILE *fp, uint32_t *crc)
{
    uint8_t buffer[IO_BUFFER_SIZE];
    size_t bytes_read;
    CRC32Context context;

    if (fp == NULL || crc == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    crc32_init(&context);

    while ((bytes_read =
            fread(buffer, 1U, sizeof(buffer), fp)) > 0U)
    {
        crc32_update(&context, buffer, bytes_read);
    }

    if (ferror(fp))
    {
        return STEG_ERR_FILE_READ;
    }

    *crc = crc32_finalize(&context);

    return STEG_SUCCESS;
}


/**
 * @brief Returns the filename component of a path.
 *
 * Handles both Unix-style '/' and Windows-style '\\'
 * path separators.
 *
 * @param path File path.
 *
 * @return Pointer to the filename portion of the path.
 */
static const char *get_basename(const char *path)
{
    const char *slash;
    const char *backslash;

    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');

    if (slash == NULL && backslash == NULL)
    {
        return path;
    }

    if (slash == NULL)
    {
        return backslash + 1;
    }

    if (backslash == NULL)
    {
        return slash + 1;
    }

    return (slash > backslash ? slash : backslash) + 1;
}


/**
 * @brief Copies a specified number of bytes between streams.
 *
 * @param input Source stream.
 * @param output Destination stream.
 * @param count Number of bytes to copy.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus copy_bytes(FILE *input,
                             FILE *output,
                             uint64_t count)
{
    uint8_t buffer[IO_BUFFER_SIZE];

    if (input == NULL || output == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    while (count > 0U)
    {
        size_t chunk_size;

        chunk_size =
            (count < IO_BUFFER_SIZE)
                ? (size_t)count
                : IO_BUFFER_SIZE;

        if (fread(buffer, 1U, chunk_size, input) != chunk_size)
        {
            return STEG_ERR_FILE_READ;
        }

        if (fwrite(buffer, 1U, chunk_size, output) != chunk_size)
        {
            return STEG_ERR_FILE_WRITE;
        }

        count -= chunk_size;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Calculates the number of usable RGB bytes in one BMP row.
 *
 * For a 24-bit BMP:
 *
 *     width × 3
 *
 * bytes contain actual BGR pixel data.
 *
 * Remaining bytes in row_size are padding and are not modified.
 *
 * @param bmp BMP metadata.
 *
 * @return Number of usable pixel bytes per row.
 */
static uint32_t get_row_pixel_bytes(const BMPInfo *bmp)
{
    return (uint32_t)bmp->width * 3U;
}


/**
 * @brief Copies BMP row padding without modifying it.
 *
 * @param context Embedding context.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus copy_row_padding(EmbedContext *context)
{
    uint32_t pixel_bytes;
    uint32_t padding_bytes;

    if (context == NULL || context->bmp == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    pixel_bytes = get_row_pixel_bytes(context->bmp);

    if (context->bmp->row_size < pixel_bytes)
    {
        return STEG_ERR_INVALID_BMP;
    }

    padding_bytes =
        context->bmp->row_size - pixel_bytes;

    return copy_bytes(context->input,
                      context->output,
                      padding_bytes);
}


/**
 * @brief Prepares the input/output streams for the next pixel byte.
 *
 * When the end of a BMP row is reached, its padding is copied
 * unchanged before embedding continues on the next row.
 *
 * @param context Embedding context.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus prepare_next_pixel_byte(EmbedContext *context)
{
    uint64_t row_pixel_bytes;
    uint64_t position_in_row;

    if (context == NULL || context->bmp == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    row_pixel_bytes =
        (uint64_t)get_row_pixel_bytes(context->bmp);

    if (row_pixel_bytes == 0U)
    {
        return STEG_ERR_INVALID_BMP;
    }

    position_in_row =
        context->pixel_bytes_processed %
        row_pixel_bytes;

    /*
     * If we have just finished a complete row,
     * copy the row padding before continuing.
     */
    if (position_in_row == 0U &&
        context->pixel_bytes_processed != 0U)
    {
        return copy_row_padding(context);
    }

    return STEG_SUCCESS;
}


/**
 * @brief Embeds one byte into eight BMP pixel bytes.
 *
 * The most-significant bit of the data byte is embedded first.
 *
 * @param context Embedding context.
 * @param data Byte to embed.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus embed_byte(EmbedContext *context,
                             uint8_t data)
{
    if (context == NULL ||
        context->input == NULL ||
        context->output == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    for (int bit_position = 7; bit_position >= 0; bit_position--)
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

        /*
         * Extract one bit from the secret byte.
         */
        bit = (uint8_t)((data >> bit_position) & 0x01U);

        /*
         * Replace the carrier byte's LSB.
         */
        status = lsb_embed_bit(&carrier, bit);

        if (status != STEG_SUCCESS)
        {
            return status;
        }

        /*
         * Write modified carrier byte to output.
         */
        if (fwrite(&carrier, 1U, 1U, context->output) != 1U)
        {
            return STEG_ERR_FILE_WRITE;
        }

        context->pixel_bytes_processed++;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Embeds a memory buffer into the BMP pixel data.
 *
 * @param context Embedding context.
 * @param data Buffer containing data to hide.
 * @param length Number of bytes to hide.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus embed_buffer(EmbedContext *context,
                               const uint8_t *data,
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

        status = embed_byte(context, data[i]);

        if (status != STEG_SUCCESS)
        {
            return status;
        }
    }

    return STEG_SUCCESS;
}


/**
 * @brief Embeds a secret file into the BMP pixel data.
 *
 * The secret file is processed in fixed-size chunks.
 *
 * @param context Embedding context.
 * @param secret_file Open secret-file stream.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus embed_file(EmbedContext *context,
                             FILE *secret_file)
{
    uint8_t buffer[IO_BUFFER_SIZE];
    size_t bytes_read;

    if (context == NULL || secret_file == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    if (fseek(secret_file, 0L, SEEK_SET) != 0)
    {
        return STEG_ERR_FILE_READ;
    }

    while ((bytes_read =
            fread(buffer, 1U, sizeof(buffer), secret_file)) > 0U)
    {
        StegStatus status;

        status = embed_buffer(context,
                              buffer,
                              bytes_read);

        if (status != STEG_SUCCESS)
        {
            return status;
        }
    }

    if (ferror(secret_file))
    {
        return STEG_ERR_FILE_READ;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Copies all remaining BMP data unchanged.
 *
 * @param context Embedding context.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
static StegStatus copy_remaining_bmp(EmbedContext *context)
{
    uint8_t buffer[IO_BUFFER_SIZE];
    size_t bytes_read;

    if (context == NULL ||
        context->input == NULL ||
        context->output == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    while ((bytes_read =
            fread(buffer, 1U, sizeof(buffer), context->input)) > 0U)
    {
        if (fwrite(buffer, 1U, bytes_read, context->output)
            != bytes_read)
        {
            return STEG_ERR_FILE_WRITE;
        }
    }

    if (ferror(context->input))
    {
        return STEG_ERR_FILE_READ;
    }

    return STEG_SUCCESS;
}


/**
 * @brief Embeds a secret file into a BMP carrier image.
 *
 * The encoder:
 *
 * 1. Validates input arguments.
 * 2. Opens and validates the BMP carrier.
 * 3. Opens the secret file.
 * 4. Determines secret-file size.
 * 5. Calculates secret-file CRC32.
 * 6. Builds payload metadata.
 * 7. Checks carrier capacity.
 * 8. Creates the output BMP.
 * 9. Copies the BMP header unchanged.
 * 10. Embeds payload metadata.
 * 11. Embeds the filename.
 * 12. Embeds the secret file.
 * 13. Copies all remaining BMP bytes unchanged.
 * 14. Cleans up all resources.
 *
 * @param input_bmp Path to the source BMP carrier.
 * @param secret_file Path to the file being hidden.
 * @param output_bmp Path for the resulting stego BMP.
 *
 * @return STEG_SUCCESS on success, otherwise an error code.
 */
StegStatus encode_file(const char *input_bmp,
                       const char *secret_file,
                       const char *output_bmp)
{
    FILE *bmp_input = NULL;
    FILE *secret_input = NULL;
    FILE *bmp_output = NULL;

    BMPInfo bmp;
    PayloadInfo payload;
    EmbedContext context;

    uint8_t header[PAYLOAD_HEADER_SIZE];

    uint32_t secret_size;
    uint32_t secret_crc;

    const char *filename;
    size_t filename_length;

    uint64_t total_payload_bytes;
    uint64_t required_bits;
    uint64_t capacity;

    StegStatus status = STEG_SUCCESS;

    /*
     * Validate required arguments.
     */
    if (input_bmp == NULL ||
        secret_file == NULL ||
        output_bmp == NULL)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Prevent overwriting the carrier.
     */
    if (strcmp(input_bmp, output_bmp) == 0)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Prevent destroying the secret file.
     */
    if (strcmp(secret_file, output_bmp) == 0)
    {
        return STEG_ERR_INVALID_ARGUMENT;
    }

    /*
     * Open carrier BMP for binary reading.
     */
    bmp_input = fopen(input_bmp, "rb");

    if (bmp_input == NULL)
    {
        status = STEG_ERR_FILE_OPEN;
        goto cleanup;
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
     * Open secret file for binary reading.
     */
    secret_input = fopen(secret_file, "rb");

    if (secret_input == NULL)
    {
        status = STEG_ERR_FILE_OPEN;
        goto cleanup;
    }

    /*
     * Determine secret-file size.
     */
    status = get_file_size(secret_input, &secret_size);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Empty files are not accepted as payloads.
     */
    if (secret_size == 0U)
    {
        status = STEG_ERR_INVALID_PAYLOAD;
        goto cleanup;
    }

    /*
     * First pass:
     * calculate CRC32 of the secret file.
     */
    status = calculate_file_crc32(secret_input,
                                  &secret_crc);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Store only the filename, not its directory.
     */
    filename = get_basename(secret_file);
    filename_length = strlen(filename);

    if (filename_length == 0U ||
        filename_length > UINT16_MAX)
    {
        status = STEG_ERR_INVALID_PAYLOAD;
        goto cleanup;
    }

    /*
     * Construct payload metadata.
     */
    payload.version = PAYLOAD_VERSION;

    payload.flags =
        PAYLOAD_FLAG_FILENAME |
        PAYLOAD_FLAG_CRC32;

    payload.type = PAYLOAD_TYPE_FILE;

    payload.filename_length =
        (uint16_t)filename_length;

    payload.payload_size =
        secret_size;

    payload.crc32 =
        secret_crc;

    /*
     * Validate metadata before modifying the output.
     */
    status = payload_validate(&payload);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Calculate total payload size:
     *
     *     header
     *       +
     *     filename
     *       +
     *     secret data
     */
    total_payload_bytes =
        (uint64_t)PAYLOAD_HEADER_SIZE +
        (uint64_t)filename_length +
        (uint64_t)secret_size;

    /*
     * One hidden bit occupies one carrier pixel byte.
     */
    required_bits =
        total_payload_bytes * 8U;

    /*
     * Check whether the BMP has enough usable
     * RGB pixel bytes.
     */
    capacity = bmp_get_capacity(&bmp);

    if (required_bits > capacity)
    {
        status = STEG_ERR_INSUFFICIENT_CAPACITY;
        goto cleanup;
    }

    /*
     * Create output BMP.
     */
    bmp_output = fopen(output_bmp, "wb");

    if (bmp_output == NULL)
    {
        status = STEG_ERR_FILE_OPEN;
        goto cleanup;
    }

    /*
     * Rewind carrier to its beginning.
     */
    if (fseek(bmp_input, 0L, SEEK_SET) != 0)
    {
        status = STEG_ERR_FILE_READ;
        goto cleanup;
    }

    /*
     * Copy BMP header and all data before
     * the pixel array unchanged.
     */
    status = copy_bytes(bmp_input,
                        bmp_output,
                        bmp.pixel_offset);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Initialize embedding context.
     *
     * Both streams are now positioned at the
     * beginning of the BMP pixel array.
     */
    context.input = bmp_input;
    context.output = bmp_output;
    context.bmp = &bmp;
    context.pixel_bytes_processed = 0U;

    /*
     * Serialize payload metadata.
     */
    status = payload_serialize_header(&payload,
                                      header);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Embed payload header.
     */
    status = embed_buffer(&context,
                          header,
                          PAYLOAD_HEADER_SIZE);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Embed original filename.
     */
    status = embed_buffer(&context,
                          (const uint8_t *)filename,
                          filename_length);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Second pass:
     * embed the actual secret-file contents.
     */
    status = embed_file(&context,
                        secret_input);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * Copy every remaining carrier byte unchanged.
     *
     * This includes:
     * - unused pixel bytes
     * - row padding
     * - any remaining BMP data
     */
    status = copy_remaining_bmp(&context);

    if (status != STEG_SUCCESS)
    {
        goto cleanup;
    }

    status = STEG_SUCCESS;

cleanup:

    if (bmp_input != NULL)
    {
        fclose(bmp_input);
    }

    if (secret_input != NULL)
    {
        fclose(secret_input);
    }

    if (bmp_output != NULL)
    {
        fclose(bmp_output);
    }

    /*
     * If encoding failed after creating the output,
     * remove the incomplete/corrupted file.
     */
    if (status != STEG_SUCCESS)
    {
        remove(output_bmp);
    }

    return status;
}