#include <zlib.h>
#include <stdio.h>
#include "kseq.h"
KSEQ_INIT(gzFile, gzread)

int main(int argc, char *argv[]) {

    gzFile input_file = gzopen(argv[1], "r");
    kseq_t *seq = kseq_init(input_file);

    while (kseq_read(seq) >= 0) {
        puts(seq->seq.s);
    }

    kseq_destroy(seq);
    gzclose(input_file);

    return 0;

}
