/*
  LZ4io.c - LZ4 File/Stream Interface
  Copyright (C) Yann Collet 2011-2015

  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - LZ4 source repository : https://github.com/lz4/lz4
  - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
/*
  Note : this is stand-alone program.
  It is not part of LZ4 compression library, it is a user code of the LZ4 library.
  - The license of LZ4 library is BSD.
  - The license of xxHash library is BSD.
  - The license of this source file is GPLv2.
*/

#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz4file.h"

/*****************************
*  Constants
*****************************/
#define MAGICNUMBER_SIZE    4
#define LZ4IO_MAGICNUMBER   0x184D2204
#define LZ4FILE_BUFFER_SIZE (64 * (1 << 10))

/**************************************
*  Macros
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static int g_displayLevel = 4;   /* 0 : no display  ; 1: errors  ; 2 : + result + interaction + warnings ; 3 : + progression; 4 : + information */

/**************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, " \n");                                               \
    exit(error);                                                          \
}

size_t _refill_compressed_buffer(lz4File file) {
  // Refill source buffer
  size_t src_read_size = fread(file->compressed_buffer, 1, file->compressed_buffer_size, file->src);
  // printf("_refill_compressed_buffer: src_read_size = %d\n", src_read_size);
  /* can be out because readSize == 0, which could be an fread() error */
  if (ferror(file->src)) {
    EXM_THROW(67, "Read error");
  }
  file->compressed_buffer_tail = file->compressed_buffer;
  return src_read_size;
}

size_t _read_from_compressed_buffer(lz4File file, void *consumer_buffer, size_t consumer_buffer_size) {

  size_t bytes_copied = 0;

  while (!file->eof && bytes_copied < consumer_buffer_size) {

    size_t bytes_left_in_buffer = file->compressed_buffer_size - (file->compressed_buffer_tail - file->compressed_buffer);

    if (bytes_left_in_buffer == 0) {
      size_t compressed_read_size = _refill_compressed_buffer(file);
      if (compressed_read_size == 0) {
        return 0;
      }
      bytes_left_in_buffer = compressed_read_size;
    }

    size_t bytes_to_copy = (bytes_copied + bytes_left_in_buffer > consumer_buffer_size) ? bytes_left_in_buffer - consumer_buffer_size  : bytes_left_in_buffer;

    size_t lz4_src_size_in_consumed_out = bytes_left_in_buffer;
    size_t lz4_dst_size_in_regenerated_out = bytes_to_copy;

//    printf("lz4read: lz4_src_size = %d\n", lz4_src_size_in_consumed_out);
//    printf("lz4read: lz4_dst_size = %d\n", lz4_dst_size_in_regenerated_out);

    LZ4F_errorCode_t lz4_error_code = LZ4F_decompress(file->ctx, consumer_buffer + bytes_copied, &lz4_dst_size_in_regenerated_out, file->compressed_buffer_tail, &lz4_src_size_in_consumed_out, NULL);
    if (LZ4F_isError(lz4_error_code)) {
      EXM_THROW(66, "Decompression error : %s", LZ4F_getErrorName(lz4_error_code));
    }

    if (lz4_error_code == 0) {
      file->eof = true;
    }

//    printf("_read_from_compressed_buffer: lz4_consumed = %d\n", lz4_src_size_in_consumed_out);
//    printf("_read_from_compressed_buffer: lz4_regenerated = %d\n", lz4_dst_size_in_regenerated_out);

    file->compressed_buffer_tail += lz4_src_size_in_consumed_out;
    bytes_copied += lz4_dst_size_in_regenerated_out;

  }

  return bytes_copied;

}

size_t _refill_decompressed_buffer(lz4File file) {
  size_t bytes_read = _read_from_compressed_buffer(file, file->decompressed_buffer, file->decompressed_buffer_size);
  file->decompressed_buffer_tail = file->decompressed_buffer;
  return bytes_read;
}

size_t _read_from_uncompressed_buffer(lz4File file, void *consumer_buffer, size_t consumer_buffer_size) {

  size_t bytes_copied = 0;

  while (!file->eof && bytes_copied < consumer_buffer_size) {

    size_t bytes_left_in_buffer = file->decompressed_buffer_size - (file->decompressed_buffer_tail - file->decompressed_buffer);

    if (bytes_left_in_buffer == 0) {
      size_t decompressed_read_size = _refill_decompressed_buffer(file);
      if (decompressed_read_size == 0) {
        return 0;
      }
      bytes_left_in_buffer = decompressed_read_size;
    }

    size_t bytes_to_copy = (bytes_copied + bytes_left_in_buffer > consumer_buffer_size) ? bytes_left_in_buffer - consumer_buffer_size  : bytes_left_in_buffer;

    memcpy(consumer_buffer + bytes_copied, file->decompressed_buffer_tail, bytes_to_copy);
    file->decompressed_buffer_tail += bytes_to_copy;
    bytes_copied += bytes_to_copy;

  }

  return bytes_copied;

}

lz4File lz4open(const char *path, const char *mode) {

  lz4File file = (lz4File) calloc(1, sizeof(lz4file_t));
  file->src = fopen(path, mode);
  if (file->src == NULL) {
    DISPLAYLEVEL(1, "%s: %s \n", path, strerror(errno));
  }

  /* init */
  LZ4F_errorCode_t error_code = LZ4F_createDecompressionContext(&(file->ctx), LZ4F_VERSION);
  if (LZ4F_isError(error_code)) {
    EXM_THROW(60, "Can't create LZ4F context : %s", LZ4F_getErrorName(error_code));
  }

  /* Allocate Memory */
  file->compressed_buffer_size = LZ4FILE_BUFFER_SIZE;
  file->compressed_buffer = calloc(file->compressed_buffer_size, sizeof(char));
  file->compressed_buffer_tail = file->compressed_buffer + file->compressed_buffer_size;
  file->decompressed_buffer_size = LZ4FILE_BUFFER_SIZE;
  file->decompressed_buffer = calloc(file->decompressed_buffer_size, sizeof(char));
  file->decompressed_buffer_tail = file->decompressed_buffer + file->decompressed_buffer_size;
  if (!file->compressed_buffer || !file->decompressed_buffer) {
    EXM_THROW(61, "Allocation error : not enough memory");
  }

//  unsigned char magic_number[MAGICNUMBER_SIZE];
//  size_t compressed_read_size = _read_from_compressed_buffer(file, magic_number, MAGICNUMBER_SIZE);

//  printf("%d\n", compressed_read_size);
//  printf("%s\n", magic_number);

//  if (compressed_read_size < MAGICNUMBER_SIZE || le32toh(*(uint32_t *) magic_number) != LZ4IO_MAGICNUMBER) {
//    EXM_THROW(44,"Unrecognized header : file cannot be decoded");   /* Wrong magic number at the beginning of 1st stream */
//  }

//  size_t lz4_src_size_in_consumed_out = file->compressed_buffer_size - (file->compressed_buffer_tail - file->compressed_buffer);
//  LZ4F_errorCode_t lz4_error_code = LZ4F_getFrameInfo(file->ctx, &(file->frame_info), file->compressed_buffer_tail, &lz4_src_size_in_consumed_out);
//  if (LZ4F_isError(lz4_error_code)) {
//    EXM_THROW(62, "Header error : %s", LZ4F_getErrorName(lz4_error_code));
//  }

//  file->compressed_buffer_tail += lz4_src_size_in_consumed_out;

  return file;

}

int lz4read(lz4File file, void *consumer_buffer, unsigned int consumer_buffer_size) {

  if (file->eof) {
    return 0;
  }

  size_t total_data_read = 0;
  size_t data_read = 0;

  while(total_data_read < consumer_buffer_size && (data_read = _read_from_uncompressed_buffer(file, consumer_buffer, consumer_buffer_size)) > 0) {
    total_data_read += data_read;
  };

  if (data_read == 0) {
    file->eof = true;
  }

  return data_read;

}

void lz4close(lz4File file) {

  LZ4F_errorCode_t error_code = LZ4F_freeDecompressionContext(file->ctx);
  if (LZ4F_isError(error_code)) EXM_THROW(69, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(error_code));
  free(file->compressed_buffer);

  fclose(file->src);
  free(file);

}

