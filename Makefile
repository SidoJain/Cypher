# Use variables for clarity and reuse
CC = gcc
EXE = cypher.exe
SRC = cypher.c
FILE = test.txt

# Default target (optional, for convenience)
all: $(EXE)

# Build rule
$(EXE): $(SRC)
	$(CC) $(SRC) -o $(EXE) -Wall -Wextra -pedantic -std=c99

# Run rule depends on the binary
run: $(EXE)
	./$(EXE) $(FILE)

# Clean up build products
clean:
	rm -f $(EXE)