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

#ifndef __OFA_ACCOUNT_NOTEBOOK_H__
#define __OFA_ACCOUNT_NOTEBOOK_H__

/**
 * SECTION: ofa_account_notebook
 * @short_description: #ofaAccountNotebook class definition.
 * @include: ui/ofa-account-notebook.h
 *
 * This is a convenience class which manages the display of the accounts
 * inside of a notebook, with one class per page.
 *
 * At creation time, this convenience class defines a Alt+1..Alt+9
 * mnemonic for each class at the GtkWindow parent level. These
 * mnemonics are removed when disposing.
 */

#include <gtk/gtk.h>

#include "api/ofo-account-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_NOTEBOOK                ( ofa_account_notebook_get_type())
#define OFA_ACCOUNT_NOTEBOOK( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_NOTEBOOK, ofaAccountNotebook ))
#define OFA_ACCOUNT_NOTEBOOK_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_NOTEBOOK, ofaAccountNotebookClass ))
#define OFA_IS_ACCOUNT_NOTEBOOK( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_NOTEBOOK ))
#define OFA_IS_ACCOUNT_NOTEBOOK_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_NOTEBOOK ))
#define OFA_ACCOUNT_NOTEBOOK_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_NOTEBOOK, ofaAccountNotebookClass ))

typedef struct _ofaAccountNotebookPrivate        ofaAccountNotebookPrivate;

typedef struct {
	/*< private >*/
	GObject                    parent;
	ofaAccountNotebookPrivate *private;
}
	ofaAccountNotebook;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaAccountNotebookClass;

/**
 * ofaAccountNotebookCb:
 *
 * A callback triggered when the selected account changes.
 *
 * Passed arguments are:
 * - the selected account
 * - provided user data.
 */
typedef void ( *ofaAccountNotebookCb )( ofoAccount *, gpointer );

GType ofa_account_notebook_get_type ( void ) G_GNUC_CONST;

/**
 * ofaAccountNotebookParms:
 * @book: an empty notebook which must have been defined by the caller
 * @dossier: the currently opened ofoDossier
 * @pfnSelect: [allow-none]: a user-provided callback which will be
 *  triggered each time the selection changes
 * @user_data_select:
 * @pfnDoubleClic: [allow-none]: a user-provided callback which will be
 *  triggered each time a new row is activated (by hitting Enter or
 *  double-clicking the row)
 */
typedef struct {
	GtkNotebook         *book;
	ofoDossier          *dossier;
	ofaAccountNotebookCb pfnSelect;
	gpointer             user_data_select;
	ofaAccountNotebookCb pfnDoubleClic;
	gpointer             user_data_double_clic;
}
	ofaAccountNotebookParms;

ofaAccountNotebook *ofa_account_notebook_init_dialog ( ofaAccountNotebookParms *parms );

void                ofa_account_notebook_init_view   ( ofaAccountNotebook *self,
															const gchar *number );

ofoAccount         *ofa_account_notebook_get_selected( ofaAccountNotebook *self );

void                ofa_account_notebook_set_selected( ofaAccountNotebook *self,
															const gchar *number );

void                ofa_account_notebook_grab_focus  ( ofaAccountNotebook *self );

G_END_DECLS

#endif /* __OFA_ACCOUNT_NOTEBOOK_H__ */
