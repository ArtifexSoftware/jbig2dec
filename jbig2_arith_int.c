/* Annex A */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memset() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_int.h"

struct _Jbig2ArithIntCtx {
  Jbig2ArithCx IAx[512];
};

Jbig2ArithIntCtx *
jbig2_arith_int_ctx_new(Jbig2Ctx *ctx)
{
  Jbig2ArithIntCtx *result = jbig2_new(ctx, Jbig2ArithIntCtx, 1);

  memset(result->IAx, 0, sizeof(result->IAx));

  return result;
}

/* A.2 */
/* Return value: -1 on error, 0 on normal value, 1 on OOB return. */
int
jbig2_arith_int_decode(Jbig2ArithIntCtx *ctx, Jbig2ArithState *as,
		       int32_t *p_result)
{
  Jbig2ArithCx *IAx = ctx->IAx;
  int PREV = 1;
  int S, V;
  int bit;
  int n_tail, offset;
  int i;

  S = jbig2_arith_decode(as, &IAx[PREV]);
  PREV = (PREV << 1) | S;

  bit = jbig2_arith_decode(as, &IAx[PREV]);
  PREV = (PREV << 1) | bit;
  if (bit)
    {
      bit = jbig2_arith_decode(as, &IAx[PREV]);
      PREV = (PREV << 1) | bit;

      if (bit)
	{
	  bit = jbig2_arith_decode(as, &IAx[PREV]);
	  PREV = (PREV << 1) | bit;

	  if (bit)
	    {
	      bit = jbig2_arith_decode(as, &IAx[PREV]);
	      PREV = (PREV << 1) | bit;

	      if (bit)
		{
		  bit = jbig2_arith_decode(as, &IAx[PREV]);
		  PREV = (PREV << 1) | bit;

		  if (bit)
		    {
		      n_tail = 32;
		      offset = 4436;
		    }
		  else
		    {
		      n_tail = 12;
		      offset = 340;
		    }
		}
	      else
		{
		  n_tail = 8;
		  offset = 84;
		}
	    }
	  else
	    {
	      n_tail = 6;
	      offset = 20;
	    }
	}
      else
	{
	  n_tail = 4;
	  offset = 4;
	}
    }
  else
    {
      n_tail = 2;
      offset = 0;
    }

  V = 0;
  for (i = 0; i < n_tail; i++)
    {
      bit = jbig2_arith_decode(as, &IAx[PREV]);
      PREV = ((PREV << 1) & 511) | (PREV & 256) | bit;
      V = (V << 1) | bit;
    }

  V += offset;
  V = S ? -V : V;
  *p_result = V;
  return S && V == 0 ? 1 : 0;
}

void
jbig2_arith_int_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIntCtx *iax)
{
  jbig2_free(ctx->allocator, iax);
}
