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
#include "ui/ofa-journal-properties.h"
#include "ui/ofo-dossier.h"

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

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;
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
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_journal_properties_instance_dispose";
	ofaJournalPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( instance ));

	priv = ( OFA_JOURNAL_PROPERTIES( instance ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
		g_free( priv->label );
		g_free( priv->maj_user );

		gtk_widget_destroy( GTK_WIDGET( priv->dialog ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( instance );
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_journal_properties_instance_finalize";
	ofaJournalProperties *self;

	g_return_if_fail( OFA_IS_JOURNAL_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_JOURNAL_PROPERTIES( instance );

	g_free( self->private );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( instance );
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
			title = g_strdup_printf( _( "Updating « %s » journal" ), jou_mnemo );
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

		my_utils_init_notes_ex( journal );

		if( jou_mnemo ){
			my_utils_init_maj_user_stamp_ex( journal );
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
	ofoJournal *exists;
	gboolean ok;

	priv = self->private;
	button = my_utils_container_get_child_by_name(
						GTK_CONTAINER( self->private->dialog ), "btn-ok" );
	ok = ofo_journal_is_valid( priv->mnemo, priv->label );
	if( ok ){
		exists = ofo_journal_get_by_mnemo(
				ofa_main_window_get_dossier( priv->main_window ), priv->mnemo );
		ok &= !exists ||
				( ofo_journal_get_id( exists ) == ofo_journal_get_id( priv->journal ));
	}

	gtk_widget_set_sensitive( button, ok );
}

/*
 * either creating a new journal (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 */
static gboolean
do_update( ofaJournalProperties *self )
{
	ofoDossier *dossier;
	ofoJournal *existing;

	dossier = ofa_main_window_get_dossier( self->private->main_window );
	existing = ofo_journal_get_by_mnemo( dossier, self->private->mnemo );
	g_return_val_if_fail(
			!existing ||
			ofo_journal_get_id( existing ) == ofo_journal_get_id( self->private->journal ), NULL );

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même journal (n'a pas été modifié)
	 */
	ofo_journal_set_mnemo( self->private->journal, self->private->mnemo );
	ofo_journal_set_label( self->private->journal, self->private->label );

	my_utils_getback_notes_ex( journal );

	if( !existing ){
		self->private->updated =
				ofo_journal_insert( self->private->journal, dossier );
	} else {
		self->private->updated =
				ofo_journal_update( self->private->journal, dossier );
	}

	return( self->private->updated );
}
