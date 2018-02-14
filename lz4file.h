#ifndef LZ4FILE_H
#define LZ4FILE_H

#include <stdbool.h>
#include <lz4frame.h>

typedef struct {
  FILE *src;
  bool eof;
  char *compressed_buffer;
  size_t compressed_buffer_size;
  char *compressed_buffer_tail;
  char *decompressed_buffer;
  size_t decompressed_buffer_size;
  char *decompressed_buffer_tail;
  size_t decompressed_bytes;
  LZ4F_decompressionContext_t ctx;
  LZ4F_frameInfo_t frame_info;
} lz4file_t;

typedef lz4file_t* lz4File;

lz4File lz4open(const char *path, const char *mode);
int lz4read(lz4File file, void *consumer_buffer, unsigned int consumer_buffer_size);
void lz4close(lz4File file);

#endif
