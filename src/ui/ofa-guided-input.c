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
#include "ui/ofa-guided-input.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaGuidedInputClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaGuidedInputPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow  *main_window;
	GtkDialog      *dialog;
	const ofoModel *model;

	/* data
	 */
	gint            journal_id;
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-guided-input.ui";
static const gchar  *st_ui_id        = "GuidedInputDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaGuidedInputClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaGuidedInput *self, ofaMainWindow *main, const ofoModel *model );
static gboolean  ok_to_terminate( ofaGuidedInput *self, gint code );
static void      init_dialog_journal( ofaGuidedInput *self );
static void      init_dialog_entries( ofaGuidedInput *self );
static void      on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaGuidedInput *self );
static void      check_for_enable_dlg( ofaGuidedInput *self );
static gboolean  do_update( ofaGuidedInput *self );

GType
ofa_guided_input_get_type( void )
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
	static const gchar *thisfn = "ofa_guided_input_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaGuidedInputClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaGuidedInput ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaGuidedInput", &info, 0 );

	return( type );
}

static void
class_init( ofaGuidedInputClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaGuidedInputClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_guided_input_instance_init";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_GUIDED_INPUT( instance );

	self->private = g_new0( ofaGuidedInputPrivate, 1 );

	self->private->dispose_has_run = FALSE;

	self->private->journal_id = -1;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_guided_input_instance_dispose";
	ofaGuidedInputPrivate *priv;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( window ));

	priv = ( OFA_GUIDED_INPUT( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

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
	static const gchar *thisfn = "ofa_guided_input_instance_finalize";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_GUIDED_INPUT( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_guided_input_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
void
ofa_guided_input_run( ofaMainWindow *main_window, const ofoModel *model )
{
	static const gchar *thisfn = "ofa_guided_input_run";
	ofaGuidedInput *self;
	gint code;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, journal=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new( OFA_TYPE_GUIDED_INPUT, NULL );

	do_initialize_dialog( self, main_window, model );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	g_object_unref( self );
}

static void
do_initialize_dialog( ofaGuidedInput *self, ofaMainWindow *main, const ofoModel *model )
{
	static const gchar *thisfn = "ofa_guided_input_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaGuidedInputPrivate *priv;

	priv = self->private;
	priv->main_window = main;
	priv->model = model;

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

		init_dialog_journal( self );
		init_dialog_entries( self );
	}

	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaGuidedInput *self, gint code )
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
init_dialog_journal( ofaGuidedInput *self )
{
	ofa_journal_combo_init_dialog(
			self->private->dialog, "p1-journal", NULL,
			ofa_main_window_get_dossier( self->private->main_window ),
			FALSE, TRUE,
			( ofaJournalComboCb ) on_journal_changed, self,
			ofo_model_get_journal( self->private->model ));
}

static void
init_dialog_entries( ofaGuidedInput *self )
{

}

static void
on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaGuidedInput *self )
{
	self->private->journal_id = id;

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaGuidedInput *self )
{

}

/*
 * either creating a new journal (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 */
static gboolean
do_update( ofaGuidedInput *self )
{
	return( TRUE );
}
