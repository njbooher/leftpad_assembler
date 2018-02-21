default:
	gcc -fmax-errors=1 -std=gnu99 -O0 -g -Wall -march=native -lz -o leftpad_assembler main.c

clean:
	rm -f *.o *~ leftpad_assembler
