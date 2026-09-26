#include "lz77.h"

#include <stdlib.h>

#define WINDOW 4095
#define MIN_MATCH 3
#define MAX_MATCH 258
#define HASH_SIZE 65536
#define MAX_CHAIN 64

static unsigned hash3(const uint8_t *p)
{
    unsigned value = (unsigned)p[0] * 251u;
    value ^= (unsigned)p[1] * 67u;
    value ^= (unsigned)p[2] * 17u;
    return value & (HASH_SIZE - 1u);
}

static void insert_position(const uint8_t *data, size_t size, size_t pos,
                            int *head, int *prev)
{
    unsigned hash;

    if (pos + 2 >= size) {
        return;
    }

    hash = hash3(data + pos);
    prev[pos] = head[hash];
    head[hash] = (int)pos;
}

int lz77_compress(const uint8_t *data, size_t size, BitWriter *writer)
{
    int *head;
    int *prev;
    size_t i = 0;
    int h;

    head = malloc(sizeof(*head) * HASH_SIZE);
    prev = malloc(sizeof(*prev) * (size == 0 ? 1 : size));
    if (head == NULL || prev == NULL) {
        free(head);
        free(prev);
        return -1;
    }

    for (h = 0; h < HASH_SIZE; ++h) {
        head[h] = -1;
    }

    while (i < size) {
        size_t best_len = 0;
        size_t best_dist = 0;

        if (i + MIN_MATCH <= size) {
            int candidate = head[hash3(data + i)];
            int seen = 0;

            while (candidate >= 0 && seen < MAX_CHAIN) {
                size_t pos = (size_t)candidate;
                size_t dist = i - pos;
                size_t len = 0;
                size_t limit = size - i;

                if (dist > WINDOW) {
                    break;
                }
                if (limit > MAX_MATCH) {
                    limit = MAX_MATCH;
                }

                while (len < limit && data[pos + len] == data[i + len]) {
                    ++len;
                }

                if (len >= MIN_MATCH && len > best_len) {
                    best_len = len;
                    best_dist = dist;
                    if (len == limit) {
                        break;
                    }
                }

                candidate = prev[pos];
                ++seen;
            }
        }

        if (best_len >= MIN_MATCH) {
            size_t j;

            bit_writer_write(writer, 1);
            bit_writer_write_bits(writer, (uint32_t)best_dist, 12);
            bit_writer_write_bits(writer,
                                  (uint32_t)(best_len - MIN_MATCH), 8);

            for (j = 0; j < best_len; ++j) {
                insert_position(data, size, i + j, head, prev);
            }
            i += best_len;
        } else {
            bit_writer_write(writer, 0);
            bit_writer_write_bits(writer, data[i], 8);
            insert_position(data, size, i, head, prev);
            ++i;
        }

        if (writer->error) {
            free(head);
            free(prev);
            return -1;
        }
    }

    free(head);
    free(prev);
    return 0;
}

int lz77_decompress(BitReader *reader, uint8_t *output, size_t size)
{
    size_t pos = 0;

    while (pos < size) {
        unsigned match = bit_reader_read(reader);

        if (reader->error) {
            return -1;
        }

        if (!match) {
            output[pos++] = (uint8_t)bit_reader_read_bits(reader, 8);
            if (reader->error) {
                return -1;
            }
        } else {
            size_t dist = bit_reader_read_bits(reader, 12);
            size_t len = bit_reader_read_bits(reader, 8) + MIN_MATCH;
            size_t j;

            if (reader->error || dist == 0 || dist > pos || len > size - pos) {
                return -1;
            }

            for (j = 0; j < len; ++j) {
                output[pos] = output[pos - dist];
                ++pos;
            }
        }
    }

    return 0;
}
