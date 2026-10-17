# ============================================================================
#  Makefile Multiplataforma Genérico y Robusto - Simulador Sísmico
# ============================================================================

CXX      ?= g++
CXXSTD   ?= -std=c++17
CXXFLAGS ?= -O2 -Wall -Wextra -I.
LDFLAGS  ?=

SRCS := SEGYParser.cpp StorageBackend.cpp main.cpp
OBJS := $(SRCS:.cpp=.o)
BIN  := seismic_sim

# --- Detección y configuración estricta para Windows vs Linux/macOS ---
ifeq ($(OS),Windows_NT)
    SHELL      = cmd.exe
    CLEAN_CMD  = del /f /q $(OBJS) $(BIN) seismic_sim.exe 2>nul & rmdir /s /q data 2>nul & rmdir /s /q -p 2>nul & exit 0
    MKDIR_DATA = if not exist data mkdir data 2>nul
else
    CLEAN_CMD  = rm -f $(OBJS) $(BIN) && rm -rf data
    MKDIR_DATA = mkdir -p data
endif

.PHONY: all run clean data

all: $(BIN)

# Enlazado final
$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compilación por archivo
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CXXSTD) -c $< -o $@

# Ejecución con datos por defecto
run: $(BIN)
	@$(MKDIR_DATA)
	./$(BIN)

# Generar dataset grande de prueba
data:
	@$(MKDIR_DATA)
	./$(BIN) --gen 128 128 512 -o data/big.sgy

clean:
	@$(CLEAN_CMD)