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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "ui/my-utils.h"
#include "ui/ofa-mod-family-properties.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaModFamilyPropertiesClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaModFamilyPropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow *main_window;
	GtkDialog     *dialog;
	ofoModFamily  *family;
	gboolean       updated;

	/* data
	 */
	gint           id;
	gchar         *label;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-mod-family-properties.ui";
static const gchar  *st_ui_id        = "ModFamilyPropertiesDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaModFamilyPropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaModFamilyProperties *self, ofaMainWindow *main, ofoModFamily *family );
static gboolean  ok_to_terminate( ofaModFamilyProperties *self, gint code );
static void      on_label_changed( GtkEntry *entry, ofaModFamilyProperties *self );
static void      check_for_enable_dlg( ofaModFamilyProperties *self );
static gboolean  do_update( ofaModFamilyProperties *self );

GType
ofa_mod_family_properties_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_mod_family_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaModFamilyPropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaModFamilyProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaModFamilyProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaModFamilyPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_mod_family_properties_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaModFamilyPropertiesClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_mod_family_properties_instance_init";
	ofaModFamilyProperties *self;

	g_return_if_fail( OFA_IS_MOD_FAMILY_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_MOD_FAMILY_PROPERTIES( instance );

	self->private = g_new0( ofaModFamilyPropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_mod_family_properties_instance_dispose";
	ofaModFamilyPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_MOD_FAMILY_PROPERTIES( window ));

	priv = ( OFA_MOD_FAMILY_PROPERTIES( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->label );
		g_free( priv->maj_user );

		gtk_widget_destroy( GTK_WIDGET( priv->dialog ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_mod_family_properties_instance_finalize";
	ofaModFamilyProperties *self;

	g_return_if_fail( OFA_IS_MOD_FAMILY_PROPERTIES( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_MOD_FAMILY_PROPERTIES( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_mod_family_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an model family
 */
gboolean
ofa_mod_family_properties_run( ofaMainWindow *main_window, ofoModFamily *family )
{
	static const gchar *thisfn = "ofa_mod_family_properties_run";
	ofaModFamilyProperties *self;
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, mod_family=%p",
			thisfn, ( void * ) main_window, ( void * ) family );

	self = g_object_new( OFA_TYPE_MOD_FAMILY_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, family );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
do_initialize_dialog( ofaModFamilyProperties *self, ofaMainWindow *main, ofoModFamily *family )
{
	static const gchar *thisfn = "ofa_mod_family_properties_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaModFamilyPropertiesPrivate *priv;
	gchar *title;
	const gchar *fam_label;
	GtkEntry *entry;
	gchar *notes;
	GtkTextView *text;
	GtkTextBuffer *buffer;

	priv = self->private;
	priv->main_window = main;
	priv->family = family;

	/* create the GtkDialog */
	error = NULL;
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
		priv->dialog = GTK_DIALOG( gtk_builder_get_object( builder, st_ui_id ));
		if( !priv->dialog ){
			g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}
	g_object_unref( builder );

	/* initialize the newly created dialog */
	if( priv->dialog ){

		/*gtk_window_set_transient_for( GTK_WINDOW( priv->dialog ), GTK_WINDOW( main ));*/

		fam_label = ofo_mod_family_get_label( family );
		if( !fam_label ){
			title = g_strdup( _( "Defining a new family" ));
		} else {
			title = g_strdup_printf( _( "Updating family %s" ), fam_label );
		}
		gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );

		priv->label = g_strdup( ofo_mod_family_get_label( family ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
		if( priv->label ){
			gtk_entry_set_text( entry, priv->label );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

		notes = g_strdup( ofo_mod_family_get_notes( family ));
		if( notes ){
			text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-notes" ));
			buffer = gtk_text_buffer_new( NULL );
			gtk_text_buffer_set_text( buffer, notes, -1 );
			gtk_text_view_set_buffer( text, buffer );
		}
	}

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaModFamilyProperties *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_NONE:
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_CANCEL:
			quit = TRUE;
			break;

		case GTK_RESPONSE_OK:
			quit = do_update( self );
			break;
	}

	return( quit );
}

static void
on_label_changed( GtkEntry *entry, ofaModFamilyProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaModFamilyProperties *self )
{
	ofaModFamilyPropertiesPrivate *priv;
	GtkWidget *button;

	priv = self->private;
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button, priv->label && g_utf8_strlen( priv->label, -1 ));
}

static gboolean
do_update( ofaModFamilyProperties *self )
{
	ofoDossier *dossier;
	gboolean is_new;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *notes;

	dossier = ofa_main_window_get_dossier( self->private->main_window );
	ofo_mod_family_set_label( self->private->family, self->private->label );
	is_new = ( self->private->id > 0 );

	text = GTK_TEXT_VIEW(
			my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-notes" ));
	buffer = gtk_text_view_get_buffer( text );
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
	ofo_mod_family_set_notes( self->private->family, notes );
	g_free( notes );

	if( is_new ){
		self->private->updated =
				ofo_dossier_insert_mod_family( dossier, self->private->family );
	} else {
		self->private->updated =
				ofo_dossier_update_mod_family( dossier, self->private->family );
	}

	return( self->private->updated );
}
