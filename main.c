#include <stdio.h>
#include "lz4stream.h"
#include "kseq.h"
KSEQ_INIT(lz4stream*, lz4stream_read)

int main(int argc, char *argv[]) {

    lz4stream *input_file = lz4stream_open_read(argv[1]);
    kseq_t *seq = kseq_init(input_file);

    unsigned int max_length = 0;

    while (kseq_read(seq) >= 0) {
        if (seq->seq.l > max_length) {
            max_length = seq->seq.l;
         }
    }

    printf("%d", max_length);

    kseq_destroy(seq);
    gzclose(input_file);

    return 0;

}
