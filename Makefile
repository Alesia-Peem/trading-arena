CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror

# main.c (terminal game) and server.c (web game) each have their own main(),
# so they are kept out of LIB_SRC, the shared game code.
LIB_SRC = $(filter-out src/main.c src/server.c,$(wildcard src/*.c))
HEADERS = $(wildcard src/*.h)
TEST_SRC = $(wildcard tests/test_*.c)
TESTS = $(patsubst tests/test_%.c,tests/%_test,$(TEST_SRC))

all: arena arena-server

# Terminal version:  ./arena [seed]
arena: src/main.c $(LIB_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ src/main.c $(LIB_SRC)

# Web version:  ./arena-server [port]   then open http://localhost:8080
arena-server: src/server.c $(LIB_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ src/server.c $(LIB_SRC)

tests/%_test: tests/test_%.c $(LIB_SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_SRC)

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

# Build the server (if needed) and start it.
web: arena-server
	./arena-server

clean:
	rm -f arena arena-server tests/*_test

.PHONY: all test web clean
