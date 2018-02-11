#include <zlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "kseq.h"
#include "kvec.h"
#include "seqtk.h"
KSEQ_INIT(gzFile, gzread)

typedef struct
{
  uint8_t anchor_pos;
  char seq_str[256];
  uint8_t seq_str_len;
} match;

int compare_matches(const void *a, const void *b) {
  uint8_t a_anchor_pos = ((match *) a)->anchor_pos;
  uint8_t b_anchor_pos = ((match *) b)->anchor_pos;
  if (a_anchor_pos < b_anchor_pos) {
    return -1;
  } else if (a_anchor_pos == b_anchor_pos) {
    return 0;
  } else {
    return 1;
  }
}

int main(int argc, char *argv[]) {

  char *anchor_str = argv[2];
  uint8_t anchor_str_len = strlen(anchor_str);

  char *rev_anchor_str = calloc(sizeof(char), anchor_str_len);
  strcpy(rev_anchor_str, anchor_str);
  reverse_complement(rev_anchor_str, anchor_str_len);

  gzFile input_file = gzopen(argv[1], "r");
  kseq_t *seq = kseq_init(input_file);

  uint8_t read_pad = 0;

  kvec_t(match) matching_reads;
  kv_init(matching_reads);

  while (kseq_read(seq) >= 0) {

    char *anchor_pos = strstr(seq->seq.s, anchor_str);

    if (anchor_pos == NULL) {
      if (strstr(seq->seq.s, rev_anchor_str) != NULL) {
        reverse_complement(seq->seq.s, seq->seq.l);
        anchor_pos = strstr(seq->seq.s, anchor_str);
      }
    }

    if (anchor_pos != NULL) {
      match matching_read = { .anchor_pos = (anchor_pos - seq->seq.s), .seq_str_len = seq->seq.l };
      strcpy(matching_read.seq_str, seq->seq.s);
      kv_push(match, matching_reads, matching_read);
      read_pad = (read_pad > matching_read.anchor_pos) ? read_pad : matching_read.anchor_pos;
    }

  }

  qsort(matching_reads.a, matching_reads.n, sizeof(match), compare_matches);

  printf("%*s\n", read_pad + anchor_str_len, anchor_str);

  while (kv_size(matching_reads) > 0) {
    match matching_read = kv_pop(matching_reads);
    uint8_t aligned_read_start_pos = (read_pad > matching_read.anchor_pos) ? read_pad - matching_read.anchor_pos : 0;
    printf("%*s\n", aligned_read_start_pos + matching_read.seq_str_len, matching_read.seq_str);
  }

  kv_destroy(matching_reads);

  kseq_destroy(seq);
  gzclose(input_file);

  return 0;

}
