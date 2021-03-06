/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-display-notes.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;
	GtkWindow   *parent;
	const gchar *main_notes;
	const gchar *exe_notes;

	/* runtime
	 */
	GtkWindow   *actual_parent;

	/* result
	 */
}
	ofaDossierDisplayNotesPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-display-notes.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
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
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 */
void
ofa_dossier_display_notes_run( ofaIGetter *getter, GtkWindow *parent, const gchar *main_notes, const gchar *exe_notes )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_run";
	ofaDossierDisplayNotes *self;
	ofaDossierDisplayNotesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_DOSSIER_DISPLAY_NOTES, NULL );

	priv = ofa_dossier_display_notes_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->main_notes = main_notes;
	priv->exe_notes = exe_notes;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_dossier_display_notes_iwindow_init";
	ofaDossierDisplayNotesPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_display_notes_get_instance_private( OFA_DOSSIER_DISPLAY_NOTES( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
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
	ofaHub *hub;
	gboolean is_writable;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dossier_display_notes_get_instance_private( OFA_DOSSIER_DISPLAY_NOTES( instance ));

	set_notes( OFA_DOSSIER_DISPLAY_NOTES( instance ), "main-label", "main-text", priv->main_notes );
	set_notes( OFA_DOSSIER_DISPLAY_NOTES( instance ), "exe-label", "exe-text", priv->exe_notes );

	hub = ofa_igetter_get_hub( priv->getter );
	is_writable = ofa_hub_is_writable_dossier( hub );

	my_utils_container_set_editable( GTK_CONTAINER( instance ), is_writable );
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
