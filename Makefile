CC = gcc
CXX = g++
EXE = cypher
SRC = cypher.c
STATIC_LIB = lib/tree-sitter/libtree-sitter.a
FILE ?=

INCLUDES = -I lib/tree-sitter/lib/include
CFLAGS = -Wall -Wextra -pedantic -std=c99

LANGS = c cpp python java go json
PARSERS = $(patsubst %,parsers/tree-sitter-%.so,$(LANGS))

all: $(EXE) $(PARSERS)

$(STATIC_LIB):
	@echo "Building Tree-sitter static library..."
	$(MAKE) -C lib/tree-sitter

$(EXE): $(SRC) $(STATIC_LIB)
	$(CC) $(SRC) $(STATIC_LIB) -o $(EXE) $(CFLAGS) $(INCLUDES)

parsers/tree-sitter-%.so:
	@echo "Building $* parser..."
	@mkdir -p parsers
	$(CC) -O3 -fPIC -shared \
		lib/tree-sitter-$*/src/parser.c \
		$(wildcard lib/tree-sitter-$*/src/scanner.c) \
		-I lib/tree-sitter-$*/src -o $@

run: $(EXE)
	./$(EXE) $(FILE)

clean:
	rm -f $(EXE)
	rm -f parsers/*.so
	$(MAKE) -C lib/tree-sitter clean

.PHONY: all run clean