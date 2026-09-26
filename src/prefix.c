#include "prefix.h"

#include <stdlib.h>
#include <string.h>

#define MAX_NODES 511
#define MAX_CODE_BITS 256

typedef struct {
    uint64_t freq;
    int symbol;
    int left;
    int right;
    int min_symbol;
} Node;

typedef struct {
    uint8_t bits[MAX_CODE_BITS];
    unsigned length;
} Code;

typedef struct {
    uint64_t freq;
    int symbol;
} SymbolFreq;

typedef struct {
    int item[MAX_NODES];
    int size;
    Node *nodes;
} Heap;

static int node_less(const Node *a, const Node *b)
{
    if (a->freq != b->freq) {
        return a->freq < b->freq;
    }
    return a->min_symbol < b->min_symbol;
}

static int heap_less(const Heap *heap, int a, int b)
{
    return node_less(&heap->nodes[a], &heap->nodes[b]);
}

static void heap_push(Heap *heap, int node)
{
    int i = heap->size++;
    heap->item[i] = node;

    while (i > 0) {
        int parent = (i - 1) / 2;
        int tmp;

        if (!heap_less(heap, heap->item[i], heap->item[parent])) {
            break;
        }

        tmp = heap->item[i];
        heap->item[i] = heap->item[parent];
        heap->item[parent] = tmp;
        i = parent;
    }
}

static int heap_pop(Heap *heap)
{
    int result = heap->item[0];
    int i = 0;

    --heap->size;
    if (heap->size == 0) {
        return result;
    }

    heap->item[0] = heap->item[heap->size];

    for (;;) {
        int left = i * 2 + 1;
        int right = left + 1;
        int best = i;
        int tmp;

        if (left < heap->size &&
            heap_less(heap, heap->item[left], heap->item[best])) {
            best = left;
        }
        if (right < heap->size &&
            heap_less(heap, heap->item[right], heap->item[best])) {
            best = right;
        }
        if (best == i) {
            break;
        }

        tmp = heap->item[i];
        heap->item[i] = heap->item[best];
        heap->item[best] = tmp;
        i = best;
    }

    return result;
}

static int symbol_cmp(const void *pa, const void *pb)
{
    const SymbolFreq *a = pa;
    const SymbolFreq *b = pb;

    if (a->freq < b->freq) {
        return 1;
    }
    if (a->freq > b->freq) {
        return -1;
    }
    return a->symbol - b->symbol;
}

static int new_leaf(Node *nodes, int *count, int symbol, uint64_t freq)
{
    int index = (*count)++;

    nodes[index].freq = freq;
    nodes[index].symbol = symbol;
    nodes[index].left = -1;
    nodes[index].right = -1;
    nodes[index].min_symbol = symbol;
    return index;
}

static int new_parent(Node *nodes, int *count, int left, int right)
{
    int index = (*count)++;

    nodes[index].freq = nodes[left].freq + nodes[right].freq;
    nodes[index].symbol = -1;
    nodes[index].left = left;
    nodes[index].right = right;
    nodes[index].min_symbol = nodes[left].min_symbol < nodes[right].min_symbol
        ? nodes[left].min_symbol : nodes[right].min_symbol;
    return index;
}

static int build_huffman(Node *nodes, int *count, const uint64_t freq[256])
{
    Heap heap;
    int symbol;

    heap.size = 0;
    heap.nodes = nodes;

    for (symbol = 0; symbol < 256; ++symbol) {
        if (freq[symbol] != 0) {
            heap_push(&heap, new_leaf(nodes, count, symbol, freq[symbol]));
        }
    }

    while (heap.size > 1) {
        int left = heap_pop(&heap);
        int right = heap_pop(&heap);
        heap_push(&heap, new_parent(nodes, count, left, right));
    }

    return heap_pop(&heap);
}

static int build_shannon(Node *nodes, int *count, const SymbolFreq *symbols,
                         int begin, int end)
{
    uint64_t total = 0;
    uint64_t left_sum = 0;
    uint64_t best_diff = UINT64_MAX;
    int split = begin + 1;
    int i;

    if (end - begin == 1) {
        return new_leaf(nodes, count, symbols[begin].symbol,
                        symbols[begin].freq);
    }

    for (i = begin; i < end; ++i) {
        total += symbols[i].freq;
    }

    for (i = begin; i + 1 < end; ++i) {
        uint64_t right_sum;
        uint64_t diff;

        left_sum += symbols[i].freq;
        right_sum = total - left_sum;
        diff = left_sum > right_sum ? left_sum - right_sum
                                    : right_sum - left_sum;
        if (diff < best_diff) {
            best_diff = diff;
            split = i + 1;
        }
    }

    {
        int left = build_shannon(nodes, count, symbols, begin, split);
        int right = build_shannon(nodes, count, symbols, split, end);
        return new_parent(nodes, count, left, right);
    }
}

static int build_shannon_fano(Node *nodes, int *count,
                              const uint64_t freq[256])
{
    SymbolFreq symbols[256];
    int n = 0;
    int symbol;

    for (symbol = 0; symbol < 256; ++symbol) {
        if (freq[symbol] != 0) {
            symbols[n].freq = freq[symbol];
            symbols[n].symbol = symbol;
            ++n;
        }
    }

    qsort(symbols, (size_t)n, sizeof(symbols[0]), symbol_cmp);
    return build_shannon(nodes, count, symbols, 0, n);
}

static void make_codes(const Node *nodes, int node, uint8_t *path,
                       unsigned depth, Code codes[256])
{
    if (nodes[node].symbol >= 0) {
        Code *code = &codes[nodes[node].symbol];
        code->length = depth;
        memcpy(code->bits, path, depth);
        return;
    }

    path[depth] = 0;
    make_codes(nodes, nodes[node].left, path, depth + 1, codes);
    path[depth] = 1;
    make_codes(nodes, nodes[node].right, path, depth + 1, codes);
}

static void write_tree(const Node *nodes, int node, BitWriter *writer)
{
    if (nodes[node].symbol >= 0) {
        bit_writer_write(writer, 1);
        bit_writer_write_bits(writer, (uint32_t)nodes[node].symbol, 8);
        return;
    }

    bit_writer_write(writer, 0);
    write_tree(nodes, nodes[node].left, writer);
    write_tree(nodes, nodes[node].right, writer);
}

static int read_tree(Node *nodes, int *count, BitReader *reader, int depth)
{
    int node;

    if (depth > 255 || *count >= MAX_NODES) {
        return -1;
    }

    node = (*count)++;
    nodes[node].left = -1;
    nodes[node].right = -1;
    nodes[node].freq = 0;

    if (bit_reader_read(reader)) {
        nodes[node].symbol = (int)bit_reader_read_bits(reader, 8);
        nodes[node].min_symbol = nodes[node].symbol;
    } else {
        nodes[node].symbol = -1;
        nodes[node].left = read_tree(nodes, count, reader, depth + 1);
        nodes[node].right = read_tree(nodes, count, reader, depth + 1);
        if (nodes[node].left < 0 || nodes[node].right < 0) {
            return -1;
        }
    }

    return reader->error ? -1 : node;
}

int prefix_compress(const uint8_t *data, size_t size, Algorithm algorithm,
                    BitWriter *writer)
{
    uint64_t freq[256] = {0};
    Node nodes[MAX_NODES];
    Code codes[256] = {{{0}, 0}};
    uint8_t path[MAX_CODE_BITS];
    int count = 0;
    int root;
    size_t i;

    if (size == 0) {
        return 0;
    }

    for (i = 0; i < size; ++i) {
        ++freq[data[i]];
    }

    if (algorithm == ALG_HUFFMAN) {
        root = build_huffman(nodes, &count, freq);
    } else {
        root = build_shannon_fano(nodes, &count, freq);
    }

    write_tree(nodes, root, writer);
    make_codes(nodes, root, path, 0, codes);

    for (i = 0; i < size; ++i) {
        Code *code = &codes[data[i]];
        unsigned j;

        for (j = 0; j < code->length; ++j) {
            bit_writer_write(writer, code->bits[j]);
        }
    }

    return writer->error ? -1 : 0;
}

int prefix_decompress(BitReader *reader, uint8_t *output, size_t size)
{
    Node nodes[MAX_NODES];
    int count = 0;
    int root;
    size_t i;

    if (size == 0) {
        return 0;
    }

    root = read_tree(nodes, &count, reader, 0);
    if (root < 0) {
        return -1;
    }

    if (nodes[root].symbol >= 0) {
        memset(output, nodes[root].symbol, size);
        return 0;
    }

    for (i = 0; i < size; ++i) {
        int node = root;

        while (nodes[node].symbol < 0) {
            node = bit_reader_read(reader) ? nodes[node].right
                                           : nodes[node].left;
            if (reader->error || node < 0 || node >= count) {
                return -1;
            }
        }

        output[i] = (uint8_t)nodes[node].symbol;
    }

    return 0;
}
