//SPDX-License-Identifier: LGPL-2.0-or-later

/*

   Copyright (c) 2014-2024 Cyril Hrubis <metan@ucw.cz>

 */

/**
 * @file gp_widget_radiobutton.h
 * @brief A radio button widget.
 *
 * Radio button is a choice class widget, after it's created it's changed by the
 * class functions such as gp_widget_choice_sel_set().
 */

#ifndef GP_WIDGET_RADIOBUTTON_H
#define GP_WIDGET_RADIOBUTTON_H

#include <widgets/gp_widget_class_choice.h>

/**
 * @brief Allocates and initializes new radiobutton widget.
 *
 * @param choices An array of strings describing available choices.
 * @param cnt Size of the choices array.
 * @param sel Initially selected choice.
 *
 * @return A radiobutton widget.
 */
static inline gp_widget *gp_widget_radiobutton_new(const char *choices[],
                                                   size_t cnt, size_t sel)
{
	return gp_widget_choice_new(GP_WIDGET_RADIOBUTTON, choices, cnt, sel);
}

/**
 * @brief Creates a radiobutton widget based on widget ops.
 *
 * @param ops A pointer to the widget ops.
 *
 * @return A radio button widget.
 */
static inline gp_widget *
gp_widget_radiobutton_ops_new(const struct gp_widget_choice_ops *ops)
{
	return gp_widget_choice_ops_new(GP_WIDGET_RADIOBUTTON, ops);
}

/**
 * @brief Creates a radiobutton widget based on a static array.
 *
 * @param array A pointer an array.
 * @param memb_cnt An array size, i.e. number of elements.
 * @param memb_size An array member size, e.g. sizeof(struct foo)
 * @param memb_off An offset of the string for the choice in the array, e.g.
 *                 offsetof(struct foo, str_elem)
 * @param sel A selected choice.
 * @param flags A bitwise combination of flags.
 *
 * @return A radiobutton widget.
 */
static inline gp_widget *
gp_widget_radiobutton_arr_new(const void *array,
                             size_t memb_cnt, uint16_t memb_size,
                             uint16_t memb_off, size_t sel,
			     enum gp_widget_choice_flags flags)
{
	return gp_widget_choice_arr_new(GP_WIDGET_RADIOBUTTON, array,
                                        memb_cnt, memb_size, memb_off,
	                                sel, flags);
}

#endif /* GP_WIDGET_RADIOBUTTON_H */
