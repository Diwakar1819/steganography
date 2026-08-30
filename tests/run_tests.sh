#!/bin/bash

#
# Comprehensive test suite for the BMP steganography tool.
#
# Usage:
#     cd <project_root>
#     bash tests/run_tests.sh
#

set -euo pipefail

# ──────────────────────────────────────────────────────────────
# Configuration
# ──────────────────────────────────────────────────────────────

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
STEG="${PROJECT_DIR}/steg"
TEST_DIR="${PROJECT_DIR}/tests/tmp"
CARRIER="${PROJECT_DIR}/input/beautiful.bmp"

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

# ──────────────────────────────────────────────────────────────
# Colours (disabled when stdout is not a terminal)
# ──────────────────────────────────────────────────────────────

if [ -t 1 ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[0;33m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    RESET='\033[0m'
else
    GREEN=''
    RED=''
    YELLOW=''
    CYAN=''
    BOLD=''
    RESET=''
fi

# ──────────────────────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────────────────────

pass() {
    printf "${GREEN}  PASS${RESET}  %s\n" "$1"
    PASS_COUNT=$((PASS_COUNT + 1))
}

fail() {
    printf "${RED}  FAIL${RESET}  %s\n" "$1"
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

skip() {
    printf "${YELLOW}  SKIP${RESET}  %s\n" "$1"
    SKIP_COUNT=$((SKIP_COUNT + 1))
}

section() {
    printf "\n${CYAN}${BOLD}── %s ──${RESET}\n" "$1"
}

cleanup_test_dir() {
    rm -rf "${TEST_DIR}"
    mkdir -p "${TEST_DIR}/decode_out"
}

#
# create_bmp <path> <width> <height>
#
# Generates a minimal valid 24-bit uncompressed BMP.
# Pixels are filled with 0x80 so LSB embedding is visible.
#
create_bmp() {
    local path="$1"
    local width="$2"
    local height="$3"

    python3 - "$path" "$width" "$height" <<'PYEOF'
import struct, sys

path   = sys.argv[1]
width  = int(sys.argv[2])
height = int(sys.argv[3])

row_bytes   = width * 3
padding     = (4 - (row_bytes % 4)) % 4
row_size    = row_bytes + padding
pixel_size  = row_size * height
file_size   = 54 + pixel_size

with open(path, "wb") as f:
    # BMP file header (14 bytes)
    f.write(b"BM")
    f.write(struct.pack("<I", file_size))   # file size
    f.write(struct.pack("<HH", 0, 0))       # reserved
    f.write(struct.pack("<I", 54))           # pixel data offset

    # DIB header - BITMAPINFOHEADER (40 bytes)
    f.write(struct.pack("<I", 40))           # header size
    f.write(struct.pack("<i", width))        # width
    f.write(struct.pack("<i", height))       # height
    f.write(struct.pack("<HH", 1, 24))       # planes, bpp
    f.write(struct.pack("<I", 0))            # compression
    f.write(struct.pack("<I", pixel_size))   # image size
    f.write(struct.pack("<i", 2835))         # h-res
    f.write(struct.pack("<i", 2835))         # v-res
    f.write(struct.pack("<I", 0))            # colours used
    f.write(struct.pack("<I", 0))            # important colours

    # Pixel data
    row_data = bytes([0x80] * row_bytes) + bytes(padding)
    for _ in range(height):
        f.write(row_data)
PYEOF
}

# ──────────────────────────────────────────────────────────────
# 1. Compile
# ──────────────────────────────────────────────────────────────

section "1. Compilation"

# Normal build
(cd "${PROJECT_DIR}" && make clean >/dev/null 2>&1 && make >/dev/null 2>&1) \
    && pass "Normal compilation (gcc -Wall -Wextra -Werror -std=c11)" \
    || fail "Normal compilation"

# Strict build
(cd "${PROJECT_DIR}" && make clean >/dev/null 2>&1 && \
 make CFLAGS="-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -std=c11 -Iinclude" \
 >/dev/null 2>&1) \
    && pass "Strict compilation (-Wconversion -Wshadow)" \
    || fail "Strict compilation (-Wconversion -Wshadow)"

# Rebuild with normal flags for the rest of the tests
(cd "${PROJECT_DIR}" && make clean >/dev/null 2>&1 && make >/dev/null 2>&1)

if [ ! -x "${STEG}" ]; then
    echo "FATAL: steg binary not found at ${STEG}"
    exit 1
fi

# ──────────────────────────────────────────────────────────────
# 2. Encode a small text file
# ──────────────────────────────────────────────────────────────

section "2. Encode a small text file"

cleanup_test_dir

echo "Hello, steganography!" > "${TEST_DIR}/hello.txt"

if "${STEG}" -e "${CARRIER}" "${TEST_DIR}/hello.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    pass "Encode small text file"
else
    fail "Encode small text file"
fi

# ──────────────────────────────────────────────────────────────
# 3. Decode it
# ──────────────────────────────────────────────────────────────

section "3. Decode it"

if "${STEG}" -d "${TEST_DIR}/stego.bmp" \
   "${TEST_DIR}/decode_out" >/dev/null 2>&1; then
    pass "Decode stego BMP"
else
    fail "Decode stego BMP"
fi

# ──────────────────────────────────────────────────────────────
# 4. Compare original vs recovered
# ──────────────────────────────────────────────────────────────

section "4. Compare original vs recovered (cmp)"

DECODED_FILE="${TEST_DIR}/decode_out/hello.txt"

if [ -f "${DECODED_FILE}" ]; then
    if cmp -s "${TEST_DIR}/hello.txt" "${DECODED_FILE}"; then
        pass "Recovered file matches original (cmp)"
    else
        fail "Recovered file differs from original"
    fi
else
    fail "Recovered file not found at ${DECODED_FILE}"
fi

# ──────────────────────────────────────────────────────────────
# 5. Binary data (not only text)
# ──────────────────────────────────────────────────────────────

section "5. Binary data"

cleanup_test_dir

# Generate 256 bytes of binary data (all byte values 0x00–0xFF)
python3 -c "import sys; sys.stdout.buffer.write(bytes(range(256)))" \
    > "${TEST_DIR}/binary.dat"

if "${STEG}" -e "${CARRIER}" "${TEST_DIR}/binary.dat" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    pass "Encode binary data"
else
    fail "Encode binary data"
fi

if "${STEG}" -d "${TEST_DIR}/stego.bmp" \
   "${TEST_DIR}/decode_out" >/dev/null 2>&1; then
    pass "Decode binary data"
else
    fail "Decode binary data"
fi

if cmp -s "${TEST_DIR}/binary.dat" \
   "${TEST_DIR}/decode_out/binary.dat"; then
    pass "Binary roundtrip matches (cmp)"
else
    fail "Binary roundtrip mismatch"
fi

# ──────────────────────────────────────────────────────────────
# 6. Filename preservation
# ──────────────────────────────────────────────────────────────

section "6. Filename preservation"

cleanup_test_dir

echo "test data" > "${TEST_DIR}/my_report_2024.csv"

"${STEG}" -e "${CARRIER}" "${TEST_DIR}/my_report_2024.csv" \
    "${TEST_DIR}/stego.bmp" >/dev/null 2>&1

"${STEG}" -d "${TEST_DIR}/stego.bmp" \
    "${TEST_DIR}/decode_out" >/dev/null 2>&1

if [ -f "${TEST_DIR}/decode_out/my_report_2024.csv" ]; then
    pass "Filename preserved: my_report_2024.csv"
else
    fail "Filename not preserved (expected my_report_2024.csv)"
fi

# ──────────────────────────────────────────────────────────────
# 7. BMP row padding (widths that require padding)
# ──────────────────────────────────────────────────────────────

section "7. BMP row padding"

# Width=1: row_bytes=3, padding=1
# Width=2: row_bytes=6, padding=2
# Width=3: row_bytes=9, padding=3
# Width=4: row_bytes=12, padding=0

for w in 1 2 3 4 5 7 10; do
    cleanup_test_dir

    PADDED_BMP="${TEST_DIR}/pad_w${w}.bmp"
    create_bmp "${PADDED_BMP}" "$w" 100

    echo "padding test w=${w}" > "${TEST_DIR}/pad_secret.txt"

    if "${STEG}" -e "${PADDED_BMP}" "${TEST_DIR}/pad_secret.txt" \
       "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then

        "${STEG}" -d "${TEST_DIR}/stego.bmp" \
            "${TEST_DIR}/decode_out" >/dev/null 2>&1

        if cmp -s "${TEST_DIR}/pad_secret.txt" \
           "${TEST_DIR}/decode_out/pad_secret.txt"; then
            pass "Row padding: width=${w} (padding=$((( 4 - (w * 3 % 4) ) % 4)) bytes)"
        else
            fail "Row padding: width=${w} roundtrip mismatch"
        fi
    else
        # Small BMPs may lack capacity; that's acceptable
        skip "Row padding: width=${w} (insufficient capacity)"
    fi
done

# ──────────────────────────────────────────────────────────────
# 8. Insufficient capacity
# ──────────────────────────────────────────────────────────────

section "8. Insufficient capacity"

cleanup_test_dir

# Create a tiny 2×2 BMP (12 RGB bytes = 12 bits of capacity).
# Payload header alone is 17 bytes = 136 bits.
TINY_BMP="${TEST_DIR}/tiny.bmp"
create_bmp "${TINY_BMP}" 2 2

echo "This payload is too large for a 2x2 BMP" > "${TEST_DIR}/big.txt"

if "${STEG}" -e "${TINY_BMP}" "${TEST_DIR}/big.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "Insufficient capacity: should have been rejected"
else
    pass "Insufficient capacity: correctly rejected"
fi

# ──────────────────────────────────────────────────────────────
# 9. Corrupted embedded data → CRC mismatch
# ──────────────────────────────────────────────────────────────

section "9. CRC mismatch (corrupted stego BMP)"

cleanup_test_dir

echo "CRC integrity test" > "${TEST_DIR}/crc_test.txt"

"${STEG}" -e "${CARRIER}" "${TEST_DIR}/crc_test.txt" \
    "${TEST_DIR}/stego_crc.bmp" >/dev/null 2>&1

# Corrupt pixel bytes in the payload data area.
# Each payload byte uses 8 pixel bytes in the carrier.
# Header (17 bytes) + filename occupy the first (17+N)*8 pixel bytes.
# We corrupt well into the data region.
python3 - "${TEST_DIR}/stego_crc.bmp" <<'PYEOF'
import struct, sys

path = sys.argv[1]

with open(path, "r+b") as f:
    # Read pixel_offset
    f.seek(10)
    offset = struct.unpack("<I", f.read(4))[0]

    # Read width to calculate row geometry
    f.seek(18)
    width = struct.unpack("<i", f.read(4))[0]

    # Payload header = 17 bytes, filename ~15 bytes max
    # Data starts around pixel byte (17+15)*8 = 256
    # Corrupt bytes deep in the data area
    corrupt_pos = offset + 300
    f.seek(corrupt_pos)
    original = f.read(32)
    f.seek(corrupt_pos)
    f.write(bytes([b ^ 0xFF for b in original]))
PYEOF

if "${STEG}" -d "${TEST_DIR}/stego_crc.bmp" \
   "${TEST_DIR}/decode_out" >/dev/null 2>&1; then
    fail "CRC mismatch: decoder should have rejected corrupted data"
else
    pass "CRC mismatch: correctly detected corruption"
fi

# Ensure no partial file was left behind
if ls "${TEST_DIR}/decode_out/"* >/dev/null 2>&1; then
    fail "CRC mismatch: partial output file left behind"
else
    pass "CRC mismatch: no partial output file left"
fi

# ──────────────────────────────────────────────────────────────
# 10. Invalid / non-BMP input
# ──────────────────────────────────────────────────────────────

section "10. Invalid / non-BMP input"

cleanup_test_dir

echo "not a bmp" > "${TEST_DIR}/secret.txt"

# Test with a plaintext file as carrier
echo "this is not a BMP file" > "${TEST_DIR}/fake.bmp"

if "${STEG}" -e "${TEST_DIR}/fake.bmp" "${TEST_DIR}/secret.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "Non-BMP carrier: should have been rejected (encode)"
else
    pass "Non-BMP carrier: correctly rejected (encode)"
fi

if "${STEG}" -d "${TEST_DIR}/fake.bmp" \
   "${TEST_DIR}/decode_out" >/dev/null 2>&1; then
    fail "Non-BMP carrier: should have been rejected (decode)"
else
    pass "Non-BMP carrier: correctly rejected (decode)"
fi

# Test with a PNG-like header
printf '\x89PNG\r\n\x1a\n' > "${TEST_DIR}/fake.png"

if "${STEG}" -e "${TEST_DIR}/fake.png" "${TEST_DIR}/secret.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "PNG as carrier: should have been rejected"
else
    pass "PNG as carrier: correctly rejected"
fi

# Test with an empty file
: > "${TEST_DIR}/empty.bmp"

if "${STEG}" -e "${TEST_DIR}/empty.bmp" "${TEST_DIR}/secret.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "Empty file as carrier: should have been rejected"
else
    pass "Empty file as carrier: correctly rejected"
fi

# Test with truncated BMP (only the signature)
printf 'BM' > "${TEST_DIR}/truncated.bmp"

if "${STEG}" -e "${TEST_DIR}/truncated.bmp" "${TEST_DIR}/secret.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "Truncated BMP: should have been rejected"
else
    pass "Truncated BMP: correctly rejected"
fi

# Test decode against a normal (non-stego) BMP
if "${STEG}" -d "${CARRIER}" \
   "${TEST_DIR}/decode_out" >/dev/null 2>&1; then
    fail "Clean BMP: decode should fail (no embedded data)"
else
    pass "Clean BMP: decode correctly rejected"
fi

# ──────────────────────────────────────────────────────────────
# 11. Empty secret file
# ──────────────────────────────────────────────────────────────

section "11. Empty secret file"

cleanup_test_dir

: > "${TEST_DIR}/empty_secret.txt"

if "${STEG}" -e "${CARRIER}" "${TEST_DIR}/empty_secret.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "Empty secret file: should have been rejected"
else
    pass "Empty secret file: correctly rejected"
fi

# ──────────────────────────────────────────────────────────────
# 12. Malicious filename / path cases
# ──────────────────────────────────────────────────────────────

section "12. Malicious filename / path cases"

cleanup_test_dir

# Test that output overwrites are prevented
# (same input and output)
echo "test" > "${TEST_DIR}/secret.txt"

if "${STEG}" -e "${CARRIER}" "${TEST_DIR}/secret.txt" \
   "${CARRIER}" >/dev/null 2>&1; then
    fail "Same input/output: should have been rejected"
else
    pass "Same input/output: correctly rejected"
fi

# Test that secret=output is prevented
if "${STEG}" -e "${CARRIER}" "${TEST_DIR}/secret.txt" \
   "${TEST_DIR}/secret.txt" >/dev/null 2>&1; then
    fail "Secret=output: should have been rejected"
else
    pass "Secret=output: correctly rejected"
fi

# Test nonexistent carrier
if "${STEG}" -e "/nonexistent/path/carrier.bmp" \
   "${TEST_DIR}/secret.txt" "${TEST_DIR}/stego.bmp" \
   >/dev/null 2>&1; then
    fail "Nonexistent carrier: should have been rejected"
else
    pass "Nonexistent carrier: correctly rejected"
fi

# Test nonexistent secret file
if "${STEG}" -e "${CARRIER}" "/nonexistent/secret.txt" \
   "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then
    fail "Nonexistent secret: should have been rejected"
else
    pass "Nonexistent secret: correctly rejected"
fi

# Test decode to nonexistent output directory
echo "dir test" > "${TEST_DIR}/dir_test.txt"
"${STEG}" -e "${CARRIER}" "${TEST_DIR}/dir_test.txt" \
    "${TEST_DIR}/stego.bmp" >/dev/null 2>&1

if "${STEG}" -d "${TEST_DIR}/stego.bmp" \
   "/nonexistent/directory/" >/dev/null 2>&1; then
    fail "Nonexistent output dir: should have been rejected"
else
    pass "Nonexistent output dir: correctly rejected"
fi

# Test with no arguments
if "${STEG}" >/dev/null 2>&1; then
    fail "No arguments: should return failure"
else
    pass "No arguments: correctly returns failure"
fi

# Test with unknown option
if "${STEG}" -z >/dev/null 2>&1; then
    fail "Unknown option: should return failure"
else
    pass "Unknown option: correctly returns failure"
fi

# Test encode with wrong argument count
if "${STEG}" -e "${CARRIER}" >/dev/null 2>&1; then
    fail "Encode wrong argc: should return failure"
else
    pass "Encode wrong argc: correctly returns failure"
fi

# Test decode with wrong argument count
if "${STEG}" -d >/dev/null 2>&1; then
    fail "Decode wrong argc: should return failure"
else
    pass "Decode wrong argc: correctly returns failure"
fi

# ──────────────────────────────────────────────────────────────
# 13. Larger payloads crossing the 4096-byte IO buffer
# ──────────────────────────────────────────────────────────────

section "13. Larger payloads (crossing 4096-byte buffer)"

cleanup_test_dir

# Test payloads around the buffer boundary
for size in 4000 4096 4097 8192 16384; do
    # Generate random binary data of specified size
    head -c "${size}" /dev/urandom > "${TEST_DIR}/large_${size}.bin"

    if "${STEG}" -e "${CARRIER}" "${TEST_DIR}/large_${size}.bin" \
       "${TEST_DIR}/stego.bmp" >/dev/null 2>&1; then

        "${STEG}" -d "${TEST_DIR}/stego.bmp" \
            "${TEST_DIR}/decode_out" >/dev/null 2>&1

        if cmp -s "${TEST_DIR}/large_${size}.bin" \
           "${TEST_DIR}/decode_out/large_${size}.bin"; then
            pass "Large payload: ${size} bytes roundtrip"
        else
            fail "Large payload: ${size} bytes roundtrip mismatch"
        fi

        # Clean decoded file for next iteration
        rm -f "${TEST_DIR}/decode_out/large_${size}.bin"
    else
        skip "Large payload: ${size} bytes (may exceed carrier capacity)"
    fi
done

# ──────────────────────────────────────────────────────────────
# 14. Additional edge cases
# ──────────────────────────────────────────────────────────────

section "14. Additional edge cases"

cleanup_test_dir

# Single byte file
printf 'X' > "${TEST_DIR}/one_byte.bin"

"${STEG}" -e "${CARRIER}" "${TEST_DIR}/one_byte.bin" \
    "${TEST_DIR}/stego.bmp" >/dev/null 2>&1

"${STEG}" -d "${TEST_DIR}/stego.bmp" \
    "${TEST_DIR}/decode_out" >/dev/null 2>&1

if cmp -s "${TEST_DIR}/one_byte.bin" \
   "${TEST_DIR}/decode_out/one_byte.bin"; then
    pass "Single byte file roundtrip"
else
    fail "Single byte file roundtrip"
fi

# File with null bytes
cleanup_test_dir
python3 -c "import sys; sys.stdout.buffer.write(b'\x00' * 100)" \
    > "${TEST_DIR}/nulls.bin"

"${STEG}" -e "${CARRIER}" "${TEST_DIR}/nulls.bin" \
    "${TEST_DIR}/stego.bmp" >/dev/null 2>&1

"${STEG}" -d "${TEST_DIR}/stego.bmp" \
    "${TEST_DIR}/decode_out" >/dev/null 2>&1

if cmp -s "${TEST_DIR}/nulls.bin" \
   "${TEST_DIR}/decode_out/nulls.bin"; then
    pass "File with null bytes roundtrip"
else
    fail "File with null bytes roundtrip"
fi

# Stego BMP preserves file size of the carrier
cleanup_test_dir
echo "size test" > "${TEST_DIR}/size_test.txt"

"${STEG}" -e "${CARRIER}" "${TEST_DIR}/size_test.txt" \
    "${TEST_DIR}/stego.bmp" >/dev/null 2>&1

ORIGINAL_SIZE=$(stat -c%s "${CARRIER}")
STEGO_SIZE=$(stat -c%s "${TEST_DIR}/stego.bmp")

if [ "${ORIGINAL_SIZE}" = "${STEGO_SIZE}" ]; then
    pass "Stego BMP preserves carrier file size"
else
    fail "Stego BMP size mismatch (original=${ORIGINAL_SIZE}, stego=${STEGO_SIZE})"
fi

# ──────────────────────────────────────────────────────────────
# Summary
# ──────────────────────────────────────────────────────────────

section "Summary"

TOTAL=$((PASS_COUNT + FAIL_COUNT + SKIP_COUNT))

printf "\n"
printf "  Total:   %d\n" "${TOTAL}"
printf "  ${GREEN}Passed:  %d${RESET}\n" "${PASS_COUNT}"
printf "  ${RED}Failed:  %d${RESET}\n" "${FAIL_COUNT}"
printf "  ${YELLOW}Skipped: %d${RESET}\n" "${SKIP_COUNT}"
printf "\n"

# ──────────────────────────────────────────────────────────────
# Cleanup
# ──────────────────────────────────────────────────────────────

rm -rf "${TEST_DIR}"

if [ "${FAIL_COUNT}" -gt 0 ]; then
    printf "${RED}${BOLD}SOME TESTS FAILED${RESET}\n\n"
    exit 1
fi

printf "${GREEN}${BOLD}ALL TESTS PASSED${RESET}\n\n"
exit 0
