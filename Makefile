default:
	gcc -fmax-errors=1 -std=gnu99 -DDEBUG -O3 -Wall -m64 -llz4 -o leftpad_assembler lz4file.c main.c

clean:
	rm -f *.o *~ leftpad_assembler
