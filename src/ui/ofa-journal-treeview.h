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

#ifndef __OFA_JOURNAL_TREEVIEW_H__
#define __OFA_JOURNAL_TREEVIEW_H__

/**
 * SECTION: ofa_journal_treeview
 * @short_description: #ofaJournalTreeview class definition.
 * @include: ui/ofa-journal-treeview.h
 *
 * A convenience class to display journals treeview based on a liststore
 * (use cases: ofaJournalsSet main page, ofaIntClosing dialog)
 *
 * In the provided parent container, this class will create a GtkTreeView
 * embedded in a GtkScrolledWindow.
 */

#include "ui/ofa-main-window-def.h"
#include "ui/ofo-dossier-def.h"
#include "ui/ofo-journal-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_JOURNAL_TREEVIEW                ( ofa_journal_treeview_get_type())
#define OFA_JOURNAL_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_JOURNAL_TREEVIEW, ofaJournalTreeview ))
#define OFA_JOURNAL_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_JOURNAL_TREEVIEW, ofaJournalTreeviewClass ))
#define OFA_IS_JOURNAL_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_JOURNAL_TREEVIEW ))
#define OFA_IS_JOURNAL_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_JOURNAL_TREEVIEW ))
#define OFA_JOURNAL_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_JOURNAL_TREEVIEW, ofaJournalTreeviewClass ))

typedef struct _ofaJournalTreeviewPrivate        ofaJournalTreeviewPrivate;

typedef struct {
	/*< private >*/
	GObject                    parent;
	ofaJournalTreeviewPrivate *private;
}
	ofaJournalTreeview;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaJournalTreeviewClass;

/**
 * JournalTreeviewCb:
 *
 * A callback to be triggered when a row is selected or activated.
 *
 * Passed parameters are:
 * - mnemo
 * - user_data provided at initialization time
 */
typedef void ( *JournalTreeviewCb )( const gchar *, gpointer );

/**
 * JournalTreeviewParms:
 *
 * The structure used to initialize this convenience class.
 */
typedef struct {
	ofaMainWindow    *main_window;
	GtkContainer     *parent;
	gboolean          allow_multiple_selection;
	JournalTreeviewCb pfnSelection;
	JournalTreeviewCb pfnActivation;
	void             *user_data;
}
	JournalTreeviewParms;

GType               ofa_journal_treeview_get_type    ( void ) G_GNUC_CONST;

ofaJournalTreeview *ofa_journal_treeview_new         ( const JournalTreeviewParms *parms );

void                ofa_journal_treeview_init_view   ( ofaJournalTreeview *view, const gchar *initial_selection );

ofoJournal         *ofa_journal_treeview_get_selected( ofaJournalTreeview *view );

void                ofa_journal_treeview_grab_focus  ( ofaJournalTreeview *view );

G_END_DECLS

#endif /* __OFA_JOURNAL_TREEVIEW_H__ */
