#include "bitstream.h"

void bit_writer_init(BitWriter *writer, FILE *file)
{
    writer->file = file;
    writer->byte = 0;
    writer->used = 0;
    writer->error = 0;
}

void bit_writer_write(BitWriter *writer, unsigned bit)
{
    if (writer->error) {
        return;
    }

    writer->byte = (uint8_t)((writer->byte << 1) | (bit & 1u));
    ++writer->used;

    if (writer->used == 8) {
        if (fputc(writer->byte, writer->file) == EOF) {
            writer->error = 1;
        }
        writer->byte = 0;
        writer->used = 0;
    }
}

void bit_writer_write_bits(BitWriter *writer, uint32_t value, unsigned count)
{
    while (count > 0) {
        --count;
        bit_writer_write(writer, (value >> count) & 1u);
    }
}

int bit_writer_flush(BitWriter *writer)
{
    if (writer->used != 0 && !writer->error) {
        writer->byte <<= 8 - writer->used;
        if (fputc(writer->byte, writer->file) == EOF) {
            writer->error = 1;
        }
    }

    writer->byte = 0;
    writer->used = 0;
    return writer->error ? -1 : 0;
}

void bit_reader_init(BitReader *reader, FILE *file)
{
    reader->file = file;
    reader->byte = 0;
    reader->left = 0;
    reader->error = 0;
}

unsigned bit_reader_read(BitReader *reader)
{
    int ch;

    if (reader->error) {
        return 0;
    }

    if (reader->left == 0) {
        ch = fgetc(reader->file);
        if (ch == EOF) {
            reader->error = 1;
            return 0;
        }
        reader->byte = (uint8_t)ch;
        reader->left = 8;
    }

    --reader->left;
    return (reader->byte >> reader->left) & 1u;
}

uint32_t bit_reader_read_bits(BitReader *reader, unsigned count)
{
    uint32_t value = 0;

    while (count > 0) {
        value = (value << 1) | bit_reader_read(reader);
        --count;
    }

    return value;
}
