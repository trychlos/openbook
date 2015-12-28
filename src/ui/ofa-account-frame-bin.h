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

#ifndef __OFA_ACCOUNT_FRAME_BIN_H__
#define __OFA_ACCOUNT_FRAME_BIN_H__

/**
 * SECTION: ofa_account_frame_bin
 * @short_description: #ofaAccountFrameBin class definition.
 * @include: ui/ofa-account-frame-bin.h
 *
 * This is a convenience class which manages both the accounts notebook
 * and the buttons box on the right.
 *
 * The class also acts as a proxy for "changed" and "activated" messages
 * sent by the underlying ofaAccountStore class. It relays these
 * messages as:
 * - 'ofa-changed' when the selection changes
 * - 'ofa-activated' when the selection is activated.
 */

#include "api/ofa-main-window-def.h"

#include "ui/ofa-account-chart-bin.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_FRAME_BIN                ( ofa_account_frame_bin_get_type())
#define OFA_ACCOUNT_FRAME_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBin ))
#define OFA_ACCOUNT_FRAME_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBinClass ))
#define OFA_IS_ACCOUNT_FRAME_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_FRAME_BIN ))
#define OFA_IS_ACCOUNT_FRAME_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_FRAME_BIN ))
#define OFA_ACCOUNT_FRAME_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBinClass ))

typedef struct _ofaAccountFrameBinPrivate         ofaAccountFrameBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                     parent;

	/*< private members >*/
	ofaAccountFrameBinPrivate *priv;
}
	ofaAccountFrameBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                parent;
}
	ofaAccountFrameBinClass;

/* "clicked" signal on frame buttons are proxyed with the following id's
 */
enum {
	ACCOUNT_BUTTON_NEW = 1,
	ACCOUNT_BUTTON_PROPERTIES,
	ACCOUNT_BUTTON_DELETE,
	ACCOUNT_BUTTON_VIEW_ENTRIES,
	ACCOUNT_BUTTON_SETTLEMENT,
	ACCOUNT_BUTTON_RECONCILIATION
};

GType               ofa_account_frame_bin_get_type   ( void ) G_GNUC_CONST;

ofaAccountFrameBin *ofa_account_frame_bin_new        ( const ofaMainWindow *main_window );

void                ofa_account_frame_bin_set_buttons( ofaAccountFrameBin *bin,
																gboolean view_entries,
																gboolean settlement,
																gboolean reconciliation );

ofaAccountChartBin *ofa_account_frame_bin_get_chart  ( const ofaAccountFrameBin *bin );

G_END_DECLS

#endif /* __OFA_ACCOUNT_FRAME_BIN_H__ */
