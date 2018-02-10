default:
	gcc -O3 -Wall -m64 -lz -lz4 -o leftpad_assembler xxhash.c lz4stream.c main.c
