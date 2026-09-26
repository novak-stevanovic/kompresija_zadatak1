#include "compression.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr,
            "usage:\n"
            "  %s entropy <file>\n"
            "  %s compress <algorithm> <input> <output>\n"
            "  %s decompress <input> <output>\n"
            "  %s benchmark <file>\n",
            program, program, program, program);
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "entropy") == 0) {
        Buffer buffer;
        double entropy;

        if (read_file(argv[2], &buffer) != 0) {
            return 1;
        }
        entropy = byte_entropy(buffer.data, buffer.size);
        printf("%.6f bit/B\n", entropy);
        free_buffer(&buffer);
        return 0;
    }

    if (argc == 5 && strcmp(argv[1], "compress") == 0) {
        Algorithm algorithm;

        if (parse_algorithm(argv[2], &algorithm) != 0) {
            fprintf(stderr, "error: unknown algorithm %s\n", argv[2]);
            return 1;
        }
        return compress_file(algorithm, argv[3], argv[4]) == 0 ? 0 : 1;
    }

    if (argc == 4 && strcmp(argv[1], "decompress") == 0) {
        return decompress_file(argv[2], argv[3]) == 0 ? 0 : 1;
    }

    if (argc == 3 && strcmp(argv[1], "benchmark") == 0) {
        return benchmark_file(argv[2]) == 0 ? 0 : 1;
    }

    usage(argv[0]);
    return 1;
}
