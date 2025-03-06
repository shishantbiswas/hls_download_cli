CC = gcc
CFLAGS = -Iinclude -g -std=c11  -pedantic -Wall -Wextra -Werror -pedantic -D_GNU_SOURCE
SRC = src/main.c src/helpers.c src/hls.c 
OBJ = $(SRC:src/%.c=build/%.o)
OUT = build/spd

$(OUT): $(OBJ)
	$(CC) -O -o $(OUT) $(OBJ) -lcurl -pthread 
build/%.o: src/%.c
	$(CC)  -c $(CFLAGS) $< -o $@

clean:
	rm -f build/** $(OUT)
	clear
