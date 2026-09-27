CC = gcc
CFLAGS = -std=c99 -O2 -Wall -Wextra -Wpedantic -Wfatal-errors -Iinclude -Isrc

all:
	$(CC) $(CFLAGS) src/main.c src/compression.c src/bitstream.c src/prefix.c src/lz77.c src/lzw.c -o kompresija -lm

clean:
	rm -f kompresija
