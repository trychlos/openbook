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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-dossier-display-notes.h"

/* private instance data
 */
struct _ofaDossierDisplayNotesPrivate {

	/* UI
	 */

	/* runtime data
	 */
	ofaMainWindow *main_window;
	const gchar   *main_notes;
	const gchar   *exe_notes;

	/* result
	 */
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-display-notes.ui";
static const gchar *st_ui_id            = "DossierDisplayNotesDlg";

G_DEFINE_TYPE( ofaDossierDisplayNotes, ofa_dossier_display_notes, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      set_notes( ofaDossierDisplayNotes *self, GtkWindow *toplevel, const gchar *label_name, const gchar *note_name, const gchar *notes );

static void
dossier_display_notes_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_DISPLAY_NOTES( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_display_notes_parent_class )->finalize( instance );
}

static void
dossier_display_notes_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_DISPLAY_NOTES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_display_notes_parent_class )->dispose( instance );
}

static void
ofa_dossier_display_notes_init( ofaDossierDisplayNotes *self )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DISPLAY_NOTES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_DISPLAY_NOTES, ofaDossierDisplayNotesPrivate );
}

static void
ofa_dossier_display_notes_class_init( ofaDossierDisplayNotesClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_display_notes_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_display_notes_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;

	g_type_class_add_private( klass, sizeof( ofaDossierDisplayNotesPrivate ));
}

/**
 * ofa_dossier_display_notes_run:
 * @main: the main window of the application.
 */
void
ofa_dossier_display_notes_run( ofaMainWindow *main_window, const gchar *main_notes, const gchar *exe_notes )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_run";
	ofaDossierDisplayNotes *self;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_DISPLAY_NOTES,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->priv->main_window = main_window;
	self->priv->main_notes = main_notes;
	self->priv->exe_notes = exe_notes;

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierDisplayNotesPrivate *priv;
	GtkWindow *toplevel;

	priv = OFA_DOSSIER_DISPLAY_NOTES( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	set_notes( OFA_DOSSIER_DISPLAY_NOTES( dialog ), toplevel, "main-label", "main-text", priv->main_notes );
	set_notes( OFA_DOSSIER_DISPLAY_NOTES( dialog ), toplevel, "exe-label", "exe-text", priv->exe_notes );

	my_utils_container_set_editable(
			GTK_CONTAINER( toplevel ),
			ofo_dossier_is_current( ofa_main_window_get_dossier( priv->main_window )));
}

static void
set_notes( ofaDossierDisplayNotes *self, GtkWindow *toplevel, const gchar *label_name, const gchar *w_name, const gchar *notes )
{
	GtkWidget *textview, *label;
	GtkTextBuffer *buffer;
	gchar *str;

	textview = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), w_name );
	g_return_if_fail( textview && GTK_IS_TEXT_VIEW( textview ));

	str = g_strdup( notes ? notes : "" );
	buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( buffer, str, -1 );
	gtk_text_view_set_buffer( GTK_TEXT_VIEW( textview ), buffer );

	g_object_unref( buffer );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), textview );
}
