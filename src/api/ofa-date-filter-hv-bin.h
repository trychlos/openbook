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

#ifndef __OPENBOOK_API_OFA_DATE_FILTER_HV_BIN_H__
#define __OPENBOOK_API_OFA_DATE_FILTER_HV_BIN_H__

/**
 * SECTION: ofa_date_filter_hv_bin
 * @short_description: #ofaDateFilterHVBin class definition.
 * @include: openbook/ofa-date-filter-hv-bin.h
 *
 * Display a frame with a starting and an ending date to be used as
 * filters in a treeview.
 *
 * Each date entry comes with a check label which displays the entered
 * date in another format, for visual check.
 * This label may come either besides the entry, or below it.
 *
 * The two entries may also come either besides each one (horizontally
 * aligned), or one below the other (vertically aligned).
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no  (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DATE_FILTER_HV_BIN                ( ofa_date_filter_hv_bin_get_type())
#define OFA_DATE_FILTER_HV_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DATE_FILTER_HV_BIN, ofaDateFilterHVBin ))
#define OFA_DATE_FILTER_HV_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DATE_FILTER_HV_BIN, ofaDateFilterHVBinClass ))
#define OFA_IS_DATE_FILTER_HV_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DATE_FILTER_HV_BIN ))
#define OFA_IS_DATE_FILTER_HV_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DATE_FILTER_HV_BIN ))
#define OFA_DATE_FILTER_HV_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DATE_FILTER_HV_BIN, ofaDateFilterHVBinClass ))

typedef struct _ofaDateFilterHVBinPrivate          ofaDateFilterHVBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                     parent;

	/*< private members >*/
	ofaDateFilterHVBinPrivate *priv;
}
	ofaDateFilterHVBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                parent;
}
	ofaDateFilterHVBinClass;

GType               ofa_date_filter_hv_bin_get_type( void ) G_GNUC_CONST;

ofaDateFilterHVBin *ofa_date_filter_hv_bin_new     ( void );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_DATE_FILTER_HV_BIN_H__ */
