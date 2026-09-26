# Projektni zadatak 1

Implementacija bajt-entropije i algoritama Shannon-Fano, Huffman, LZ77 i LZW u C-u.
Koristi se samo standardna C biblioteka.

## Build

```sh
make
```

## Pokretanje

```sh
./kompresija entropy data/tekst.txt
./kompresija compress huffman data/tekst.txt build/tekst.huf
./kompresija decompress build/tekst.huf build/vracen.txt
cmp data/tekst.txt build/vracen.txt
./kompresija benchmark data/tekst.txt
```

Algoritmi za `compress` su `shannon-fano`, `huffman`, `lz77` i `lzw`.
`benchmark` daje samo podatke potrebne za izvestaj i proverava da li se posle
dekompresije dobija originalni fajl.

Kompresovani fajl pocinje zaglavljem `KZP1`, oznakom algoritma i originalnom
velicinom. Shannon-Fano i Huffman zatim cuvaju kodno stablo. LZ77 koristi
prozor od 4095 bajtova, a LZW 12-bitni recnik sa najvise 4096 kodova.
