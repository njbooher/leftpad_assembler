default:
	gcc -fmax-errors=1 -std=gnu99 -O3 -Wall -m64 -lz -o leftpad_assembler main.c

clean:
	rm -f *.o *~ leftpad_assembler
