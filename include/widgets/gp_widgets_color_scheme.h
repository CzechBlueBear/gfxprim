//SPDX-License-Identifier: LGPL-2.0-or-later

/*

   Copyright (c) 2014-2022 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef GP_WIDGETS_COLOR_SCHEME_H
#define GP_WIDGETS_COLOR_SCHEME_H

#include <widgets/gp_widget_types.h>

/*
 * Decides if the pixmaps generated by application should have dark or light
 * background. There is also default light and dark scheme compiled in the
 * library.
 */
enum gp_widgets_color_scheme {
	GP_WIDGET_COLOR_SCHEME_DEFAULT,
	GP_WIDGET_COLOR_SCHEME_LIGHT,
	GP_WIDGET_COLOR_SCHEME_DARK,
};

enum gp_widgets_color {
	/* theme colors */
	GP_WIDGETS_COL_TEXT,
	GP_WIDGETS_COL_FG,
	GP_WIDGETS_COL_BG,
	GP_WIDGETS_COL_HIGHLIGHT,
	GP_WIDGETS_COL_SELECT,
	GP_WIDGETS_COL_ALERT,
	GP_WIDGETS_COL_WARN,
	GP_WIDGETS_COL_ACCEPT,
	GP_WIDGETS_COL_FILL,
	GP_WIDGETS_THEME_COLORS,

	/* 16 colors */
	GP_WIDGETS_COL_BLACK = GP_WIDGETS_THEME_COLORS,
	GP_WIDGETS_COL_RED,
	GP_WIDGETS_COL_GREEN,
	GP_WIDGETS_COL_YELLOW,
	GP_WIDGETS_COL_BLUE,
	GP_WIDGETS_COL_MAGENTA,
	GP_WIDGETS_COL_CYAN,
	GP_WIDGETS_COL_GRAY,
	GP_WIDGETS_COL_BR_BLACK,
	GP_WIDGETS_COL_BR_RED,
	GP_WIDGETS_COL_BR_GREEN,
	GP_WIDGETS_COL_BR_YELLOW,
	GP_WIDGETS_COL_BR_BLUE,
	GP_WIDGETS_COL_BR_MAGENTA,
	GP_WIDGETS_COL_BR_CYAN,
	GP_WIDGETS_COL_WHITE,
	GP_WIDGETS_COL_CNT,
};

/**
 * Converts a color name into a color index.
 *
 * Widget theme color names:
 *
 * - "fg"
 * - "bg"
 * - "text"
 * - "highlight"
 * - "alert"
 * - "warn",
 * - "accept",
 *
 * 16 colors names:
 *
 * "black", "red", "green", "yellow", "blue", "magenta", "cyan", "gray",
 * "bright black", "bright red", "bright green", "bright yellow",
 * "bright blue", "bright magenta", "bright cyan", "white"
 *
 * @name a color name
 * @return An enum gp_widgets_color or -1 on a failure.
 */
enum gp_widgets_color gp_widgets_color_name_idx(const char *name);

/**
 * Returns a pixel given an color index.
 *
 * @color A color index.
 * @return A pixel value.
 */
gp_pixel gp_widgets_color(const gp_widget_render_ctx *ctx, enum gp_widgets_color color);

/*
 * Toggles current color scheme, reloads colors, redraws application.
 */
void gp_widgets_color_scheme_toggle(void);

/*
 * Sets current color scheme, reloads colors, redraws application.
 */
void gp_widgets_color_scheme_set(enum gp_widgets_color_scheme color_scheme);

/*
 * Returns current color scheme.
 */
enum gp_widgets_color_scheme gp_widgets_color_scheme_get(void);

/*
 * Creates a new color scheme switch widget.
 */
gp_widget *gp_widget_color_scheme_switch(void);

#endif /* GP_WIDGETS_COLOR_SCHEME_H */
