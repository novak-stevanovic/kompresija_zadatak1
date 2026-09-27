#include "compression.h"
#include "bitstream.h"
#include "lz77.h"
#include "lzw.h"
#include "prefix.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE (256u * 1024u * 1024u)

static const uint8_t magic[4] = {'K', 'Z', 'P', '1'};

static int write_u64(FILE *file, uint64_t value)
{
    unsigned i;

    for (i = 0; i < 8; ++i) {
        if (fputc((int)(value & 0xffu), file) == EOF) {
            return -1;
        }
        value >>= 8;
    }
    return 0;
}

static int read_u64(FILE *file, uint64_t *value)
{
    uint64_t result = 0;
    unsigned i;

    for (i = 0; i < 8; ++i) {
        int ch = fgetc(file);
        if (ch == EOF) {
            return -1;
        }
        result |= (uint64_t)(uint8_t)ch << (8u * i);
    }

    *value = result;
    return 0;
}

int read_file(const char *path, Buffer *buffer)
{
    FILE *file;
    long length;
    uint8_t *data;

    buffer->data = NULL;
    buffer->size = 0;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: cannot determine file size\n");
        fclose(file);
        return -1;
    }

    if ((unsigned long)length > MAX_FILE_SIZE) {
        fprintf(stderr, "error: file is larger than 256 MiB\n");
        fclose(file);
        return -1;
    }

    data = malloc(length == 0 ? 1u : (size_t)length);
    if (data == NULL) {
        fprintf(stderr, "error: out of memory\n");
        fclose(file);
        return -1;
    }

    if (length > 0 && fread(data, 1, (size_t)length, file) != (size_t)length) {
        fprintf(stderr, "error: cannot read %s\n", path);
        free(data);
        fclose(file);
        return -1;
    }

    fclose(file);
    buffer->data = data;
    buffer->size = (size_t)length;
    return 0;
}

int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        fprintf(stderr, "error: cannot create %s\n", path);
        return -1;
    }

    if (size != 0 && fwrite(data, 1, size, file) != size) {
        fprintf(stderr, "error: cannot write %s\n", path);
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "error: cannot finish %s\n", path);
        return -1;
    }

    return 0;
}

void free_buffer(Buffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}

double byte_entropy(const uint8_t *data, size_t size)
{
    uint64_t count[256] = {0};
    double entropy = 0.0;
    size_t i;

    if (size == 0) {
        return 0.0;
    }

    for (i = 0; i < size; ++i) {
        ++count[data[i]];
    }

    for (i = 0; i < 256; ++i) {
        if (count[i] != 0) {
            double p = (double)count[i] / (double)size;
            entropy -= p * (log(p) / log(2.0));
        }
    }

    return entropy;
}

const char *algorithm_name(Algorithm algorithm)
{
    switch (algorithm) {
    case ALG_SHANNON_FANO:
        return "shannon-fano";
    case ALG_HUFFMAN:
        return "huffman";
    case ALG_LZ77:
        return "lz77";
    case ALG_LZW:
        return "lzw";
    default:
        return NULL;
    }
}

int parse_algorithm(const char *name, Algorithm *algorithm)
{
    if (strcmp(name, "shannon-fano") == 0) {
        *algorithm = ALG_SHANNON_FANO;
    } else if (strcmp(name, "huffman") == 0) {
        *algorithm = ALG_HUFFMAN;
    } else if (strcmp(name, "lz77") == 0) {
        *algorithm = ALG_LZ77;
    } else if (strcmp(name, "lzw") == 0) {
        *algorithm = ALG_LZW;
    } else {
        return -1;
    }
    return 0;
}

int compress_file(Algorithm algorithm, const char *input, const char *output)
{
    Buffer buffer;
    FILE *file;
    BitWriter writer;
    int result = -1;

    if (read_file(input, &buffer) != 0) {
        return -1;
    }

    file = fopen(output, "wb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot create %s\n", output);
        free_buffer(&buffer);
        return -1;
    }

    if (fwrite(magic, 1, sizeof(magic), file) != sizeof(magic) ||
        fputc((int)algorithm, file) == EOF ||
        write_u64(file, (uint64_t)buffer.size) != 0) {
        fprintf(stderr, "error: cannot write header\n");
        goto done;
    }

    bit_writer_init(&writer, file);

    switch (algorithm) {
    case ALG_SHANNON_FANO:
    case ALG_HUFFMAN:
        result = prefix_compress(buffer.data, buffer.size, algorithm, &writer);
        break;
    case ALG_LZ77:
        result = lz77_compress(buffer.data, buffer.size, &writer);
        break;
    case ALG_LZW:
        result = lzw_compress(buffer.data, buffer.size, &writer);
        break;
    default:
        result = -1;
        break;
    }

    if (result == 0 && bit_writer_flush(&writer) != 0) {
        result = -1;
    }

    if (result != 0) {
        fprintf(stderr, "error: compression failed\n");
    }

done:
    if (fclose(file) != 0) {
        result = -1;
    }
    free_buffer(&buffer);
    return result;
}

int decompress_file(const char *input, const char *output)
{
    FILE *file;
    uint8_t file_magic[4];
    uint64_t original_size;
    Algorithm algorithm;
    uint8_t *data = NULL;
    BitReader reader;
    int result = -1;

    file = fopen(input, "rb");
    if (file == NULL) {
        fprintf(stderr, "error: cannot open %s\n", input);
        return -1;
    }

    if (fread(file_magic, 1, sizeof(file_magic), file) != sizeof(file_magic) ||
        memcmp(file_magic, magic, sizeof(magic)) != 0) {
        fprintf(stderr, "error: invalid compressed file\n");
        goto done;
    }

    {
        int id = fgetc(file);
        if (id < ALG_SHANNON_FANO || id > ALG_LZW) {
            fprintf(stderr, "error: invalid algorithm id\n");
            goto done;
        }
        algorithm = (Algorithm)id;
    }

    if (read_u64(file, &original_size) != 0 || original_size > MAX_FILE_SIZE) {
        fprintf(stderr, "error: invalid original size\n");
        goto done;
    }

    data = malloc(original_size == 0 ? 1u : (size_t)original_size);
    if (data == NULL) {
        fprintf(stderr, "error: out of memory\n");
        goto done;
    }

    bit_reader_init(&reader, file);

    switch (algorithm) {
    case ALG_SHANNON_FANO:
    case ALG_HUFFMAN:
        result = prefix_decompress(&reader, data, (size_t)original_size);
        break;
    case ALG_LZ77:
        result = lz77_decompress(&reader, data, (size_t)original_size);
        break;
    case ALG_LZW:
        result = lzw_decompress(&reader, data, (size_t)original_size);
        break;
    default:
        result = -1;
        break;
    }

    if (result != 0) {
        fprintf(stderr, "error: invalid or damaged compressed data\n");
        goto done;
    }

    result = write_file(output, data, (size_t)original_size);

done:
    free(data);
    fclose(file);
    return result;
}

static long file_size(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    size = ftell(file);
    fclose(file);
    return size;
}

static int same_file(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb");
    FILE *fb = fopen(b, "rb");
    int result = 1;

    if (fa == NULL || fb == NULL) {
        if (fa != NULL) fclose(fa);
        if (fb != NULL) fclose(fb);
        return 0;
    }

    while (1) {
        int ca = fgetc(fa);
        int cb = fgetc(fb);

        if (ca != cb) {
            result = 0;
            break;
        }
        if (ca == EOF) {
            break;
        }
    }

    fclose(fa);
    fclose(fb);
    return result;
}

int benchmark_file(const char *input)
{
    static const Algorithm algorithms[] = {
        ALG_SHANNON_FANO, ALG_HUFFMAN, ALG_LZ77, ALG_LZW
    };
    Buffer buffer;
    size_t i;

    if (read_file(input, &buffer) != 0) {
        return -1;
    }

    printf("file: %s\n", input);
    printf("size: %zu B\n", buffer.size);
    printf("entropy: %.6f bit/B\n\n", byte_entropy(buffer.data, buffer.size));
    printf("%-14s %12s %10s %10s\n",
           "algorithm", "bytes", "ratio", "verified");

    free_buffer(&buffer);

    for (i = 0; i < sizeof(algorithms) / sizeof(algorithms[0]); ++i) {
        const char *compressed = "benchmark.bin";
        const char *restored = "benchmark.out";
        long original = file_size(input);
        long packed;
        double ratio;
        int ok;

        if (compress_file(algorithms[i], input, compressed) != 0 ||
            decompress_file(compressed, restored) != 0) {
            remove(compressed);
            remove(restored);
            return -1;
        }

        packed = file_size(compressed);
        ok = same_file(input, restored);
        ratio = original > 0 ? (double)packed / (double)original : 0.0;

        printf("%-14s %12ld %9.2f%% %10s\n",
               algorithm_name(algorithms[i]), packed, ratio * 100.0,
               ok ? "yes" : "no");

        remove(compressed);
        remove(restored);

        if (!ok) {
            return -1;
        }
    }

    return 0;
}
