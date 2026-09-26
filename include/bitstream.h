#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    uint8_t byte;
    unsigned used;
    int error;
} BitWriter;

typedef struct {
    FILE *file;
    uint8_t byte;
    unsigned left;
    int error;
} BitReader;

void bit_writer_init(BitWriter *writer, FILE *file);
void bit_writer_write(BitWriter *writer, unsigned bit);
void bit_writer_write_bits(BitWriter *writer, uint32_t value, unsigned count);
int bit_writer_flush(BitWriter *writer);

void bit_reader_init(BitReader *reader, FILE *file);
unsigned bit_reader_read(BitReader *reader);
uint32_t bit_reader_read_bits(BitReader *reader, unsigned count);

#endif
