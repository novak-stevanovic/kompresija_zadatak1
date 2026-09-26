#ifndef LZW_H
#define LZW_H

#include "bitstream.h"

#include <stddef.h>
#include <stdint.h>

int lzw_compress(const uint8_t *data, size_t size, BitWriter *writer);
int lzw_decompress(BitReader *reader, uint8_t *output, size_t size);

#endif
