/*
    jbig2dec
    
    Copyright (c) 2001-2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_page.c,v 1.14 2002/08/05 22:46:00 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_priv.h"

#ifdef OUTPUT_PBM
#include "jbig2_image.h"
#endif

/* dump the page struct info */
static void
dump_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, Jbig2Page *page)
{
    if (page->x_resolution == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
            "page %d image is %dx%d (unknown res)", page->number,
            page->width, page->height);
    } else if (page->x_resolution == page->y_resolution) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
            "page %d image is %dx%d (%d ppm)", page->number,
            page->width, page->height,
            page->x_resolution);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
            "page %d image is %dx%d (%dx%d ppm)", page->number,
            page->width, page->height,
            page->x_resolution, page->y_resolution);
    }
    if (page->striped) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
            "\tmaximum stripe size: %d", page->stripe_size);
    }
}

/**
 * jbig2_read_page_info: parse page info segment
 *
 * parse the page info segment data and fill out a corresponding
 * Jbig2Page struct is returned, including allocating an image
 * buffer for the page (or the first stripe)
 **/
int
jbig2_parse_page_info (Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2Page *page;

    /* a new page info segment implies the previous page is finished */
    page = &(ctx->pages[ctx->current_page]);
    if ((page->number != 0) &&
            ((page->state == JBIG2_PAGE_NEW) || (page->state == JBIG2_PAGE_FREE))) {
        page->state = JBIG2_PAGE_COMPLETE;
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "unexpected page info segment, marking previous page finished");
    }
        
    /* find a free page */
    {
        int index, j;
        index = ctx->current_page;
        while (ctx->pages[index].state != JBIG2_PAGE_FREE) {
            index++;
            if (index >= ctx->max_page_index) { // FIXME: should also look for freed pages?
                /* grow the list */
                jbig2_realloc(ctx->allocator, ctx->pages, (ctx->max_page_index <<= 2) * sizeof(Jbig2Page));
                for (j=index; j < ctx->max_page_index; j++) {
                    /* note to raph: and look, it gets worse! */
                    ctx->pages[j].state = JBIG2_PAGE_FREE;
                    ctx->pages[j].number = 0;
                    ctx->pages[j].image = NULL;

                }
            }
        }
        page = &(ctx->pages[index]);
        ctx->current_page = index;
        page->state = JBIG2_PAGE_NEW;
        page->number = segment->page_association;
    }
    
    // FIXME: would be nice if we tried to work around this
    if (segment->data_length < 19) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "segment too short");
    }
    
    /* 7.4.8.x */
    page->width = jbig2_get_int32(segment_data);
    page->height = jbig2_get_int32(segment_data + 4);
    
    page->x_resolution = jbig2_get_int32(segment_data + 8);
    page->y_resolution = jbig2_get_int32(segment_data + 12);
    page->flags = segment_data[16];

    /* 7.4.8.6 */
    {
        int16_t striping = jbig2_get_int16(segment_data +17);
        if (striping & 0x8000) {
            page->striped = TRUE;
            page->stripe_size = striping & 0x7FFF;
        } else {
            page->striped = FALSE;
            page->stripe_size = 0;	/* would page->height be better? */
        }
    }
    if (page->height == 0xFFFFFFFF && page->striped == FALSE) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "height is unspecified but page is not markes as striped");
        page->striped = TRUE;
    }
    
    if (segment->data_length > 19) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "extra data in segment");
    }
    
    dump_page_info(ctx, segment, page);

    /* allocate an approprate page image buffer */
    /* 7.4.8.2 */
    if (page->height == 0xFFFFFFFF) {
        page->image = jbig2_image_new(ctx, page->width, page->stripe_size);
    } else {
        page->image = jbig2_image_new(ctx, page->width, page->height);
    }
    if (page->image == NULL) {
        jbig2_free(ctx->allocator, page);
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "failed to allocate buffer for page image");
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "allocated %dx%d page image (%d bytes)",
            page->image->width, page->image->height,
            page->image->stride*page->image->height);
    }
    
    return 0;
}

/**
 * jbig2_complete_page: complete a page image
 *
 * called upon seeing an 'end of page' segment, this routine
 * marks a page as completed. final compositing and output
 * of the page image will also happen from here (NYI)
 **/
int
jbig2_complete_page (Jbig2Ctx *ctx)
{
    ctx->pages[ctx->current_page].state = JBIG2_PAGE_COMPLETE;

    return 0;
}

/**
 * jbig2_parse_end_of_page: parse an end of page segment
 **/
int
jbig2_parse_end_of_page(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    uint32_t page_number = ctx->pages[ctx->current_page].number;

    if (segment->page_association != page_number) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "end of page marker for page %d doesn't match current page number %d",
            segment->page_association, page_number);
    }
    
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
        "end of page %d", page_number);

    jbig2_complete_page(ctx);
    
#ifdef OUTPUT_PBM
    jbig2_image_write_pbm(ctx->pages[ctx->current_page].image, stdout);
#endif

    return 0;
}

/**
 * jbig2_get_page: return the next available page image buffer
 *
 * the client can call this at any time to check if any pages
 * have been decoded. If so, it returns the first available
 * one. The client should then call jbig2_release_page() when
 * it no longer needs to refer to the image buffer.
 *
 * since this is a public routine for the library clients, we
 * return an image structure pointer, even though the function
 * name refers to a page; the page structure is private.
 **/
Jbig2Image *jbig2_page_out(Jbig2Ctx *ctx)
{
    int index;

    /* search for a completed page */
    for (index=0; index < ctx->max_page_index; index++) {
        if (ctx->pages[index].state == JBIG2_PAGE_COMPLETE) {
            ctx->pages[index].state = JBIG2_PAGE_RETURNED;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1,
                "page %d returned to the client", ctx->pages[index].number);
            return ctx->pages[index].image;
        }
    }
    
    /* no pages available */
    return NULL;
}

/**
 * jbig2_release_page: tell the library a page can be freed
 **/
int jbig2_release_page(Jbig2Ctx *ctx, Jbig2Image *image)
{
    int index;
    
    /* find the matching page struct and mark it released */
    for (index = 0; index < ctx->max_page_index; index++) {
        if (ctx->pages[index].image == image) {
            /* todo: free associated image */
            ctx->pages[index].state = JBIG2_PAGE_RELEASED;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1,
                "page %d released by the client", ctx->pages[index].number);
            return 0;
        }
    }
    
    /* no matching pages */
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1,
        "jbig2_release_page called on unknown page");
    return 1;
}