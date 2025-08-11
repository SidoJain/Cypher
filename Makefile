CC = gcc
EXE = cypher.exe
SRC = cypher.c
FILE ?=

# Detect OS
ifeq ($(OS),Windows_NT)
    CLEAR_CMD = cls
	RM_CMD = del /f
else
    CLEAR_CMD = clear
	RM_CMD = rm -f
endif

$(EXE): $(SRC)
	$(CC) $(SRC) -o $(EXE) -Wall -Wextra -pedantic -std=c99

run: $(EXE)
	$(CLEAR_CMD)
	./$(EXE) $(FILE)

clean:
	$(RM_CMD) $(EXE)