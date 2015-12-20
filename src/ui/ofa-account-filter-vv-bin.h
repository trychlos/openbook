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

#ifndef __OFA_ACCOUNT_FILTER_VV_BIN_H__
#define __OFA_ACCOUNT_FILTER_VV_BIN_H__

/**
 * SECTION: ofa_account_filter_vv_bin
 * @short_description: #ofaAccountFilterVVBin class definition.
 * @include: ui/ofa-account-filter-vv-bin.h
 *
 * Display a frame with a starting and an ending accounts to be used
 * as filters.
 * Each entry comes with a control label which displays the label of
 * the entered account. This label may come either besides the entry,
 * or below it.
 * The two entries may also come either besides each one (horizontally
 * aligned), or one below the other (vertically aligned).
 *
 * This composite widget implements the ofaIAccountFilter interface.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no  (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include "api/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_FILTER_VV_BIN                ( ofa_account_filter_vv_bin_get_type())
#define OFA_ACCOUNT_FILTER_VV_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_FILTER_VV_BIN, ofaAccountFilterVVBin ))
#define OFA_ACCOUNT_FILTER_VV_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_FILTER_VV_BIN, ofaAccountFilterVVBinClass ))
#define OFA_IS_ACCOUNT_FILTER_VV_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_FILTER_VV_BIN ))
#define OFA_IS_ACCOUNT_FILTER_VV_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_FILTER_VV_BIN ))
#define OFA_ACCOUNT_FILTER_VV_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_FILTER_VV_BIN, ofaAccountFilterVVBinClass ))

typedef struct _ofaAccountFilterVVBinPrivate          ofaAccountFilterVVBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                        parent;

	/*< private members >*/
	ofaAccountFilterVVBinPrivate *priv;
}
	ofaAccountFilterVVBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                   parent;
}
	ofaAccountFilterVVBinClass;

GType                  ofa_account_filter_vv_bin_get_type( void ) G_GNUC_CONST;

ofaAccountFilterVVBin *ofa_account_filter_vv_bin_new     ( const ofaMainWindow *main_window );

G_END_DECLS

#endif /* __OFA_ACCOUNT_FILTER_VV_BIN_H__ */
