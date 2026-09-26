#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ALG_SHANNON_FANO = 1,
    ALG_HUFFMAN = 2,
    ALG_LZ77 = 3,
    ALG_LZW = 4
} Algorithm;

typedef struct {
    uint8_t *data;
    size_t size;
} Buffer;

int read_file(const char *path, Buffer *buffer);
int write_file(const char *path, const uint8_t *data, size_t size);
void free_buffer(Buffer *buffer);

double byte_entropy(const uint8_t *data, size_t size);
const char *algorithm_name(Algorithm algorithm);
int parse_algorithm(const char *name, Algorithm *algorithm);

int compress_file(Algorithm algorithm, const char *input, const char *output);
int decompress_file(const char *input, const char *output);
int benchmark_file(const char *input);


#endif
