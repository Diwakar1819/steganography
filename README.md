# BMP Steganography

A command-line tool written in C that hides any file inside a 24-bit BMP image
using Least Significant Bit (LSB) steganography. The hidden file can later be
extracted back to its original form with full integrity verification.

## How It Works

Every pixel in a 24-bit BMP is stored as three bytes (Blue, Green, Red).
The **least significant bit** of each byte is nearly invisible to the human eye.
This tool replaces those bits with the bits of the secret file.

```
Original pixel byte:  1 0 1 1 0 1 1 0
                                    ^
                              LSB replaced
                                    |
Modified pixel byte:  1 0 1 1 0 1 1 1  ← secret bit embedded here
```

The image looks identical, but it now carries hidden data.

## Payload Format

The embedded data follows a structured format so that the decoder
knows exactly what was hidden:

```
 Bytes   Field
 ─────   ──────────────────
  4      Magic ("STEG")
  1      Version
  1      Flags
  1      Type
  2      Filename length
  4      Payload size
  4      CRC32 checksum
 ─────
 17      Fixed header
  N      Original filename
  M      Secret file data
```

The CRC32 checksum is calculated over the secret file before encoding.
During decoding, the CRC is recalculated and compared — if someone
tampered with the image, the mismatch is detected.

## Build

```bash
make
```

This produces the `steg` executable in the project root.

To clean build artifacts:

```bash
make clean
```

## Usage

### Encode (hide a file)

```bash
./steg -e <carrier.bmp> <secret_file> <output.bmp>
```

Example:

```bash
./steg -e input/beautiful.bmp input/secret.txt output/stego.bmp
```

### Decode (extract the file)

```bash
./steg -d <stego.bmp> <output_directory>
```

Example:

```bash
./steg -d output/stego.bmp output/
```

The original filename is preserved — it recreates the file with its
original name inside the output directory.

### Verify

```bash
cmp input/secret.txt output/secret.txt && echo "MATCH"
```

## Testing

A test script covers 43 test cases across 14 categories:

```bash
bash tests/run_tests.sh
```

Test categories include: compilation checks, text and binary roundtrips,
filename preservation, BMP row padding, insufficient capacity rejection,
CRC integrity detection, invalid input handling, empty files, malicious
paths, and large payloads crossing internal buffer boundaries.

## Project Structure

```
steganography/
├── include/            Header files
│   ├── common.h        Status codes used across the project
│   ├── bmp.h           BMP reading, validation, capacity calculation
│   ├── lsb.h           Single-bit embed and extract operations
│   ├── payload.h       Payload metadata structure and serialization
│   ├── crc32.h         CRC32 checksum calculation
│   ├── encoder.h       Encode interface (one public function)
│   └── decoder.h       Decode interface (one public function)
│
├── src/                Source files
│   ├── main.c          CLI entry point, argument parsing, -e and -d
│   ├── bmp.c           Reads BMP headers, validates format, row size
│   ├── lsb.c           Embeds/extracts one bit in a carrier byte
│   ├── payload.c       Serializes/parses the 17-byte payload header
│   ├── crc32.c         CRC32 implementation (bit-by-bit, no table)
│   ├── encoder.c       Orchestrates encoding: validate → build
│   │                   payload → embed header → embed filename →
│   │                   embed data → copy remaining BMP
│   └── decoder.c       Orchestrates decoding: validate → extract
│                       header → extract filename → extract data →
│                       verify CRC → write recovered file
│
├── input/              Input files
│   ├── beautiful.bmp   Sample 24-bit BMP carrier image
│   └── secret.txt      Sample secret file for testing
│
├── output/             Generated output (git-ignored)
│
├── tests/
│   └── run_tests.sh    Automated test suite (43 tests)
│
├── docs/               Documentation (reserved)
├── Makefile            Build configuration
├── .gitignore          Files excluded from version control
└── README.md           This file
```

## File Details

### `common.h`
Defines `StegStatus` — the return type used by every function in the
project. All errors have a named code (`STEG_ERR_FILE_OPEN`,
`STEG_ERR_CRC_MISMATCH`, etc.) so callers always know exactly what
went wrong.

### `bmp.h` / `bmp.c`
Reads the 14-byte BMP file header and 40-byte DIB header into a
`BMPInfo` structure. Validates that the image is 24-bit, uncompressed,
with sane dimensions. Calculates the row size (including padding) and
the total RGB byte capacity available for embedding.

### `lsb.h` / `lsb.c`
Two small functions: `lsb_embed_bit()` sets the LSB of a byte,
`lsb_extract_bit()` reads the LSB. This is the core steganographic
operation — everything else is orchestration around it.

### `payload.h` / `payload.c`
Defines the `PayloadInfo` metadata structure and handles its
serialization into a 17-byte binary header (little-endian). The header
includes a "STEG" magic marker, version number, flags, payload type,
filename length, data size, and CRC32. Parsing and validation happen
here too.

### `crc32.h` / `crc32.c`
Standard CRC32 implementation using the `0xEDB88320` polynomial.
Supports streaming (init → update → finalize) so large files can be
checksummed without loading them entirely into memory.

### `encoder.h` / `encoder.c`
The encoding pipeline: opens the carrier BMP, opens the secret file,
calculates file size and CRC32, builds the payload header, checks
that the BMP has enough capacity, then embeds the header + filename +
file data bit-by-bit into the pixel bytes. Row padding is copied
unchanged. Remaining BMP bytes after the payload are also copied
unchanged, so the output file is identical in size to the input.

### `decoder.h` / `decoder.c`
The decoding pipeline: opens the stego BMP, extracts the payload
header, validates the magic and metadata, extracts the filename
(with path-traversal protection), extracts the file data while
computing CRC32 on the fly, and verifies the checksum. If anything
fails, any partial output file is deleted.

### `main.c`
Command-line interface with two operations: `-e` for encoding and
`-d` for decoding. Prints human-readable error messages for all
status codes.

### `tests/run_tests.sh`
Automated bash test script that builds the project, generates test
files, runs encode/decode roundtrips, and verifies results. Tests
cover text, binary data, various BMP widths (row padding), capacity
limits, CRC corruption, invalid inputs, empty files, argument
handling, and large payloads.

## Limitations

- Only 24-bit uncompressed BMP with BITMAPINFOHEADER (40-byte DIB)
- Secret file size limited to ~4 GB (uint32_t)
- Secret file must not be empty
- 1 bit per pixel byte (no multi-bit embedding)
