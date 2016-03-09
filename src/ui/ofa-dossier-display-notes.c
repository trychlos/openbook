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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-dossier-display-notes.h"

/* private instance data
 */
struct _ofaDossierDisplayNotesPrivate {
	gboolean       dispose_has_run;

	/* UI
	 */

	/* runtime data
	 */
	const gchar   *main_notes;
	const gchar   *exe_notes;

	/* result
	 */
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-display-notes.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void set_notes( ofaDossierDisplayNotes *self, const gchar *label_name, const gchar *note_name, const gchar *notes );

G_DEFINE_TYPE_EXTENDED( ofaDossierDisplayNotes, ofa_dossier_display_notes, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDossierDisplayNotes )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

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
	ofaDossierDisplayNotesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_DISPLAY_NOTES( instance ));

	priv = ofa_dossier_display_notes_get_instance_private( OFA_DOSSIER_DISPLAY_NOTES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_display_notes_parent_class )->dispose( instance );
}

static void
ofa_dossier_display_notes_init( ofaDossierDisplayNotes *self )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_init";
	ofaDossierDisplayNotesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DISPLAY_NOTES( self ));

	priv = ofa_dossier_display_notes_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dossier_display_notes_class_init( ofaDossierDisplayNotesClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_display_notes_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_display_notes_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
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
	ofaDossierDisplayNotesPrivate *priv;

	g_debug( "%s: main_window=%p", thisfn, main_window );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	self = g_object_new( OFA_TYPE_DOSSIER_DISPLAY_NOTES, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_dossier_display_notes_get_instance_private( self );

	priv->main_notes = main_notes;
	priv->exe_notes = exe_notes;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_idialog_init";
	ofaDossierDisplayNotesPrivate *priv;
	GtkApplicationWindow *main_window;
	ofaHub *hub;
	ofoDossier *dossier;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_display_notes_get_instance_private( OFA_DOSSIER_DISPLAY_NOTES( instance ));

	set_notes( OFA_DOSSIER_DISPLAY_NOTES( instance ), "main-label", "main-text", priv->main_notes );
	set_notes( OFA_DOSSIER_DISPLAY_NOTES( instance ), "exe-label", "exe-text", priv->exe_notes );

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	dossier = ofa_hub_get_dossier( hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	my_utils_container_set_editable( GTK_CONTAINER( instance ), ofo_dossier_is_current( dossier ));
}

static void
set_notes( ofaDossierDisplayNotes *self, const gchar *label_name, const gchar *w_name, const gchar *notes )
{
	GtkWidget *textview, *label;
	GtkTextBuffer *buffer;
	gchar *str;

	textview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), w_name );
	g_return_if_fail( textview && GTK_IS_TEXT_VIEW( textview ));

	str = g_strdup( notes ? notes : "" );
	buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( buffer, str, -1 );
	gtk_text_view_set_buffer( GTK_TEXT_VIEW( textview ), buffer );

	g_object_unref( buffer );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), label_name );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), textview );
}
