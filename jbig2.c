/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
        
    $Id: jbig2.c,v 1.18 2003/02/03 20:04:11 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_symbol_dict.h"

static void *
jbig2_default_alloc (Jbig2Allocator *allocator, size_t size)
{
  return malloc (size);
}

static void
jbig2_default_free (Jbig2Allocator *allocator, void *p)
{
  free (p);
}

static void *
jbig2_default_realloc (Jbig2Allocator *allocator, void *p, size_t size)
{
  return realloc (p, size);
}

static Jbig2Allocator jbig2_default_allocator =
{
  jbig2_default_alloc,
  jbig2_default_free,
  jbig2_default_realloc
};

void *
jbig2_alloc (Jbig2Allocator *allocator, size_t size)
{
  return allocator->alloc (allocator, size);
}

void
jbig2_free (Jbig2Allocator *allocator, void *p)
{
  allocator->free (allocator, p);
}

void *
jbig2_realloc (Jbig2Allocator *allocator, void *p, size_t size)
{
  return allocator->realloc (allocator, p, size);
}

static int
jbig2_default_error(void *data, const char *msg, 
                    Jbig2Severity severity, int32_t seg_idx)
{
    /* report only fatal errors by default */
    if (severity == JBIG2_SEVERITY_FATAL) {
        fprintf(stderr, "jbig2 decoder FATAL ERROR: %s", msg);
        if (seg_idx != -1) fprintf(stderr, " (segment 0x%02x)");
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    
    return 0;
}

int
jbig2_error(Jbig2Ctx *ctx, Jbig2Severity severity, int32_t segment_number,
	     const char *fmt, ...)
{
  char buf[1024];
  va_list ap;
  int n;
  int code;

  va_start (ap, fmt);
  n = vsnprintf (buf, sizeof(buf), fmt, ap);
  va_end (ap);
  if (n < 0 || n == sizeof(buf))
    strncpy (buf, "jbig2_error: error in generating error string", sizeof(buf));
  code = ctx->error_callback (ctx->error_callback_data, buf, severity, segment_number);
  if (severity == JBIG2_SEVERITY_FATAL)
    code = -1;
  return code;
}

Jbig2Ctx *
jbig2_ctx_new (Jbig2Allocator *allocator,
	       Jbig2Options options,
	       Jbig2GlobalCtx *global_ctx,
	       Jbig2ErrorCallback error_callback,
	       void *error_callback_data)
{
  Jbig2Ctx *result;

  if (allocator == NULL)
      allocator = &jbig2_default_allocator;
  if (error_callback == NULL)
      error_callback = &jbig2_default_error;

  result = (Jbig2Ctx *)jbig2_alloc(allocator, sizeof(Jbig2Ctx));
  if (result == NULL) {
    error_callback(error_callback_data, "initial context allocation failed!",
                    JBIG2_SEVERITY_FATAL, -1);
    return result;
  }
  
  result->allocator = allocator;
  result->options = options;
  result->global_ctx = (const Jbig2Ctx *)global_ctx;
  result->error_callback = error_callback;
  result->error_callback_data = error_callback_data;

  result->state = (options & JBIG2_OPTIONS_EMBEDDED) ?
    JBIG2_FILE_SEQUENTIAL_HEADER :
    JBIG2_FILE_HEADER;

  result->buf = NULL;
  
  result->n_segments = 0;
  result->n_segments_max = 16;
  result->segments = (Jbig2Segment **)jbig2_alloc(allocator, result->n_segments_max * sizeof(Jbig2Segment *));
  result->segment_index = 0;

  result->current_page = 0;
  result->max_page_index = 4;
  result->pages = (Jbig2Page *)jbig2_alloc(allocator, result->max_page_index * sizeof(Jbig2Page));
  {
    int index;
    for (index = 0; index < result->max_page_index; index++) {
        result->pages[index].state = JBIG2_PAGE_FREE;
        result->pages[index].number = 0;
        result->pages[index].image = NULL;
    }
  }

  return result;
}

int32_t
jbig2_get_int32 (const byte *buf)
{
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

int16_t
jbig2_get_int16 (const byte *buf)
{
  return (buf[0] << 8) | buf[1];
}


/**
 * jbig2_data_in: submit data for decoding
 * @ctx: The jbig2dec decoder context
 * @data: a pointer to the data buffer
 * @size: the size of the data buffer in bytes
 *
 * Copies the specified data into internal storage and attempts
 * to (continue to) parse it as part of a jbig2 data stream.
 *
 * Return code: 0 on success
 **/
int
jbig2_data_in (Jbig2Ctx *ctx, const unsigned char *data, size_t size)
{
  const size_t initial_buf_size = 1024;

  if (ctx->buf == NULL)
    {
      size_t buf_size = initial_buf_size;

      do
	buf_size <<= 1;
      while (buf_size < size);
      ctx->buf = (byte *)jbig2_alloc (ctx->allocator, size);
      ctx->buf_size = buf_size;
      ctx->buf_rd_ix = 0;
      ctx->buf_wr_ix = 0;
    }
  else if (ctx->buf_wr_ix + size > ctx->buf_size)
    {
      if (ctx->buf_rd_ix <= (ctx->buf_size >> 1) &&
	  ctx->buf_wr_ix - ctx->buf_rd_ix + size <= ctx->buf_size)
        {
	  memcpy (ctx->buf, ctx->buf + ctx->buf_rd_ix,
		  ctx->buf_wr_ix - ctx->buf_rd_ix);
	}
      else
	{
	  byte *buf;
	  size_t buf_size = initial_buf_size;
	  
	  do
	    buf_size <<= 1;
	  while (buf_size < ctx->buf_wr_ix - ctx->buf_rd_ix + size);
	  buf = (byte *)jbig2_alloc (ctx->allocator, buf_size);
	  memcpy (buf, ctx->buf + ctx->buf_rd_ix,
		  ctx->buf_wr_ix - ctx->buf_rd_ix);
	  jbig2_free (ctx->allocator, ctx->buf);
	  ctx->buf = buf;
	  ctx->buf_size = buf_size;
	}
      ctx->buf_wr_ix -= ctx->buf_rd_ix;
      ctx->buf_rd_ix = 0;
    }
  memcpy (ctx->buf + ctx->buf_wr_ix, data, size);
  ctx->buf_wr_ix += size;

  /* data has now been added to buffer */

  for (;;)
    {
      const byte jbig2_id_string[8] = { 0x97, 0x4a, 0x42, 0x32, 0x0d, 0x0a, 0x1a, 0x0a };
      Jbig2Segment *segment;
      size_t header_size;
      int code;

      switch (ctx->state)
	{
	case JBIG2_FILE_HEADER:
          /* D.4.1 */
	  if (ctx->buf_wr_ix - ctx->buf_rd_ix < 9)
	    return 0;
	  if (memcmp(ctx->buf + ctx->buf_rd_ix, jbig2_id_string, 8))
	    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
			       "Not a JBIG2 file header");
          /* D.4.2 */
	  ctx->file_header_flags = ctx->buf[ctx->buf_rd_ix + 8];
          if (ctx->file_header_flags & 0xFC) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1,
                "reserved bits (2-7) of file header flags are not zero (0x%02x)", ctx->file_header_flags);
          }
          /* D.4.3 */
	  if (!(ctx->file_header_flags & 2)) /* number of pages is known */
	    {
	      if (ctx->buf_wr_ix - ctx->buf_rd_ix < 13)
		return 0;
	      ctx->n_pages = jbig2_get_int32(ctx->buf + ctx->buf_rd_ix + 9);
	      ctx->buf_rd_ix += 13;
              if (ctx->n_pages == 1)
                jbig2_error(ctx, JBIG2_SEVERITY_INFO, -1, "file header indicates a single page document");
              else
                jbig2_error(ctx, JBIG2_SEVERITY_INFO, -1, "file header indicates a %d page document", ctx->n_pages);
	    }
	  else /* number of pages not known */
            {
              ctx->n_pages=0;
	      ctx->buf_rd_ix += 9;
            }
          /* determine the file organization based on the flags - D.4.2 again */
	  if (ctx->file_header_flags & 1)
	    {
	      ctx->state = JBIG2_FILE_SEQUENTIAL_HEADER;
              jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "file header indicates sequential organization");
	    }
	  else
	    {
	      ctx->state = JBIG2_FILE_RANDOM_HEADERS; 
              jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "file header indicates random-access organization");

            }
	  break;
	case JBIG2_FILE_SEQUENTIAL_HEADER:
	case JBIG2_FILE_RANDOM_HEADERS:
	  segment = jbig2_parse_segment_header(ctx, ctx->buf + ctx->buf_rd_ix,
					  ctx->buf_wr_ix - ctx->buf_rd_ix,
					  &header_size);
	  if (segment == NULL)
	    return 0; /* need more data */
	  ctx->buf_rd_ix += header_size;

	  if (ctx->n_segments == ctx->n_segments_max)
	    ctx->segments = (Jbig2Segment **)jbig2_realloc(ctx->allocator,
                ctx->segments, (ctx->n_segments_max <<= 2) * sizeof(Jbig2Segment *));

	  ctx->segments[ctx->n_segments++] = segment;
	  if (ctx->state == JBIG2_FILE_RANDOM_HEADERS)
	    {
	      if ((segment->flags & 63) == 51) /* end of file */
		ctx->state = JBIG2_FILE_RANDOM_BODIES;
	    }
	  else /* JBIG2_FILE_SEQUENTIAL_HEADER */
	    ctx->state = JBIG2_FILE_SEQUENTIAL_BODY;
	  break;
	case JBIG2_FILE_SEQUENTIAL_BODY:
	case JBIG2_FILE_RANDOM_BODIES:
	  segment = ctx->segments[ctx->segment_index];
	  if (segment->data_length > ctx->buf_wr_ix - ctx->buf_rd_ix)
	    return 0; /* need more data */
	  code = jbig2_parse_segment(ctx, segment, ctx->buf + ctx->buf_rd_ix);
	  ctx->buf_rd_ix += segment->data_length;
	  ctx->segment_index++;
	  if (ctx->state == JBIG2_FILE_RANDOM_BODIES)
	    {
	      if (ctx->segment_index == ctx->n_segments)
		ctx->state = JBIG2_FILE_EOF;
	    }
	  else /* JBIG2_FILE_SEQUENCIAL_BODY */
	    {
	      ctx->state = JBIG2_FILE_SEQUENTIAL_HEADER;
	    }
	  if (code < 0)
	    {
	      ctx->state = JBIG2_FILE_EOF;
	      return code;
	    }
	  break;
	case JBIG2_FILE_EOF:
	  if (ctx->buf_rd_ix == ctx->buf_wr_ix)
	    return 0;
	  return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1,
		      "Garbage beyond end of file");
	}
    }
  return 0;
}

void
jbig2_ctx_free (Jbig2Ctx *ctx)
{
  Jbig2Allocator *ca = ctx->allocator;
  int i;

  jbig2_free(ca, ctx->buf);
  if (ctx->segments != NULL)
    {
      for (i = ctx->segment_index; i < ctx->n_segments; i++)
	jbig2_free_segment(ctx, ctx->segments[i]);
      jbig2_free(ca, ctx->segments);
    }

  /* todo: free pages */

  jbig2_free(ca, ctx);
}

Jbig2GlobalCtx *jbig2_make_global_ctx (Jbig2Ctx *ctx)
{
  return (Jbig2GlobalCtx *)ctx;
}

void jbig2_global_ctx_free(Jbig2GlobalCtx *global_ctx)
{
  jbig2_ctx_free((Jbig2Ctx *)global_ctx);
}


/* I'm not committed to keeping the word stream interface. It's handy
   when you think you may be streaming your input, but if you're not
   (as is currently the case), it just adds complexity.
*/

typedef struct {
  Jbig2WordStream super;
  const byte *data;
  size_t size;
} Jbig2WordStreamBuf;

static uint32_t
jbig2_word_stream_buf_get_next_word(Jbig2WordStream *self, int offset)
{
  Jbig2WordStreamBuf *z = (Jbig2WordStreamBuf *)self;
  const byte *data = z->data;
  uint32_t result;

  if (offset + 4 < z->size)
    result = (data[offset] << 24) | (data[offset + 1] << 16) |
      (data[offset + 2] << 8) | data[offset + 3];
  else
    {
      int i;

      result = 0;
      for (i = 0; i < z->size - offset; i++)
	result |= data[offset + i] << ((3 - i) << 3);
    }
  return result;
}

Jbig2WordStream *
jbig2_word_stream_buf_new(Jbig2Ctx *ctx, const byte *data, size_t size)
{
  Jbig2WordStreamBuf *result = (Jbig2WordStreamBuf *)jbig2_alloc(ctx->allocator, sizeof(Jbig2WordStreamBuf));

  result->super.get_next_word = jbig2_word_stream_buf_get_next_word;
  result->data = data;
  result->size = size;

  return &result->super;
}

void
jbig2_word_stream_buf_free(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
  jbig2_free(ctx->allocator, ws);
}

