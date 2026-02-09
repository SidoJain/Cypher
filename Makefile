CC = gcc
EXE = cypher
SRC = cypher.c
FILE ?=

$(EXE): $(SRC)
	$(CC) $(SRC) -o $(EXE) -Wall -Wextra -pedantic -std=c99

run: $(EXE)
	./$(EXE) $(FILE)

clean:
	rm -f $(EXE)