CC = gcc
CFLAGS = -Iinclude 
SRC = src/main.c src/helpers.c src/hls.c src/db.c
OBJ = $(SRC:src/%.c=build/%.o)
OUT = build/spd

$(OUT): $(OBJ)
	$(CC) -o $(OUT) $(OBJ) -lcurl -lsqlite3

build/%.o: src/%.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f build/** $(OUT)
	clear
