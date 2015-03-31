/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OFA_DATE_FILTER_BIN_H__
#define __OFA_DATE_FILTER_BIN_H__

/**
 * SECTION: ofa_date_filter_bin
 * @short_description: #ofaDateFilterBin class definition.
 * @include: ui/ofa-date-filter-bin.h
 *
 * Display a frame with a starting and an ending date to be used as
 * filters in a treeview.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DATE_FILTER_BIN                ( ofa_date_filter_bin_get_type())
#define OFA_DATE_FILTER_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DATE_FILTER_BIN, ofaDateFilterBin ))
#define OFA_DATE_FILTER_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DATE_FILTER_BIN, ofaDateFilterBinClass ))
#define OFA_IS_DATE_FILTER_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DATE_FILTER_BIN ))
#define OFA_IS_DATE_FILTER_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DATE_FILTER_BIN ))
#define OFA_DATE_FILTER_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DATE_FILTER_BIN, ofaDateFilterBinClass ))

typedef struct _ofaDateFilterBinPrivate         ofaDateFilterBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                   parent;

	/*< private members >*/
	ofaDateFilterBinPrivate *priv;
}
	ofaDateFilterBin;

typedef struct {
	/*< public members >*/
	GtkBinClass              parent;
}
	ofaDateFilterBinClass;

enum {
	OFA_DATE_FILTER_FROM = 1,
	OFA_DATE_FILTER_TO
};

GType             ofa_date_filter_bin_get_type       ( void ) G_GNUC_CONST;

ofaDateFilterBin *ofa_date_filter_bin_new            ( const gchar *pref_name );

gboolean          ofa_date_filter_bin_is_from_empty  ( const ofaDateFilterBin *bin );

gboolean          ofa_date_filter_bin_is_from_valid  ( const ofaDateFilterBin *bin );

const GDate      *ofa_date_filter_bin_get_from       ( const ofaDateFilterBin *bin );

void              ofa_date_filter_bin_set_from       ( ofaDateFilterBin *bin,
															const GDate *from );

gboolean          ofa_date_filter_bin_is_to_empty    ( const ofaDateFilterBin *bin );

gboolean          ofa_date_filter_bin_is_to_valid    ( const ofaDateFilterBin *bin );

const GDate      *ofa_date_filter_bin_get_to         ( const ofaDateFilterBin *bin );

void              ofa_date_filter_bin_set_to         ( ofaDateFilterBin *bin,
															const GDate *to );

GtkWidget        *ofa_date_filter_bin_get_frame_label( const ofaDateFilterBin *bin );

GtkWidget        *ofa_date_filter_bin_get_from_prompt( const ofaDateFilterBin *bin );

G_END_DECLS

#endif /* __OFA_DATE_FILTER_BIN_H__ */
