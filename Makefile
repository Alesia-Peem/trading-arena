CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror

SRC = $(wildcard src/*.c)
LIB_SRC = $(filter-out src/main.c,$(SRC))
TEST_SRC = $(wildcard tests/test_*.c)
TESTS = $(patsubst tests/test_%.c,tests/%_test,$(TEST_SRC))

all: arena

arena: $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC)

tests/%_test: tests/test_%.c $(LIB_SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_SRC)

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

clean:
	rm -f arena tests/*_test

.PHONY: all test clean
