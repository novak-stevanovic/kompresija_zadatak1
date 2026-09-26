#ifndef LZ77_H
#define LZ77_H

#include "bitstream.h"

#include <stddef.h>
#include <stdint.h>

int lz77_compress(const uint8_t *data, size_t size, BitWriter *writer);
int lz77_decompress(BitReader *reader, uint8_t *output, size_t size);

#endif
