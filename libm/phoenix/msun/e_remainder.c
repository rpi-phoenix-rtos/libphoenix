/*
 * Phoenix-RTOS libphoenix: imported from FreeBSD lib/msun/src/e_remainder.c
 * (freebsd-src commit 77a7a48a1cb003831ff1d4342b1974fc7f79381e). Local changes:
 * - "math_private.h" replaced by the reduced "msun.h"
 * - removed the long double __weak_reference aliases
 * - remainder renamed to __msun_remainder; the public function, which adds C99 errno reporting, is in libm/phoenix/gammaextra.c
 * The original copyright and licence notice follows unchanged.
 */


/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* remainder(x,p)
 * Return :                  
 * 	returns  x REM p  =  x - [x/p]*p as if in infinite 
 * 	precise arithmetic, where [x/p] is the (infinite bit) 
 *	integer nearest x/p (in half way case choose the even one).
 * Method : 
 *	Based on fmod() return x-[x/p]chopped*p exactlp.
 */

#include <float.h>

#include <math.h>
#include "msun.h"

static const double zero = 0.0;


double
__msun_remainder(double x, double p)
{
	int32_t hx,hp;
	u_int32_t sx,lx,lp;
	double p_half;

	EXTRACT_WORDS(hx,lx,x);
	EXTRACT_WORDS(hp,lp,p);
	sx = hx&0x80000000;
	hp &= 0x7fffffff;
	hx &= 0x7fffffff;

    /* purge off exception values */
	if(((hp|lp)==0)||		 	/* p = 0 */
	  (hx>=0x7ff00000)||			/* x not finite */
	  ((hp>=0x7ff00000)&&			/* p is NaN */
	  (((hp-0x7ff00000)|lp)!=0)))
	    return nan_mix_op(x, p, *)/nan_mix_op(x, p, *);


	if (hp<=0x7fdfffff) x = fmod(x,p+p);	/* now x < 2p */
	if (((hx-hp)|(lx-lp))==0) return zero*x;
	x  = fabs(x);
	p  = fabs(p);
	if (hp<0x00200000) {
	    if(x+x>p) {
		x-=p;
		if(x+x>=p) x -= p;
	    }
	} else {
	    p_half = 0.5*p;
	    if(x>p_half) {
		x-=p;
		if(x>=p_half) x -= p;
	    }
	}
	EXTRACT_WORDS(hx, lx, x);
	if (((hx&0x7fffffff)|lx) == 0) hx = 0;
	SET_HIGH_WORD(x,hx^sx);
	return x;
}

