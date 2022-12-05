//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (c) 2014-2020 Cyril Hrubis <metan@ucw.cz>

 */

/*

   Simple markup.

   - variables are enclosed in {0.000}
     the string between {} is used for
     initial size computation

   - newlines are encoded with \n

   - bold is enclosed with asterisks: *bold*

   - escape character is \ as in \* or \{

 */

#ifndef GP_WIDGET_MARKUP_H
#define GP_WIDGET_MARKUP_H

#include <widgets/gp_markup_parser.h>

struct gp_widget_markup {
	char *text;
	char *(*get)(unsigned int var_id, char *old_val);
	struct gp_markup *markup;
};

/**
 * @brief Allocates and initializes a markup widget.
 *
 * @markup A markup string.
 * @get A callback to resolve variables.
 *
 * @return A markup widget.
 */
gp_widget *gp_widget_markup_new(const char *markup, enum gp_markup_fmt fmt,
                                char *(*get)(unsigned int var_id, char *old_val));

/**
 * @brief Sets new markup string.
 *
 * @self A markup widget.
 * @markup_str New markup string.
 *
 * @return Zero on success non-zero on a failure.
 */
int gp_widget_markup_set(gp_widget *self, enum gp_markup_fmt fmt, const char *markup_str);

/**
 * @brief Update a markup variable.
 *
 * This function updates a single markup variable.
 *
 * @self A markup widget.
 * @var_id A variable id.
 * @fmt A printf like formatting string.
 * @... Variable printf arguments.
 */
void gp_widget_markup_set_var(gp_widget *self, unsigned int var_id, const char *fmt, ...)
	                      __attribute__ ((format (printf, 3, 4)));

/**
 * @brief Request variables to be updated.
 *
 * This function loops over all variables and calls the get() callback for each
 * of them.
 *
 * @self A markup widget.
 */
void gp_widget_markup_refresh(gp_widget *self);

#endif /* GP_WIDGET_MARKUP_H */
