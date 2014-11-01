/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 *
 * $Id$
 */

#ifndef __OFA_PRINT_H__
#define __OFA_PRINT_H__

/**
 * SECTION: ofa_print
 * @short_description: ofa_print utilities
 * @include: ui/ofa-print.h
 *
 * Provides somes printing utilities in order to homogeneize the
 * summaries.
 */

#include <gtk/gtk.h>

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

enum {
	OFA_PRINT_LEFT = 1,
	OFA_PRINT_RIGHT
};

void     ofa_print_header_dossier_render     ( GtkPrintContext *context, PangoLayout *layout,
													gint page_num, gboolean is_last,
													gdouble y, const ofoDossier *dossier );

gdouble  ofa_print_header_dossier_get_height ( gint page_num, gboolean is_last );

void     ofa_print_header_title_render       ( GtkPrintContext *context, PangoLayout *layout,
													gint page_num, gboolean is_last,
													gdouble y, const gchar *title );

gdouble  ofa_print_header_title_get_height   ( gint page_num, gboolean is_last );

void     ofa_print_header_subtitle_render    ( GtkPrintContext *context, PangoLayout *layout,
													gint page_num, gboolean is_last,
													gdouble y, const gchar *title );

gdouble  ofa_print_header_subtitle_get_height( gint page_num, gboolean is_last );

void     ofa_print_header_title_set_color    ( GtkPrintContext *context, PangoLayout *layout );

void     ofa_print_footer_render             ( GtkPrintContext *context, PangoLayout *layout,
													gint page_num, gboolean is_last,
													gint pages_count );

gdouble  ofa_print_footer_get_height         ( gint page_num, gboolean is_last );

void     ofa_print_rubber                    ( GtkPrintContext *context, PangoLayout *layout,
													gdouble top, gdouble height );

void     ofa_print_set_color                 ( GtkPrintContext *context, PangoLayout *layout,
													double red, double green, double blue );

void     ofa_print_set_font                  ( GtkPrintContext *context, PangoLayout *layout,
													const gchar *font_desc );

void     ofa_print_set_text                  ( GtkPrintContext *context, PangoLayout *layout,
													gdouble x, gdouble y, const gchar *text,
													PangoAlignment align );

G_END_DECLS

#endif /* __OFA_PRINT_H__ */
