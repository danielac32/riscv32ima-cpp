#ifndef MEM_HPP
#define MEM_HPP

#include <cstdint>
#include <cstdio>

#define DRAM_SIZE 390000
#define BLOCK_SIZE 32

struct Memory {
    FILE* file;
    uint32_t size;
    char* p;
};

uint32_t get_file_size(FILE* file);
Memory create_memory(const char* filename);
void free_memory(Memory* mem);

#endif