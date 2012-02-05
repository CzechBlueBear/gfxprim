/*****************************************************************************
 * This file is part of gfxprim library.                                     *
 *                                                                           *
 * Gfxprim is free software; you can redistribute it and/or                  *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 2.1 of the License, or (at your option) any later version.        *
 *                                                                           *
 * Gfxprim is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with gfxprim; if not, write to the Free Software            *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,                        *
 * Boston, MA  02110-1301  USA                                               *
 *                                                                           *
 * Copyright (C) 2009-2011 Jiri "BlueBear" Dluhos                            *
 *                         <jiri.bluebear.dluhos@gmail.com>                  *
 *                                                                           *
 * Copyright (C) 2009-2012 Cyril Hrubis <metan@ucw.cz>                       *
 *                                                                           *
 *****************************************************************************/

#include "core/GP_GetPutPixel.h"
#include "core/GP_FnPerBpp.h"

#include "gfx/GP_Ellipse.h"
#include "gfx/GP_HLine.h"

#include "algo/Ellipse.algo.h"

/* Generate drawing functions for various bit depths. */
GP_DEF_DRAW_FN_PER_BPP(GP_Ellipse_Raw, DEF_ELLIPSE_FN)

void GP_Ellipse_Raw(GP_Context *context, GP_Coord xcenter, GP_Coord ycenter,
                    GP_Size a, GP_Size b, GP_Pixel pixel)
{
	GP_CHECK_CONTEXT(context);

	GP_FN_PER_BPP_CONTEXT(GP_Ellipse_Raw, context, context,
	                      xcenter, ycenter, a, b, pixel);
}

void GP_Ellipse(GP_Context *context, GP_Coord xcenter, GP_Coord ycenter,
                GP_Size a, GP_Size b, GP_Pixel pixel)
{
	GP_CHECK_CONTEXT(context);
	
	/* recalculate center point and swap a and b when axes are swapped */
	GP_TRANSFORM_POINT(context, xcenter, ycenter);
	GP_TRANSFORM_SWAP(context, a, b);
	
	GP_Ellipse_Raw(context, xcenter, ycenter, a, b, pixel);
}

#include "algo/FillEllipse.algo.h"

/* Generate drawing functions for various bit depths. */
GP_DEF_FILL_FN_PER_BPP(GP_FillEllipse_Raw, DEF_FILLELLIPSE_FN)

void GP_FillEllipse_Raw(GP_Context *context, GP_Coord xcenter, GP_Coord ycenter,
	                GP_Size a, GP_Size b, GP_Pixel pixel)
{
	GP_CHECK_CONTEXT(context);

	GP_FN_PER_BPP_CONTEXT(GP_FillEllipse_Raw, context, context,
	                      xcenter, ycenter, a, b, pixel);
}

void GP_FillEllipse(GP_Context *context, GP_Coord xcenter, GP_Coord ycenter,
                    GP_Size a, GP_Size b, GP_Pixel pixel)
{
	GP_CHECK_CONTEXT(context);
	
	GP_TRANSFORM_POINT(context, xcenter, ycenter);
	GP_TRANSFORM_SWAP(context, a, b);
	
	GP_FillEllipse_Raw(context, xcenter, ycenter, a, b, pixel);
}
