/*
    jbig2dec
    
    Copyright (c) 2002 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_image_pbm.c,v 1.10 2002/07/20 17:23:15 giles Exp $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stdio.h>
#include <ctype.h>

#include "jbig2.h"
#include "jbig2_image.h"

/* take an image structure and write it to a file in pbm format */

int jbig2_image_write_pbm_file(Jbig2Image *image, char *filename)
{
    FILE *out;
    int	error;
    
    if ((out = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "unable to open '%s' for writing", filename);
        return 1;
    }
    
    error = jbig2_image_write_pbm(image, out);
    
    fclose(out);
    return (error);
}

/* write out an image struct as a pbm stream to an open file pointer */

int jbig2_image_write_pbm(Jbig2Image *image, FILE *out)
{
        // pbm header
        fprintf(out, "P4\n%d %d\n", image->width, image->height);
        
        // pbm format pads to a byte boundary, so we can
        // just write out the whole data buffer
        // NB: this assumes minimal stride for the width
        fwrite(image->data, 1, image->height*image->stride, out);
        
        /* success */
	return 0;
}

/* take an image from a file in pbm format */
Jbig2Image *jbig2_image_read_pbm_file(Jbig2Ctx *ctx, char *filename)
{
    FILE *in;
    Jbig2Image *image;
    
    if ((in = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "unable to open '%s' for reading\n", filename);
		return NULL;
    }
    
    image = jbig2_image_read_pbm(ctx, in);
    
    return (image);
}

// FIXME: should handle multi-image files
Jbig2Image *jbig2_image_read_pbm(Jbig2Ctx *ctx, FILE *in)
{
    int i, dim[2];
    int done;
    Jbig2Image *image;
    int c;
    char buf[32];
    
    // look for 'P4' magic
    while ((c = fgetc(in)) != 'P') {
        if (feof(in)) return NULL;
    }
    if ((c = fgetc(in)) != '4') {
        fprintf(stderr, "not a binary pbm file.\n");
        return NULL;
    }
    // read size. we must find two decimal numbers representing
    // the image dimensions. done will index whether we're
    // looking for the width of the height and i will be our
    // array index for copying strings into our buffer
    done = 0;
    i = 0;
    while (done < 2) {
        c = fgetc(in);
        // skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        // skip comments
        if (c == '#') {
            while ((c = fgetc(in)) != '\n');
            continue;
        }
        if (isdigit(c)) {
            buf[i++] = c;
            while (isdigit(buf[i++] = fgetc(in))) {
                if (feof(in) || i >= 32) {
                    fprintf(stderr, "pbm parsing error\n");
                    return NULL;
                }
            }
            buf[i] = '\0';
            sscanf(buf, "%d", &dim[done]);
            i = 0;
            done++;
        }
    }
    // allocate image structure
    image = jbig2_image_new(ctx, dim[0], dim[1]);
    if (image == NULL) {
        fprintf(stderr, "could not allocate %dx%d image for pbm file\n", dim[0], dim[1]);
        return NULL;
    }
    // the pbm data is byte-aligned, so we can
    // do a simple block read
    fread(image->data, 1, image->height*image->stride, in);
    if (feof(in)) {
        fprintf(stderr, "unexpected end of pbm file.\n");
        jbig2_image_free(ctx, image);
        return NULL;
    }    
    // success
    return image;
}
