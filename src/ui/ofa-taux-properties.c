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
#include <stdarg.h>

#include "ui/my-utils.h"
#include "ui/ofa-taux-properties.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaTauxPropertiesClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaTauxPropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow *main_window;
	GtkDialog     *dialog;
	ofoTaux       *taux;
	gboolean       updated;

	/* data
	 */
	gint           id;
	gchar         *mnemo;
	gchar         *label;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-taux-properties.ui";
static const gchar  *st_ui_id        = "TauxPropertiesDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaTauxPropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaTauxProperties *self, ofaMainWindow *main, ofoTaux *taux );
static gboolean  ok_to_terminate( ofaTauxProperties *self, gint code );
static void      on_mnemo_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaTauxProperties *self );
/*static void      on_begin_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      on_end_changed( GtkEntry *entry, ofaTauxProperties *self );
static void      on_taux_changed( GtkEntry *entry, ofaTauxProperties *self );*/
static void      check_for_enable_dlg( ofaTauxProperties *self );
static gboolean  do_update( ofaTauxProperties *self );
static void      error_duplicate( ofaTauxProperties *self, ofoTaux *existing );

GType
ofa_taux_properties_get_type( void )
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
	static const gchar *thisfn = "ofa_taux_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaTauxPropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaTauxProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaTauxProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaTauxPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_taux_properties_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaTauxPropertiesClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_taux_properties_instance_init";
	ofaTauxProperties *self;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_TAUX_PROPERTIES( instance );

	self->private = g_new0( ofaTauxPropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;

	self->private->id = -1;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_taux_properties_instance_dispose";
	ofaTauxPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( window ));

	priv = ( OFA_TAUX_PROPERTIES( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
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
	static const gchar *thisfn = "ofa_taux_properties_instance_finalize";
	ofaTauxProperties *self;

	g_return_if_fail( OFA_IS_TAUX_PROPERTIES( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_TAUX_PROPERTIES( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_taux_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an taux
 */
gboolean
ofa_taux_properties_run( ofaMainWindow *main_window, ofoTaux *taux )
{
	static const gchar *thisfn = "ofa_taux_properties_run";
	ofaTauxProperties *self;
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, taux=%p",
			thisfn, ( void * ) main_window, ( void * ) taux );

	self = g_object_new( OFA_TYPE_TAUX_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, taux );

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
do_initialize_dialog( ofaTauxProperties *self, ofaMainWindow *main, ofoTaux *taux )
{
	static const gchar *thisfn = "ofa_taux_properties_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaTauxPropertiesPrivate *priv;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	gchar *stamp, *str;
	gchar *notes;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkLabel *label;

	priv = self->private;
	priv->main_window = main;
	priv->taux = taux;

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

		mnemo = ofo_taux_get_mnemo( taux );
		if( !mnemo ){
			title = g_strdup( _( "Defining a new rate" ));
		} else {
			title = g_strdup_printf( _( "Updating « %s » rate" ), mnemo );
			self->private->id = ofo_taux_get_id( taux );
		}
		gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );

		priv->mnemo = g_strdup( mnemo );
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-mnemo" ));
		if( priv->mnemo ){
			gtk_entry_set_text( entry, priv->mnemo );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );

		priv->label = g_strdup( ofo_taux_get_label( taux ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
		if( priv->label ){
			gtk_entry_set_text( entry, priv->label );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

		/*memcpy( &priv->begin, ofo_taux_get_val_begin( taux ), sizeof( GDate ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-begin" ));
		str = my_utils_display_from_date( &priv->begin, MY_UTILS_DATE_DMMM );
		gtk_entry_set_text( entry, str );
		g_free( str );
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_begin_changed ), self );

		memcpy( &priv->end, ofo_taux_get_val_end( taux ), sizeof( GDate ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-end" ));
		str = my_utils_display_from_date( &priv->end, MY_UTILS_DATE_DMMM );
		gtk_entry_set_text( entry, str );
		g_free( str );
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_end_changed ), self );

		priv->value = ofo_taux_get_taux( taux );
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-taux" ));
		str = g_strdup_printf( "%.3lf", priv->value );
		gtk_entry_set_text( entry, str );
		g_free( str );
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_taux_changed ), self );*/

		notes = g_strdup( ofo_taux_get_notes( taux ));
		if( notes ){
			text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-notes" ));
			buffer = gtk_text_buffer_new( NULL );
			gtk_text_buffer_set_text( buffer, notes, -1 );
			gtk_text_view_set_buffer( text, buffer );
		}

		if( mnemo ){
			label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "px-last-update" ));
			stamp = my_utils_str_from_stamp( ofo_taux_get_maj_stamp( priv->taux ));
			str = g_strdup_printf( "%s (%s)", stamp, ofo_taux_get_maj_user( priv->taux ));
			gtk_label_set_text( label, str );
			g_free( str );
			g_free( stamp );
		}
	}

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaTauxProperties *self, gint code )
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
on_mnemo_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

/*
static void
on_begin_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	const gchar *content;
	gchar *str, *markup;
	GtkLabel *label;

	content = gtk_entry_get_text( entry );
	g_date_set_parse( &self->private->begin, content );

	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-begin-label" ));
	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );
	} else if( g_date_valid( &self->private->begin )){
		str = my_utils_display_from_date( &self->private->begin, MY_UTILS_DATE_DMMM );
	} else {
		str = g_strdup( _( "invalid" ));
	}
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", str );
	gtk_label_set_markup( label, markup );
	g_free( str );
	g_free( markup );

	check_for_enable_dlg( self );
}

static void
on_end_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	const gchar *content;
	gchar *str, *markup;
	GtkLabel *label;

	content = gtk_entry_get_text( entry );
	g_date_set_parse( &self->private->end, content );

	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-end-label" ));
	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );
	} else if( g_date_valid( &self->private->end )){
		str = my_utils_display_from_date( &self->private->end, MY_UTILS_DATE_DMMM );
	} else {
		str = g_strdup( _( "invalid" ));
	}
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", str );
	gtk_label_set_markup( label, markup );
	g_free( str );
	g_free( markup );

	check_for_enable_dlg( self );
}

static void
on_taux_changed( GtkEntry *entry, ofaTauxProperties *self )
{
	const gchar *content;
	gchar *str, *markup;
	GtkLabel *label;

	content = gtk_entry_get_text( entry );
	self->private->value = g_ascii_strtod( content, NULL );

	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-taux-label" ));

	if( !content || !g_utf8_strlen( content, -1 )){
		str = g_strdup( "" );
	} else {
		str = g_strdup_printf( "%.3lf", self->private->value );
	}
	markup = g_markup_printf_escaped( "<span style=\"italic\">%s</span>", str );
	gtk_label_set_markup( label, markup );
	g_free( str );
	g_free( markup );

	check_for_enable_dlg( self );
}*/

static void
check_for_enable_dlg( ofaTauxProperties *self )
{
	ofaTauxPropertiesPrivate *priv;
	GtkWidget *button;
	/*GtkEntry *entry;
	const gchar *str;
	gboolean begin_ok, end_ok, taux_ok;*/

	priv = self->private;

	/*entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-begin" ));
	str = gtk_entry_get_text( entry );
	begin_ok = ( !str || !g_utf8_strlen( str, -1 ) || g_date_valid( &priv->begin ));

	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-end" ));
	str = gtk_entry_get_text( entry );
	end_ok = ( !str || !g_utf8_strlen( str, -1 ) || g_date_valid( &priv->end ));

	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-taux" ));
	str = gtk_entry_get_text( entry );
	taux_ok = ( str && g_utf8_strlen( str, -1 ));*/

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button,
			priv->mnemo && g_utf8_strlen( priv->mnemo, -1 ) &&
			priv->label && g_utf8_strlen( priv->label, -1 ));
}

/*
 * either creating a new taux (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaTauxProperties *self )
{
	ofoDossier *dossier;
	ofoTaux *preventer;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *notes;

	/* - we are defining a new mnemo (either by creating a new record
	 *   or by modifying an existing record to a new mnemo)
	 *   then all is fine
	 * - we are defining a mnemo which already exists
	 *   so have to check that this new validity period is consistant
	 *   with he periods already defined by the existing mnemo
	 *
	 * As a conclusion, the only case of an error may occur is when
	 * there is an already existing mnemo which covers the desired
	 * validity period
	 *
	 */
	dossier = ofa_main_window_get_dossier( self->private->main_window );
	/*
	preventer = ofo_taux_is_data_valid(
			dossier,
			self->private->id, self->private->mnemo, &self->private->begin, &self->private->end );*/
	preventer = NULL;

	if( preventer ){
		error_duplicate( self, preventer );
		return( FALSE );
	}

	ofo_taux_set_mnemo( self->private->taux, self->private->mnemo );
	ofo_taux_set_label( self->private->taux, self->private->label );
	/*ofo_taux_set_val_begin( self->private->taux, &self->private->begin );
	ofo_taux_set_val_end( self->private->taux, &self->private->end );
	ofo_taux_set_taux( self->private->taux, self->private->value );*/

	text = GTK_TEXT_VIEW(
			my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-notes" ));
	buffer = gtk_text_view_get_buffer( text );
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
	ofo_taux_set_notes( self->private->taux, notes );
	g_free( notes );

	if( self->private->id == -1 ){
		self->private->updated =
				ofo_taux_insert( self->private->taux, dossier );
	} else {
		self->private->updated =
				ofo_taux_update( self->private->taux, dossier );
	}

	return( self->private->updated );
}

static void
error_duplicate( ofaTauxProperties *self, ofoTaux *preventer )
{
	GtkMessageDialog *dlg;
	/*gchar *sbegin, *send;*/
	gchar *msg;

	/*sbegin = my_utils_display_from_date( &self->private->begin, MY_UTILS_DATE_DMMM );
	if( !g_utf8_strlen( sbegin, -1 )){
		g_free( sbegin );
		sbegin = g_strdup( _( "(unlimited)" ));
	}
	send = my_utils_display_from_date( &self->private->end, MY_UTILS_DATE_DMMM );
	if( !g_utf8_strlen( send, -1 )){
		g_free( send );
		send = g_strdup( _( "(illimité)" ));
	}*/

	/*_( "Unable to set '%s' asked mnemonic and validity period as "
		"these overlap with the already existing '%s' whose "
		"current validity is from %s to %s" ),*/

	msg = g_strdup_printf(
				_( "Unable to set '%s' asked mnemonic and validity period as "
					"these overlap with the already existing '%s'" ),
				ofo_taux_get_mnemo( preventer ),
				ofo_taux_get_label( preventer ));

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( self->private->dialog ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));

	g_free( msg );
	/*g_free( send );
	g_free( sbegin );*/
}
