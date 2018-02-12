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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lz4file.h"

/*****************************
*  Constants
*****************************/
#define MAGICNUMBER_SIZE    4
#define LZ4IO_MAGICNUMBER   0x184D2204

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

/***************************************
*   Legacy Compression
***************************************/

static unsigned LZ4IO_readLE32 (const void* s)
{
    const unsigned char* const srcPtr = (const unsigned char*)s;
    unsigned value32 = srcPtr[0];
    value32 += (srcPtr[1]<<8);
    value32 += (srcPtr[2]<<16);
    value32 += ((unsigned)srcPtr[3])<<24;
    return value32;
}

/* unoptimized version; solves endianess & alignment issues */
static void LZ4IO_writeLE32(void* p, unsigned value32)
{
    unsigned char* dstPtr = (unsigned char*)p;
    dstPtr[0] = (unsigned char)value32;
    dstPtr[1] = (unsigned char)(value32 >> 8);
    dstPtr[2] = (unsigned char)(value32 >> 16);
    dstPtr[3] = (unsigned char)(value32 >> 24);
}

lz4File lz4open(const char *path, const char *mode) {

  lz4File file = (lz4File) calloc(1, sizeof(lz4file_t));
  file->src = fopen(path, mode);
  if (file->src == NULL ) DISPLAYLEVEL(1, "%s: %s \n", path, strerror(errno));

  /* init */
  LZ4F_errorCode_t error_code = LZ4F_createDecompressionContext(&(file->ctx), LZ4F_VERSION);
  if (LZ4F_isError(error_code)) EXM_THROW(60, "Can't create LZ4F context : %s", LZ4F_getErrorName(error_code));

  /* Allocate Memory */
  file->src_buf_size = LZ4FILE_BUFFER_SIZE;
  file->src_buf = malloc(file->src_buf_size);
  if (!file->src_buf) EXM_THROW(61, "Allocation error : not enough memory");

  unsigned char mn_store[MAGICNUMBER_SIZE];

  size_t const nb_read_bytes = fread(mn_store, 1, MAGICNUMBER_SIZE, file->src);
  if (nb_read_bytes == 0 || nb_read_bytes != MAGICNUMBER_SIZE) EXM_THROW(40, "Unrecognized header : Magic Number unreadable");

  if (LZ4IO_readLE32(mn_store) != LZ4IO_MAGICNUMBER) {
    EXM_THROW(44,"Unrecognized header : file cannot be decoded");   /* Wrong magic number at the beginning of 1st stream */
  }

  /* Init feed with magic number (already consumed from FILE* sFile) */
  {
      size_t in_size = MAGICNUMBER_SIZE;
      char out[1];
      size_t out_size = 0;
      LZ4IO_writeLE32(file->src_buf, LZ4IO_MAGICNUMBER);
      file->next_to_load = LZ4F_decompress(file->ctx, out, &out_size, file->src_buf, &in_size, NULL);
      if (LZ4F_isError(file->next_to_load)) EXM_THROW(62, "Header error : %s", LZ4F_getErrorName(file->next_to_load));
  }

  return file;

}

int lz4read(lz4File file, void *buf, unsigned int len) {

  int num_bytes_read = 0;

  while(num_bytes_read < len && file->next_to_load) {

    size_t read_size;
    size_t pos = 0;
    size_t decoded_bytes = len;

    /* Read input */
    if (file->next_to_load > file->src_buf_size) file->next_to_load = file->src_buf_size;
    read_size = fread(file->src_buf, 1, file->next_to_load, file->src);
    if (!read_size) break; /* reached end of file or stream */

    while ((pos < read_size) || (decoded_bytes == len)) {  /* still to read, or still to flush */

      /* Decode Input (at least partially) */
      size_t remaining = read_size - pos;
      decoded_bytes = len;
      file->next_to_load = LZ4F_decompress(file->ctx, buf, &decoded_bytes, (char*)(file->src_buf)+pos, &remaining, NULL);
      if (LZ4F_isError(file->next_to_load)) EXM_THROW(66, "Decompression error : %s", LZ4F_getErrorName(file->next_to_load));
      pos += remaining;

      /* Write Block */
      if (decoded_bytes) {
        num_bytes_read += decoded_bytes;
      }

      if (!file->next_to_load) break;

    }

  }

  /* can be out because readSize == 0, which could be an fread() error */
  if (ferror(file->src)) EXM_THROW(67, "Read error");

  return num_bytes_read;

}

void lz4close(lz4File file) {

  LZ4F_errorCode_t error_code = LZ4F_freeDecompressionContext(file->ctx);
  if (LZ4F_isError(error_code)) EXM_THROW(69, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(error_code));
  free(file->src_buf);

  fclose(file->src);
  free(file);

}

