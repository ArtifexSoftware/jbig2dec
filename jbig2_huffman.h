/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    $Id: jbig2_huffman.h,v 1.4 2002/06/15 16:02:53 giles Exp $
*/

#include "jbig2_hufftab.h"

Jbig2HuffmanState *
jbig2_huffman_new (Jbig2WordStream *ws);

int32_t
jbig2_huffman_get (Jbig2HuffmanState *hs,
		   const Jbig2HuffmanTable *table, bool *oob);

Jbig2HuffmanTable *
jbig2_build_huffman_table (const Jbig2HuffmanParams *params);
