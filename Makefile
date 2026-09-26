CC := gcc
CFLAGS := -std=c99 -O2 -Wall -Wextra -Wpedantic -Wfatal-errors -Iinclude -Isrc
LDLIBS := -lm

TARGET := kompresija
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@mkdir -p build/test
	@printf 'ABBCBCABABCAABCAAB\nAAAAABBBBBCCCCCDDDD\n' > build/test/input.txt
	@for a in shannon-fano huffman lz77 lzw; do \
		./$(TARGET) compress $$a build/test/input.txt build/test/$$a.bin; \
		./$(TARGET) decompress build/test/$$a.bin build/test/$$a.out; \
		cmp build/test/input.txt build/test/$$a.out || exit 1; \
	done
	@echo "tests: ok"

clean:
	rm -rf build $(TARGET)
