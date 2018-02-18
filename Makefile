default:
	gcc -fmax-errors=1 -std=gnu99 -O0 -g -Wall -m64 -lz -msse4.2 -o leftpad_assembler main.c

clean:
	rm -f *.o *~ leftpad_assembler
