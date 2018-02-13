#ifndef LZ4FILE_H
#define LZ4FILE_H

#include <lz4frame.h>

#define LZ4FILE_BUFFER_SIZE (64 * (1 << 10))

typedef struct {
  FILE *src;
  bool src_eof;
  void *src_buf;
  size_t src_buf_size;
  size_t src_buf_consumed;
  LZ4F_decompressionContext_t ctx;
  LZ4F_frameInfo_t frame_info;
  LZ4F_errorCode_t lz4_next_read_size;
} lz4file_t;

typedef lz4file_t* lz4File;

lz4File lz4open(const char *path, const char *mode);
int lz4read(lz4File file, void *buf, unsigned int len);
void lz4close(lz4File file);

#endif
