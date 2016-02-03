BIN = main
SRC = src/*.c
CFLAGS = -Wall -Wextra -Werror -O2

ifeq ($(NDEBUG), 1)
  CFLAGS += -DNDEBUG
endif

.PHONY: clean

$(BIN): $(SRC)
	$(CC) -std=c99 $(CFLAGS) $< -o $(BIN)

clean:
	rm -f $(BIN)

test: $(BIN)
	valgrind --leak-check=yes ./$(BIN)
