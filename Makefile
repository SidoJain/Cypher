CC = gcc
CXX = g++
EXE = cypher
SRC = cypher.c
STATIC_LIB = lib/tree-sitter/libtree-sitter.a
PARSERS = parsers/tree-sitter-c.so parsers/tree-sitter-cpp.so parsers/tree-sitter-python.so
FILE ?=

INCLUDES = -I lib/tree-sitter/lib/include
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g

all: $(EXE) parsers

$(STATIC_LIB):
	@echo "Building Tree-sitter static library..."
	$(MAKE) -C lib/tree-sitter

$(EXE): $(SRC) $(STATIC_LIB)
	$(CC) $(SRC) $(STATIC_LIB) -o $(EXE) $(CFLAGS) $(INCLUDES)

# --- Parser Build Rules ---
parsers: $(PARSERS)

parsers/tree-sitter-c.so: lib/tree-sitter-c/src/parser.c
	@echo "Building C parser..."
	@mkdir -p parsers
	$(CC) -O3 -fPIC -shared $^ -I lib/tree-sitter-c/src -o $@

parsers/tree-sitter-cpp.so: lib/tree-sitter-cpp/src/parser.c lib/tree-sitter-cpp/src/scanner.c
	@echo "Building C++ parser..."
	@mkdir -p parsers
	$(CC) -O3 -fPIC -shared $^ -I lib/tree-sitter-cpp/src -o $@

parsers/tree-sitter-python.so: lib/tree-sitter-python/src/parser.c lib/tree-sitter-python/src/scanner.c
	@echo "Building Python parser..."
	@mkdir -p parsers
	$(CC) -O3 -fPIC -shared $^ -I lib/tree-sitter-python/src -o $@

run: $(EXE)
	./$(EXE) $(FILE)

clean:
	rm -f $(EXE)
	rm -f parsers/*.so
	$(MAKE) -C lib/tree-sitter clean

.PHONY: all run clean parsers