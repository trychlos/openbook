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
#include "ui/ofa-devise-properties.h"
#include "ui/ofo-dossier.h"

/* private instance data
 */
struct _ofaDevisePropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow *main_window;
	GtkDialog     *dialog;
	ofoDevise     *devise;
	gboolean       updated;

	/* data
	 */
	gchar         *code;
	gchar         *label;
	gchar         *symbol;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-devise-properties.ui";
static const gchar  *st_ui_id        = "DevisePropertiesDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaDevisePropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaDeviseProperties *self, ofaMainWindow *main, ofoDevise *devise );
static gboolean  ok_to_terminate( ofaDeviseProperties *self, gint code );
static void      on_code_changed( GtkEntry *entry, ofaDeviseProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaDeviseProperties *self );
static void      on_symbol_changed( GtkEntry *entry, ofaDeviseProperties *self );
static void      check_for_enable_dlg( ofaDeviseProperties *self );
static gboolean  do_update( ofaDeviseProperties *self );

GType
ofa_devise_properties_get_type( void )
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
	static const gchar *thisfn = "ofa_devise_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaDevisePropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaDeviseProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaDeviseProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaDevisePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_devise_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_devise_properties_instance_init";
	ofaDeviseProperties *self;

	g_return_if_fail( OFA_IS_DEVISE_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_DEVISE_PROPERTIES( instance );

	self->private = g_new0( ofaDevisePropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_devise_properties_instance_dispose";
	ofaDevisePropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_DEVISE_PROPERTIES( instance ));

	priv = ( OFA_DEVISE_PROPERTIES( instance ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		g_free( priv->code );
		g_free( priv->label );
		g_free( priv->symbol );

		gtk_widget_destroy( GTK_WIDGET( priv->dialog ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( instance );
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_devise_properties_instance_finalize";
	ofaDeviseProperties *self;

	g_return_if_fail( OFA_IS_DEVISE_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_DEVISE_PROPERTIES( instance );

	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( instance );
}

/**
 * ofa_devise_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an devise
 */
gboolean
ofa_devise_properties_run( ofaMainWindow *main_window, ofoDevise *devise )
{
	static const gchar *thisfn = "ofa_devise_properties_run";
	ofaDeviseProperties *self;
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, devise=%p",
			thisfn, ( void * ) main_window, ( void * ) devise );

	self = g_object_new( OFA_TYPE_DEVISE_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, devise );

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
do_initialize_dialog( ofaDeviseProperties *self, ofaMainWindow *main, ofoDevise *devise )
{
	static const gchar *thisfn = "ofa_devise_properties_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaDevisePropertiesPrivate *priv;
	gchar *title;
	const gchar *code;
	GtkEntry *entry;

	priv = self->private;
	priv->main_window = main;
	priv->devise = devise;

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

		code = ofo_devise_get_code( devise );
		if( !code ){
			title = g_strdup( _( "Defining a new devise" ));
		} else {
			title = g_strdup_printf( _( "Updating « %s » devise" ), code );
		}
		gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );

		priv->code = g_strdup( ofo_devise_get_code( devise ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-code" ));
		if( priv->code ){
			gtk_entry_set_text( entry, priv->code );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_code_changed ), self );

		priv->label = g_strdup( ofo_devise_get_label( devise ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
		if( priv->label ){
			gtk_entry_set_text( entry, priv->label );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

		priv->symbol = g_strdup( ofo_devise_get_symbol( devise ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-symbol" ));
		if( priv->symbol ){
			gtk_entry_set_text( entry, priv->symbol );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_symbol_changed ), self );

		my_utils_init_notes_ex( devise );

		if( code ){
			my_utils_init_maj_user_stamp_ex( devise );
		}
	}

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaDeviseProperties *self, gint code )
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
on_code_changed( GtkEntry *entry, ofaDeviseProperties *self )
{
	g_free( self->private->code );
	self->private->code = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaDeviseProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_symbol_changed( GtkEntry *entry, ofaDeviseProperties *self )
{
	g_free( self->private->symbol );
	self->private->symbol = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDeviseProperties *self )
{
	ofaDevisePropertiesPrivate *priv;
	GtkWidget *button;
	gboolean ok;
	ofoDevise *exists;

	priv = self->private;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( priv->dialog ), "btn-ok" );

	ok = ofo_devise_is_valid( priv->code, priv->label, priv->symbol );
	if( ok ){
		exists = ofo_devise_get_by_code(
				ofa_main_window_get_dossier( priv->main_window ), priv->code );
		ok &= !exists ||
				( ofo_devise_get_id( exists ) == ofo_devise_get_id( priv->devise ));
	}

	gtk_widget_set_sensitive( button, ok );
}

static gboolean
do_update( ofaDeviseProperties *self )
{
	ofoDossier *dossier;
	ofoDevise *existing;

	dossier = ofa_main_window_get_dossier( self->private->main_window );
	existing = ofo_devise_get_by_code( dossier, self->private->code );
	g_return_val_if_fail(
			!existing ||
			ofo_devise_get_id( existing ) == ofo_devise_get_id( self->private->devise ), NULL );

	ofo_devise_set_code( self->private->devise, self->private->code );
	ofo_devise_set_label( self->private->devise, self->private->label );
	ofo_devise_set_symbol( self->private->devise, self->private->symbol );

	my_utils_getback_notes_ex( devise );

	if( !existing ){
		self->private->updated =
				ofo_devise_insert( self->private->devise, dossier );
	} else {
		self->private->updated =
				ofo_devise_update( self->private->devise, dossier );
	}

	return( self->private->updated );
}
