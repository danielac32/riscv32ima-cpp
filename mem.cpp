#include "mem.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

uint32_t get_file_size(FILE* file) {
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    rewind(file);
    return size;
}

Memory create_memory(const char* filename) {
    Memory mem;

    mem.file = fopen(filename, "rb");
    if (mem.file == nullptr) {
        std::printf("Error: No se pudo abrir el archivo.\n");
        while(true);
    }

    mem.size = get_file_size(mem.file);
    mem.p = static_cast<char*>(malloc(DRAM_SIZE));
    
    if (mem.p == nullptr) {
        fclose(mem.file);
        std::printf("Error: No se pudo asignar memoria.\n");
        while(true);
    }

    std::fread(mem.p, 1, mem.size, mem.file);
    std::fclose(mem.file);
    
    std::printf("END\n");
    return mem;
}

void free_memory(Memory* mem) {
    if (mem->file) {
        std::fclose(mem->file);
        mem->file = nullptr;
    }
    mem->size = 0;
    free(mem->p);
    mem->p = nullptr;
}