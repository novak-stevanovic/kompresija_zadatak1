#ifndef PREFIX_H
#define PREFIX_H

#include "bitstream.h"
#include "compression.h"

int prefix_compress(const uint8_t *data, size_t size, Algorithm algorithm,
                    BitWriter *writer);
int prefix_decompress(BitReader *reader, uint8_t *output, size_t size);

#endif
