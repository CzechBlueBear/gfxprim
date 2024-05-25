//SPDX-License-Identifier: LGPL-2.0-or-later

/*

   Copyright (c) 2014-2024 Cyril Hrubis <metan@ucw.cz>

 */

/**
 * @file gp_widgets_color_scheme.h
 * @brief Color scheme.
 */
#ifndef GP_WIDGETS_COLOR_SCHEME_H
#define GP_WIDGETS_COLOR_SCHEME_H

#include <widgets/gp_widget_types.h>

/**
 * @brief A color scheme.
 *
 * Decides if the pixmaps generated by application should have dark or light
 * background. There is also default light and dark scheme compiled in the
 * library.
 */
enum gp_widgets_color_scheme {
	/** @brief Default color scheme. */
	GP_WIDGET_COLOR_SCHEME_DEFAULT,
	/** @brief Light color scheme. */
	GP_WIDGET_COLOR_SCHEME_LIGHT,
	/** @brief Dark color scheme. */
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
	GP_WIDGETS_COL_DISABLED,
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
 * @brief Converts a color name into a color index.
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
 * @param name a color name
 *
 * @return An enum gp_widgets_color or -1 on a failure.
 */
enum gp_widgets_color gp_widgets_color_name_idx(const char *name);

/**
 * @brief Returns a pixel given an color index.
 *
 * @param ctx A widget rendering context.
 * @param color A color index.
 *
 * @return A pixel value.
 */
gp_pixel gp_widgets_color(const gp_widget_render_ctx *ctx, enum gp_widgets_color color);

/**
 * @brief Toggles current color scheme.
 *
 * Reloads colors, redraws application.
 */
void gp_widgets_color_scheme_toggle(void);

/**
 * @brief Sets a color scheme.
 *
 * Reloads colors, redraws application.
 *
 * @param color_scheme New color scheme.
 */
void gp_widgets_color_scheme_set(enum gp_widgets_color_scheme color_scheme);

/**
 * @brief Returns current color scheme.
 *
 * @return Current color scheme.
 */
enum gp_widgets_color_scheme gp_widgets_color_scheme_get(void);

/**
 * @brief Creates a color scheme switch widget.
 *
 * @return TODO!
 */
gp_widget *gp_widget_color_scheme_switch(void);

#endif /* GP_WIDGETS_COLOR_SCHEME_H */
