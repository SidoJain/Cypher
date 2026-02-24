CC = gcc
EXE = cypher
SRC = cypher.c
STATIC_LIB = tree-sitter/libtree-sitter.a
FILE ?=

INCLUDES = -I tree-sitter/lib/include
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g

all: $(EXE)

$(STATIC_LIB):
	@echo "Building Tree-sitter static library..."
	$(MAKE) -C tree-sitter

$(EXE): $(SRC) $(STATIC_LIB)
	$(CC) $(SRC) $(STATIC_LIB) -o $(EXE) $(CFLAGS) $(INCLUDES)

run: $(EXE)
	./$(EXE) $(FILE)

clean:
	rm -f $(EXE)
	$(MAKE) -C tree-sitter clean

.PHONY: all run clean