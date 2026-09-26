#include "lzw.h"

#include <stdint.h>

#define DICT_SIZE 4096
#define HASH_SIZE 8192

typedef struct {
    int code;
    uint16_t prefix;
    uint8_t suffix;
} HashEntry;

static unsigned pair_hash(uint16_t prefix, uint8_t suffix)
{
    unsigned value = (unsigned)prefix * 257u + suffix;
    return (value * 2654435761u) & (HASH_SIZE - 1u);
}

static int dict_find(HashEntry table[HASH_SIZE], uint16_t prefix,
                     uint8_t suffix)
{
    unsigned index = pair_hash(prefix, suffix);

    for (;;) {
        HashEntry *entry = &table[index];

        if (entry->code < 0) {
            return -1;
        }
        if (entry->prefix == prefix && entry->suffix == suffix) {
            return entry->code;
        }
        index = (index + 1u) & (HASH_SIZE - 1u);
    }
}

static void dict_add(HashEntry table[HASH_SIZE], uint16_t prefix,
                     uint8_t suffix, int code)
{
    unsigned index = pair_hash(prefix, suffix);

    while (table[index].code >= 0) {
        index = (index + 1u) & (HASH_SIZE - 1u);
    }

    table[index].code = code;
    table[index].prefix = prefix;
    table[index].suffix = suffix;
}

int lzw_compress(const uint8_t *data, size_t size, BitWriter *writer)
{
    HashEntry table[HASH_SIZE];
    int next_code = 256;
    uint16_t current;
    size_t i;

    if (size == 0) {
        return 0;
    }

    for (i = 0; i < HASH_SIZE; ++i) {
        table[i].code = -1;
    }

    current = data[0];

    for (i = 1; i < size; ++i) {
        int found = dict_find(table, current, data[i]);

        if (found >= 0) {
            current = (uint16_t)found;
            continue;
        }

        bit_writer_write_bits(writer, current, 12);
        if (next_code < DICT_SIZE) {
            dict_add(table, current, data[i], next_code++);
        }
        current = data[i];
    }

    bit_writer_write_bits(writer, current, 12);
    return writer->error ? -1 : 0;
}

static int expand_code(uint16_t code, const uint16_t prefix[DICT_SIZE],
                       const uint8_t suffix[DICT_SIZE], uint8_t stack[DICT_SIZE],
                       int next_code)
{
    int length = 0;

    while (code >= 256) {
        if (code >= next_code || length >= DICT_SIZE - 1) {
            return -1;
        }
        stack[length++] = suffix[code];
        code = prefix[code];
    }

    stack[length++] = (uint8_t)code;
    return length;
}

static int write_stack(uint8_t *output, size_t size, size_t *pos,
                       const uint8_t stack[DICT_SIZE], int length)
{
    while (length > 0) {
        if (*pos >= size) {
            return -1;
        }
        output[(*pos)++] = stack[--length];
    }
    return 0;
}

int lzw_decompress(BitReader *reader, uint8_t *output, size_t size)
{
    uint16_t prefix[DICT_SIZE];
    uint8_t suffix[DICT_SIZE];
    uint8_t stack[DICT_SIZE];
    int next_code = 256;
    uint16_t old_code;
    uint8_t first;
    size_t pos = 0;

    if (size == 0) {
        return 0;
    }

    old_code = (uint16_t)bit_reader_read_bits(reader, 12);
    if (reader->error || old_code >= 256) {
        return -1;
    }

    first = (uint8_t)old_code;
    output[pos++] = first;

    while (pos < size) {
        uint16_t code = (uint16_t)bit_reader_read_bits(reader, 12);
        int length;

        if (reader->error) {
            return -1;
        }

        if (code < next_code) {
            length = expand_code(code, prefix, suffix, stack, next_code);
            if (length < 0) {
                return -1;
            }
            first = stack[length - 1];
            if (write_stack(output, size, &pos, stack, length) != 0) {
                return -1;
            }
        } else if (code == next_code) {
            length = expand_code(old_code, prefix, suffix, stack, next_code);
            if (length < 0) {
                return -1;
            }
            first = stack[length - 1];
            if (write_stack(output, size, &pos, stack, length) != 0 ||
                pos >= size) {
                return -1;
            }
            output[pos++] = first;
        } else {
            return -1;
        }

        if (next_code < DICT_SIZE) {
            prefix[next_code] = old_code;
            suffix[next_code] = first;
            ++next_code;
        }
        old_code = code;
    }

    return 0;
}
