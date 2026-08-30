#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "decoder.h"
#include "encoder.h"


/**
 * @brief Prints the program usage information.
 */
static void print_usage(const char *program_name)
{
    printf("\n");
    printf("BMP Steganography Tool\n");
    printf("\n");

    printf("Usage:\n");
    printf("  %s -e <input.bmp> <secret_file> <output.bmp>\n",
           program_name);
    printf("  %s -d <stego.bmp> <output_dir>\n",
           program_name);

    printf("\n");

    printf("Options:\n");
    printf("  -e    Encode a secret file into a BMP image\n");
    printf("  -d    Decode a secret file from a BMP image\n");

    printf("\n");

    printf("Examples:\n");
    printf("  %s -e input/beautiful.bmp input/secret.txt "
           "output/stego.bmp\n",
           program_name);
    printf("  %s -d output/stego.bmp output/\n",
           program_name);

    printf("\n");
}


/**
 * @brief Converts a StegStatus value into a readable message.
 *
 * @param status Status returned by the steganography modules.
 *
 * @return Constant string describing the status.
 */
static const char *status_message(StegStatus status)
{
    switch (status)
    {
        case STEG_SUCCESS:
            return "Success";

        case STEG_ERR_INVALID_ARGUMENT:
            return "Invalid argument";

        case STEG_ERR_MEMORY:
            return "Memory allocation failed";

        case STEG_ERR_FILE_OPEN:
            return "Unable to open file";

        case STEG_ERR_FILE_READ:
            return "File read error";

        case STEG_ERR_FILE_WRITE:
            return "File write error";

        case STEG_ERR_FILE_TOO_LARGE:
            return "File is too large";

        case STEG_ERR_INVALID_BMP:
            return "Invalid BMP file";

        case STEG_ERR_UNSUPPORTED_BMP:
            return "Unsupported BMP format";

        case STEG_ERR_INVALID_PAYLOAD:
            return "Invalid payload";

        case STEG_ERR_INSUFFICIENT_CAPACITY:
            return "Insufficient BMP capacity";

        case STEG_ERR_CRC_MISMATCH:
            return "CRC32 mismatch";

        default:
            return "Unknown error";
    }
}


/**
 * @brief Handles the encode operation.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 *
 * @return EXIT_SUCCESS on success, EXIT_FAILURE otherwise.
 */
static int handle_encode(int argc, char *argv[])
{
    StegStatus status;

    /*
     * Expected format:
     *
     * steg -e input.bmp secret_file output.bmp
     *
     * argc = 5
     */
    if (argc != 5)
    {
        fprintf(stderr,
                "Error: invalid number of arguments for encoding.\n");

        print_usage(argv[0]);

        return EXIT_FAILURE;
    }

    status = encode_file(argv[2],
                         argv[3],
                         argv[4]);

    if (status != STEG_SUCCESS)
    {
        fprintf(stderr,
                "Encoding failed: %s\n",
                status_message(status));

        return EXIT_FAILURE;
    }

    printf("Encoding successful.\n");
    printf("Carrier : %s\n", argv[2]);
    printf("Secret  : %s\n", argv[3]);
    printf("Output  : %s\n", argv[4]);

    return EXIT_SUCCESS;
}


/**
 * @brief Handles the decode operation.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 *
 * @return EXIT_SUCCESS on success, EXIT_FAILURE otherwise.
 */
static int handle_decode(int argc, char *argv[])
{
    StegStatus status;

    /*
     * Expected format:
     *
     * steg -d stego.bmp output_dir
     *
     * argc = 4
     */
    if (argc != 4)
    {
        fprintf(stderr,
                "Error: invalid number of arguments for decoding.\n");

        print_usage(argv[0]);

        return EXIT_FAILURE;
    }

    status = decode_file(argv[2],
                         argv[3]);

    if (status != STEG_SUCCESS)
    {
        fprintf(stderr,
                "Decoding failed: %s\n",
                status_message(status));

        return EXIT_FAILURE;
    }

    printf("Decoding successful.\n");
    printf("Input   : %s\n", argv[2]);
    printf("Output  : %s\n", argv[3]);

    return EXIT_SUCCESS;
}


/**
 * @brief Program entry point.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 *
 * @return EXIT_SUCCESS on success, EXIT_FAILURE otherwise.
 */
int main(int argc, char *argv[])
{
    /*
     * No command supplied.
     */
    if (argc < 2)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /*
     * Encoding operation.
     */
    if (strcmp(argv[1], "-e") == 0)
    {
        return handle_encode(argc, argv);
    }

    /*
     * Decoding operation.
     */
    if (strcmp(argv[1], "-d") == 0)
    {
        return handle_decode(argc, argv);
    }

    /*
     * Unknown operation.
     */
    fprintf(stderr,
            "Error: unknown option '%s'.\n",
            argv[1]);

    print_usage(argv[0]);

    return EXIT_FAILURE;
}