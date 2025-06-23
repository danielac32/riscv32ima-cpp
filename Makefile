# Makefile para el emulador RISC-V en C++

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -DMINIRV32_IMPLEMENTATION
LDFLAGS = 
DEBUG_FLAGS = -g
OPTIMIZE_FLAGS = -O2
TINY_FLAGS = -Os -ffunction-sections -fdata-sections -Wl,--gc-sections -s

SRC = main.cpp mem.cpp
HEADERS = mini-rv32imah.hpp mem.hpp
TARGET = mini-rv32ima

.PHONY: all clean

all: $(TARGET) $(TARGET).tiny

$(TARGET): $(SRC) $(HEADERS)
	# Versión para depuración
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(OPTIMIZE_FLAGS) -o $@ $(SRC) $(LDFLAGS)

$(TARGET).tiny: $(SRC) $(HEADERS)
	# Versión optimizada para tamaño
	$(CXX) $(CXXFLAGS) $(TINY_FLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -rf $(TARGET) $(TARGET).tiny $(TARGET).flt *.o

# Regla adicional si necesitas generar un .flt (aunque no está claro qué es en tu ejemplo)
$(TARGET).flt: $(TARGET)
	cp $(TARGET) $(TARGET).flt