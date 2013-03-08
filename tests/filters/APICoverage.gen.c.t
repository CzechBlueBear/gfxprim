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
 * Copyright (C) 2009-2013 Cyril Hrubis <metan@ucw.cz>                       *
 *                                                                           *
 *****************************************************************************/

 /*

    The purpose of this test is to exercise as much codepaths as possible
    without checking for result corectness.

  */

%% extends "base.test.c.t"

%% block body

#include <stdio.h>

#include <core/GP_Context.h>
#include <filters/GP_Filters.h>

#include "tst_test.h"

%% set API_List = [['MirrorH', 'GP_Context:in', 'GP_Context:out', 'GP_ProgressCallback'],
                   ['MirrorH_Alloc', 'GP_Context:in', 'GP_ProgressCallback'],

		   ['MirrorV', 'GP_Context:in', 'GP_Context:out', 'GP_ProgressCallback'],
                   ['MirrorV_Alloc', 'GP_Context:in', 'GP_ProgressCallback'],

		   ['Rotate90', 'GP_Context:in', 'GP_Context:out', 'GP_ProgressCallback'],
		   ['Rotate90_Alloc', 'GP_Context:in', 'GP_ProgressCallback'],

		   ['Rotate180', 'GP_Context:in', 'GP_Context:out', 'GP_ProgressCallback'],
		   ['Rotate180_Alloc', 'GP_Context:in', 'GP_ProgressCallback'],

		   ['Rotate270', 'GP_Context:in', 'GP_Context:out', 'GP_ProgressCallback'],
		   ['Rotate270_Alloc', 'GP_Context:in', 'GP_ProgressCallback'],
		   
		   ['GaussianBlur', 'GP_Context:in', 'GP_Context:out',
                    'float:sigma_x', 'float:sigma_y', 'GP_ProgressCallback'],
		   ['GaussianBlurAlloc', 'GP_Context:in', 'float:sigma_x',
                    'float:sigma_y', 'GP_ProgressCallback'],

                   ['GaussianNoiseAdd', 'GP_Context:in', 'GP_Context:out',
                    'float:sigma', 'float:mu', 'GP_ProgressCallback'],
                   ['GaussianNoiseAddAlloc', 'GP_Context:in',
                    'float:sigma', 'float:mu', 'GP_ProgressCallback'],

                   ['Median', 'GP_Context:in', 'GP_Context:out',
                    'int:xmed', 'int:ymed', 'GP_ProgressCallback'],
                   ['MedianAlloc', 'GP_Context:in',
                    'int:xmed', 'int:ymed', 'GP_ProgressCallback'],

		   ['Sigma', 'GP_Context:in', 'GP_Context:out',
                    'int:xrad', 'int:yrad', 'int:min', 'float:sigma', 'GP_ProgressCallback'],
		   ['SigmaAlloc', 'GP_Context:in',
                    'int:xrad', 'int:yrad', 'int:min', 'float:sigma', 'GP_ProgressCallback'],

                   ['ResizeNN', 'GP_Context:in', 'GP_Context:out', 'GP_ProgressCallback'],
                   ['ResizeNNAlloc', 'GP_Context:in', 'int:w', 'int:h', 'GP_ProgressCallback'],
]

%% macro prep_context(id, pt)
	GP_Context *{{ id }} = GP_ContextAlloc(113, 900, GP_PIXEL_{{ pt.name }});
%% endmacro

%% macro prep_float(id)
	float {{ id }} = 1;
%% endmacro

%% macro prep_int(id)
	int {{ id }} = 2;
%% endmacro

%% macro prep_param(param, pt)
%%  if (param.split(':', 1)[0] == 'GP_Context')
{{ prep_context(param.split(':', 1)[1], pt) }}
%%  endif
%%  if (param.split(':', 1)[0] == 'float')
{{ prep_float(param.split(':', 1)[1]) }}
%%  endif
%%  if (param.split(':', 1)[0] == 'int')
{{ prep_int(param.split(':', 1)[1]) }}
%%  endif
%% endmacro

{% macro get_param(param) %}{% if len(param.split(':', 1)) == 1 %}NULL{% else %}{{ param.split(':', 1)[1] }}{% endif %}{% endmacro %}

%% for pt in pixeltypes
%% if not pt.is_unknown()
%% for fn in API_List

static int Filter_{{ fn[0]}}_{{ pt.name }}(void)
{
%% for param in fn[1:]
{{ prep_param(param, pt) }}
%% endfor

	GP_Filter{{ fn[0] }}({{ get_param(fn[1]) }}{% for param in fn[2:] %}, {{ get_param(param) }}{% endfor %});

	return TST_SUCCESS;
}

%% endfor
%% endif
%% endfor

const struct tst_suite tst_suite = {
	.suite_name = "Filters API Coverage",
	.tests = {
%% for fn in API_List
%%  for pt in pixeltypes
%%   if not pt.is_unknown()
		{.name = "Filter {{ fn[0] }} {{ pt.name }}", 
		 .tst_fn = Filter_{{ fn[0] }}_{{ pt.name }}},
%%   endif
%%  endfor
%% endfor
		{.name = NULL}
	}
};

%% endblock body
