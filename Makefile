default:
	gcc -O3 -Wall -m64 -lz -o leftpad_assembler main.c

clean:
	rm -f *.o *~
