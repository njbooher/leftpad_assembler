ZLIB_LIBRARY ?= -lz
ZLIB_PATH ?= .

ZSTDLIBDIR = zstd/lib
ZSTDLIBRARY = $(ZSTDLIBDIR)/libzstd.a
ZLIBWRAPPER_PATH = zstd/zlibWrapper
GZFILES = $(ZLIBWRAPPER_PATH)/gzclose.o $(ZLIBWRAPPER_PATH)/gzlib.o $(ZLIBWRAPPER_PATH)/gzread.o $(ZLIBWRAPPER_PATH)/gzwrite.o
EXAMPLE_PATH = examples
PROGRAMS_PATH = zstd/programs

CPPFLAGS = -DXXH_NAMESPACE=ZSTD_ -I$(ZLIB_PATH) -I$(PROGRAMS_PATH)       \
           -I$(ZSTDLIBDIR) -I$(ZSTDLIBDIR)/common -I$(ZLIBWRAPPER_PATH)
CFLAGS  ?= $(MOREFLAGS) -O3 -std=gnu99
CFLAGS  += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wswitch-enum \
           -Wdeclaration-after-statement -Wstrict-prototypes -Wundef     \
           -Wstrict-aliasing=1

all: leftpad_assembler

leftpad_assembler: main.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o $(GZFILES) $(ZSTDLIBRARY)
#	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	$(CC) $(LDFLAGS) $^ $(ZSTDLIBRARY) $(ZLIB_LIBRARY) -o $@

#leftpad_assembler:
#	gcc -c -fmax-errors=1 -std=gnu99 -O3 -Wall -m64 -DZWRAP_USE_ZSTD=1 -DXXH_NAMESPACE=ZSTD_ -I$(ZLIB_PATH) -I$(PROGRAMS_PATH) -I$(ZSTDLIBDIR) -I$(ZSTDLIBDIR)/common -I$(ZLIBWRAPPER_PATH) -o leftpad_assembler.o main.c
#	gcc leftpad_assembler.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(ZLIBWRAPPER_PATH)/gzclose.o $(ZLIBWRAPPER_PATH)/gzlib.o $(ZLIBWRAPPER_PATH)/gzread.o $(ZLIBWRAPPER_PATH)/gzwrite.o $(ZSTDLIBRARY) -lz -o leftpad_assembler

$(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

$(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -DZWRAP_USE_ZSTD=1 -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

$(ZSTDLIBDIR)/libzstd.a:
	$(MAKE) -C $(ZSTDLIBDIR) libzstd.a

$(ZSTDLIBDIR)/libzstd.so:
	$(MAKE) -C $(ZSTDLIBDIR) libzstd

clean:
	rm -f *.o *~ leftpad_assembler
