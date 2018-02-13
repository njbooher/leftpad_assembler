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
  file->src_buf_size = LZ4FILE_BUFFER_SIZE;
  file->src_buf = malloc(file->src_buf_size);
  if (!file->src_buf) EXM_THROW(61, "Allocation error : not enough memory");

  size_t src_read_size = fread(file->src_buf, 1, file->src_buf_size, file->src);
  file->src_eof = (src_read_size == 0);

  if (src_read_size < MAGICNUMBER_SIZE) {
    EXM_THROW(40, "Unrecognized header : Magic Number unreadable");
  }

  if (le32toh(*(uint32_t *)file->src_buf) != LZ4IO_MAGICNUMBER) {
    EXM_THROW(44,"Unrecognized header : file cannot be decoded");   /* Wrong magic number at the beginning of 1st stream */
  }

  size_t lz4_src_size_in_consumed_out = file->src_buf_size - file->src_buf_consumed;
  file->lz4_next_read_size = LZ4F_getFrameInfo(file->ctx, &(file->frame_info), (char*)(file->src_buf) + file->src_buf_consumed, &lz4_src_size_in_consumed_out);
  file->src_buf_consumed += lz4_src_size_in_consumed_out;
  if (LZ4F_isError(file->lz4_next_read_size)) EXM_THROW(62, "Header error : %s", LZ4F_getErrorName(file->lz4_next_read_size));

  return file;

}

int lz4read(lz4File file, void *buf, unsigned int len) {

  if (file->src_eof && file->lz4_next_read_size == 0) return 0;

  if (file->src_buf_consumed == file->src_buf_size) {
    // Refill source buffer
    size_t src_read_size_wanted = (file->lz4_next_read_size > file->src_buf_size) ? file->src_buf_size : file->lz4_next_read_size;
    size_t src_read_size = fread(file->src_buf, 1, src_read_size_wanted, file->src);
    /* can be out because readSize == 0, which could be an fread() error */
    if (ferror(file->src)) EXM_THROW(67, "Read error");
    file->src_eof = (src_read_size == 0);
    file->src_buf_consumed = 0;
  }

  // There's stuff left over in the source buffer to decompress
  size_t lz4_src_size_in_consumed_out = file->src_buf_size - file->src_buf_consumed;
  size_t lz4_dst_size_in_regenerated_out = len;
  file->lz4_next_read_size = LZ4F_decompress(file->ctx, buf, &lz4_dst_size_in_regenerated_out, (char*)(file->src_buf)+file->src_buf_consumed, &lz4_src_size_in_consumed_out, NULL);
  file->src_buf_consumed += lz4_src_size_in_consumed_out;
  if (LZ4F_isError(file->lz4_next_read_size)) EXM_THROW(66, "Decompression error : %s", LZ4F_getErrorName(file->lz4_next_read_size));
  return lz4_dst_size_in_regenerated_out;

}

void lz4close(lz4File file) {

  LZ4F_errorCode_t error_code = LZ4F_freeDecompressionContext(file->ctx);
  if (LZ4F_isError(error_code)) EXM_THROW(69, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(error_code));
  free(file->src_buf);

  fclose(file->src);
  free(file);

}

