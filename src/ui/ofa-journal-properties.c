/*
 * Open Freelance Journaling
 * A double-entry journaling application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance Journaling is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance Journaling is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance Journaling; see the file COPYING. If not,
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
#include "ui/ofa-journal-properties.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaJournalPropertiesClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaJournalPropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow *main_window;
	GtkDialog     *dialog;
	ofoJournal    *journal;
	gboolean       updated;

	/* data
	 */
	gchar         *mnemo;
	gchar         *label;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
	GDate          cloture;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-journal-properties.ui";
static const gchar  *st_ui_id        = "JournalPropertiesDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaJournalPropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaJournalProperties *self, ofaMainWindow *main, ofoJournal *journal );
static gboolean  ok_to_terminate( ofaJournalProperties *self, gint code );
static void      on_mnemo_changed( GtkEntry *entry, ofaJournalProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaJournalProperties *self );
static void      check_for_enable_dlg( ofaJournalProperties *self );
static gboolean  do_update( ofaJournalProperties *self );
static void      error_duplicate( ofaJournalProperties *self, ofoJournal *existing, const gchar *prev_number );

GType
ofa_journal_properties_get_type( void )
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
	static const gchar *thisfn = "ofa_journal_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaJournalPropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaJournalProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaJournalProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaJournalPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_journal_properties_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaJournalPropertiesClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_journal_properties_instance_init";
	ofaJournalProperties *self;

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_JOURNAL_PROPERTIES( instance );

	self->private = g_new0( ofaJournalPropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_journal_properties_instance_dispose";
	ofaJournalPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( window ));

	priv = ( OFA_JOURNAL_PROPERTIES( window ))->private;

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
	static const gchar *thisfn = "ofa_journal_properties_instance_finalize";
	ofaJournalProperties *self;

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_JOURNAL_PROPERTIES( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_journal_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
gboolean
ofa_journal_properties_run( ofaMainWindow *main_window, ofoJournal *journal )
{
	static const gchar *thisfn = "ofa_journal_properties_run";
	ofaJournalProperties *self;
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, journal=%p",
			thisfn, ( void * ) main_window, ( void * ) journal );

	self = g_object_new( OFA_TYPE_JOURNAL_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, journal );

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
do_initialize_dialog( ofaJournalProperties *self, ofaMainWindow *main, ofoJournal *journal )
{
	static const gchar *thisfn = "ofa_journal_properties_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaJournalPropertiesPrivate *priv;
	gchar *title;
	const gchar *jou_mnemo;
	GtkEntry *entry;
	gchar *notes;
	GtkTextView *text;
	GtkTextBuffer *buffer;

	priv = self->private;
	priv->main_window = main;
	priv->journal = journal;

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

		jou_mnemo = ofo_journal_get_mnemo( journal );
		if( !jou_mnemo ){
			title = g_strdup( _( "Defining a new journal" ));
		} else {
			title = g_strdup_printf( _( "Updating journal %s" ), jou_mnemo );
		}
		gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );

		priv->mnemo = g_strdup( ofo_journal_get_mnemo( journal ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-mnemo" ));
		if( priv->mnemo ){
			gtk_entry_set_text( entry, priv->mnemo );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );

		priv->label = g_strdup( ofo_journal_get_label( journal ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
		if( priv->label ){
			gtk_entry_set_text( entry, priv->label );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

		notes = g_strdup( ofo_journal_get_notes( journal ));
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
ok_to_terminate( ofaJournalProperties *self, gint code )
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
on_mnemo_changed( GtkEntry *entry, ofaJournalProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaJournalProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaJournalProperties *self )
{
	ofaJournalPropertiesPrivate *priv;
	GtkWidget *button;

	priv = self->private;
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button,
			priv->mnemo && g_utf8_strlen( priv->mnemo, -1 ) &&
			priv->label && g_utf8_strlen( priv->label, -1 ));
}

static gboolean
do_update( ofaJournalProperties *self )
{
	gchar *prev_mnemo;
	ofoDossier *dossier;
	ofoJournal *existing;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *notes;

	prev_mnemo = g_strdup( ofo_journal_get_mnemo( self->private->journal ));
	dossier = ofa_main_window_get_dossier( self->private->main_window );
	existing = ofo_dossier_get_journal( dossier, self->private->mnemo );

	if( existing ){
		/* c'est un nouveau journal, ou bien un journal existant dont on
		 * veut changer le numéro: no luck, le nouveau mnemo de journal
		 * existe déjà
		 */
		if( !prev_mnemo || g_utf8_collate( prev_mnemo, self->private->mnemo )){
			error_duplicate( self, existing, prev_mnemo );
			g_free( prev_mnemo );
			return( FALSE );
		}
	}

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même journal (n'a pas été modifié)
	 */
	ofo_journal_set_mnemo( self->private->journal, self->private->mnemo );
	ofo_journal_set_label( self->private->journal, self->private->label );

	text = GTK_TEXT_VIEW(
			my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p2-notes" ));
	buffer = gtk_text_view_get_buffer( text );
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
	ofo_journal_set_notes( self->private->journal, notes );
	g_free( notes );

	if( !prev_mnemo ){
		self->private->updated =
				ofo_dossier_insert_journal( dossier, self->private->journal );
	} else {
		self->private->updated =
				ofo_dossier_update_journal( dossier, self->private->journal, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( self->private->updated );
}

static void
error_duplicate( ofaJournalProperties *self, ofoJournal *existing, const gchar *prev_mnemo )
{
	GtkMessageDialog *dlg;
	gchar *msg;

	if( prev_mnemo ){
		msg = g_strdup_printf(
				_( "Il est impossible d'effectuer les modifications demandées "
					"car le nouveau mnémonique '%s' est déjà utilisé par le journal '%s'." ),
				ofo_journal_get_mnemo( existing ),
				ofo_journal_get_label( existing ));
	} else {
		msg = g_strdup_printf(
				_( "Il est impossible de définir ce nouveau journal "
					"car son mnémonique '%s' est déjà utilisé par le journal '%s'." ),
				ofo_journal_get_mnemo( existing ),
				ofo_journal_get_label( existing ));
	}

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( self->private->dialog ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
	g_free( msg );
}
