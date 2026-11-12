CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -pedantic -g
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
BIN     = minifs

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c src/sistema_arquivos.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

valgrind: $(BIN)
	valgrind --leak-check=full --show-leak-kinds=all ./$(BIN)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: run valgrind clean
