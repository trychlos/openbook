/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 * and the buttons box on the right if needed.
 *
 * The class also acts as a proxy for "changed" and "activated" messages
 * sent by the underlying ofaAccountStore class. It relays these
 * messages as:
 * - 'ofa-changed' when the selection changes
 * - 'ofa-activated' when the selection is activated.
 *
 * At time, #ofaAccountFrameBin composite widget is used by
 * #ofaAccountPage page and by #ofaAccountSelect selection dialog box.
 */

#include <gtk/gtk.h>

#include "api/ofa-buttons-box.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_FRAME_BIN                ( ofa_account_frame_bin_get_type())
#define OFA_ACCOUNT_FRAME_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBin ))
#define OFA_ACCOUNT_FRAME_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBinClass ))
#define OFA_IS_ACCOUNT_FRAME_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_FRAME_BIN ))
#define OFA_IS_ACCOUNT_FRAME_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_FRAME_BIN ))
#define OFA_ACCOUNT_FRAME_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_FRAME_BIN, ofaAccountFrameBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaAccountFrameBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaAccountFrameBinClass;

/**
 * ofeAccountFrameBtn:
 *
 * These are the buttons that the frame is able to manage.
 * It is up to the caller to set the desired buttons.
 * Default is none.
 */
typedef enum {
	ACCOUNT_BTN_SPACER = 1,
	ACCOUNT_BTN_NEW,
	ACCOUNT_BTN_PROPERTIES,
	ACCOUNT_BTN_DELETE,
	ACCOUNT_BTN_VIEW_ENTRIES,
	ACCOUNT_BTN_SETTLEMENT,
	ACCOUNT_BTN_RECONCILIATION,
}
	ofeAccountFrameBtn;

GType               ofa_account_frame_bin_get_type            ( void ) G_GNUC_CONST;

ofaAccountFrameBin *ofa_account_frame_bin_new                 ( ofaIGetter *getter );

GtkWidget          *ofa_account_frame_bin_add_button          ( ofaAccountFrameBin *bin,
																	ofeAccountFrameBtn id,
																	gboolean sensitive );

GtkWidget          *ofa_account_frame_bin_get_current_treeview( const ofaAccountFrameBin *bin );

gchar              *ofa_account_frame_bin_get_selected        ( ofaAccountFrameBin *bin );

void                ofa_account_frame_bin_set_selected        ( ofaAccountFrameBin *bin,
																	const gchar *account_identifier );

void                ofa_account_frame_bin_set_cell_data_func  ( ofaAccountFrameBin *bin,
																	GtkTreeCellDataFunc fn_cell,
																	void *user_data );

void                ofa_account_frame_bin_cell_data_render    ( ofaAccountFrameBin *bin,
																	GtkTreeViewColumn *tcolumn,
																	GtkCellRenderer *cell,
																	GtkTreeModel *tmodel,
																	GtkTreeIter *iter );

G_END_DECLS

#endif /* __OFA_ACCOUNT_FRAME_BIN_H__ */
