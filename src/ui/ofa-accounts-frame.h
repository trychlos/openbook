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
 *
 * $Id$
 */

#ifndef __OFA_ACCOUNTS_FRAME_H__
#define __OFA_ACCOUNTS_FRAME_H__

/**
 * SECTION: ofa_accounts_frame
 * @short_description: #ofaAccountsFrame class definition.
 * @include: ui/ofa-accounts-frame.h
 *
 * This is a convenience class which manages both the accounts notebook
 * and the buttons box on the right.
 *
 * The class also acts as a proxy for "changed" and "activated" messages
 * sent by the underlying ofaAccountIStore interface.
 */

#include <gtk/gtk.h>

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNTS_FRAME                ( ofa_accounts_frame_get_type())
#define OFA_ACCOUNTS_FRAME( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNTS_FRAME, ofaAccountsFrame ))
#define OFA_ACCOUNTS_FRAME_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNTS_FRAME, ofaAccountsFrameClass ))
#define OFA_IS_ACCOUNTS_FRAME( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNTS_FRAME ))
#define OFA_IS_ACCOUNTS_FRAME_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNTS_FRAME ))
#define OFA_ACCOUNTS_FRAME_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNTS_FRAME, ofaAccountsFrameClass ))

typedef struct _ofaAccountsFramePrivate        ofaAccountsFramePrivate;

typedef struct {
	/*< public members >*/
	GtkBin                   parent;

	/*< private members >*/
	ofaAccountsFramePrivate *priv;
}
	ofaAccountsFrame;

typedef struct {
	/*< public members >*/
	GtkBinClass              parent;
}
	ofaAccountsFrameClass;

GType             ofa_accounts_frame_get_type                ( void ) G_GNUC_CONST;

ofaAccountsFrame *ofa_accounts_frame_new                     ( void );

void              ofa_accounts_frame_set_main_window         ( ofaAccountsFrame *frame,
																		ofaMainWindow *main_window );

void              ofa_accounts_frame_set_buttons             ( ofaAccountsFrame *frame,
																		gboolean view_entries );

gchar            *ofa_accounts_frame_get_selected            ( ofaAccountsFrame *frame );

void              ofa_accounts_frame_set_selected            ( ofaAccountsFrame *frame,
																		const gchar *number );

void              ofa_accounts_frame_toggle_collapse         ( ofaAccountsFrame *frame );

GtkWidget        *ofa_accounts_frame_get_top_focusable_widget( const ofaAccountsFrame *frame );

G_END_DECLS

#endif /* __OFA_ACCOUNTS_FRAME_H__ */
