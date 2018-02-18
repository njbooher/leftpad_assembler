#include <nmmintrin.h>
#include <mm_malloc.h>
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

  uint8_t anchor_str_len = strlen(argv[2]);

  if (anchor_str_len > 127) {
    exit(1);
  }

  char *anchor_str = _mm_malloc(128 * sizeof(char), 16);
  memcpy(anchor_str, argv[2], anchor_str_len);
  anchor_str[anchor_str_len] = '\0';
  __m128i packed_anchor_str = _mm_load_si128(anchor_str);

//  char *rev_anchor_str = _mm_malloc(128 * sizeof(char), 16);
//  memcpy(rev_anchor_str, anchor_str, anchor_str_len);
//  reverse_complement(rev_anchor_str, anchor_str_len);
//  __m128i packed_rev_anchor_str = _mm_load_si128(rev_anchor_str);

  const int imm = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ORDERED | _SIDD_BIT_MASK;

  gzFile input_file = gzopen(argv[1], "r");
  kseq_t *seq = kseq_init(input_file);

  uint8_t read_pad = 0;

  kvec_t(match) matching_reads;
  kv_init(matching_reads);

  while (kseq_read(seq) >= 0) {

//    __m128i haystack = _mm_load_si128(seq->seq.s);

    int anchor_pos = _mm_cmpestri(_mm_load_si128(seq->seq.s), 128, packed_anchor_str, anchor_str_len, imm);
//    int anchor_pos2 = _mm_cmpestri(_mm_load_si128(seq->seq.s + 128), 128, packed_anchor_str, anchor_str_len, imm);

//    int anchor_pos = anchor_pos1;

//    if (anchor_pos == 0)
//    if (!anchor_pos) {

//      if (anchor_pos) {
//        anchor_pos += 128;
//      }
//    }

    if (anchor_pos != 0) {
      match matching_read = { .anchor_pos = anchor_pos, .seq_str_len = seq->seq.l };
      strcpy(matching_read.seq_str, seq->seq.s);
      kv_push(match, matching_reads, matching_read);
      read_pad = (read_pad > matching_read.anchor_pos) ? read_pad : matching_read.anchor_pos;
    }

  }

//  qsort(matching_reads.a, matching_reads.n, sizeof(match), compare_matches);

  printf("%*s\n", read_pad + anchor_str_len, anchor_str);

  while (kv_size(matching_reads) > 0) {
    match matching_read = kv_pop(matching_reads);
    uint8_t aligned_read_start_pos = 0;
    //uint8_t aligned_read_start_pos = (read_pad > matching_read.anchor_pos) ? read_pad - matching_read.anchor_pos : 0;
    printf("%*s\n", aligned_read_start_pos + matching_read.seq_str_len, matching_read.seq_str);
  }

  kv_destroy(matching_reads);

//  _mm_free(rev_anchor_str);
  _mm_free(anchor_str);

  kseq_destroy(seq);
  gzclose(input_file);

  return 0;

}
