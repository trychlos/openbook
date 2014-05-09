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
#include "ui/ofa-dossier-open.h"
#include "ui/ofa-settings.h"
#include "ui/ofo-dossier.h"

#if 0
static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;
#endif

/* private class data
 */
struct _ofaDossierOpenClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaDossierOpenPrivate {
	gboolean        dispose_has_run;

	/* properties
	 */
	ofaMainWindow  *main_window;

	/* internals
	 */
	GtkDialog      *dialog;
#if 0
	gboolean        escape_key_pressed;
#endif
	gchar          *name;
	gchar          *account;
	gchar          *password;

	/* return value
	 * the structure itself, along with its datas, will be freed by the
	 * MainWindow signal final handler
	 */
	ofaOpenDossier *ood;
};

/* class properties
 */
enum {
	OFA_PROP_0,

	OFA_PROP_TOPLEVEL_ID,

	OFA_PROP_N_PROPERTIES
};

#define PROP_TOPLEVEL                  "dossier-open-prop-toplevel"

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-dossier-open.ui";
static const gchar  *st_ui_id        = "DossierOpenDlg";

/* column ordering in the selection listview
 */
enum {
	COL_NAME = 0,
	N_COLUMNS
};

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaDossierOpenClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec );
static void      instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec );
static void      instance_constructed( GObject *instance );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
#if 0
static gboolean  on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaDossierOpen *self );
#endif
static gboolean  ok_to_terminate( ofaDossierOpen *self, gint code );
static void      do_initialize_dialog( ofaDossierOpen *self );
static void      on_dossier_selected( GtkTreeSelection *selection, ofaDossierOpen *self );
static void      on_account_changed( GtkEntry *entry, ofaDossierOpen *self );
static void      on_password_changed( GtkEntry *entry, ofaDossierOpen *self );
static void      check_for_enable_dlg( ofaDossierOpen *self );
static gboolean  do_open( ofaDossierOpen *self );
static void      connect_error( GtkWindow *parent, gchar *host, gchar *account, gchar *dbname, gint port, gchar *socket, MYSQL *mysql );

GType
ofa_dossier_open_get_type( void )
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
	static const gchar *thisfn = "ofa_dossier_open_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaDossierOpenClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaDossierOpen ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaDossierOpen", &info, 0 );

	return( type );
}

static void
class_init( ofaDossierOpenClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_open_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->get_property = instance_get_property;
	object_class->set_property = instance_set_property;
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	g_object_class_install_property( object_class, OFA_PROP_TOPLEVEL_ID,
			g_param_spec_pointer(
					PROP_TOPLEVEL,
					"Main window",
					"A pointer (not a ref) to the toplevel parent main window",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	klass->private = g_new0( ofaDossierOpenClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_dossier_open_instance_init";
	ofaDossierOpen *self;

	g_return_if_fail( OFA_IS_DOSSIER_OPEN( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_DOSSIER_OPEN( instance );

	self->private = g_new0( ofaDossierOpenPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaDossierOpenPrivate *priv;

	g_return_if_fail( OFA_IS_DOSSIER_OPEN( object ));
	priv = OFA_DOSSIER_OPEN( object )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_TOPLEVEL_ID:
				g_value_set_pointer( value, priv->main_window );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaDossierOpenPrivate *priv;

	g_return_if_fail( OFA_IS_DOSSIER_OPEN( object ));
	priv = OFA_DOSSIER_OPEN( object )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_TOPLEVEL_ID:
				priv->main_window = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_constructed( GObject *window )
{
	static const gchar *thisfn = "ofa_dossier_open_instance_constructed";
	ofaDossierOpenPrivate *priv;
	GtkBuilder *builder;
	GError *error;

	g_return_if_fail( OFA_IS_DOSSIER_OPEN( window ));

	priv = OFA_DOSSIER_OPEN( window )->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->constructed ){
			G_OBJECT_CLASS( st_parent_class )->constructed( window );
		}

		/* create the GtkDialog */
		error = NULL;
		builder = gtk_builder_new();
		if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
			priv->dialog = GTK_DIALOG( gtk_builder_get_object( builder, st_ui_id ));
			if( priv->dialog ){
				do_initialize_dialog( OFA_DOSSIER_OPEN( window ));
			} else {
				g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
			}
		} else {
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}
		g_object_unref( builder );
	}
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_dossier_open_instance_dispose";
	ofaDossierOpenPrivate *priv;

	g_return_if_fail( OFA_IS_DOSSIER_OPEN( window ));

	priv = ( OFA_DOSSIER_OPEN( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->name );
		g_free( priv->account );
		g_free( priv->password );

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
	static const gchar *thisfn = "ofa_dossier_open_instance_finalize";
	ofaDossierOpen *self;

	g_return_if_fail( OFA_IS_DOSSIER_OPEN( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_DOSSIER_OPEN( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_dossier_open_run:
 * @main: the main window of the application.
 *
 * Run the selection dialog to choose a dossier to be opened
 */
ofaOpenDossier *
ofa_dossier_open_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_open_run";
	ofaDossierOpen *self;
	gint code;
	ofaOpenDossier *ood;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_OPEN,
				PROP_TOPLEVEL, main_window,
				NULL );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	ood = self->private->ood;
	g_object_unref( self );
	return( ood );
}

#if 0
static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, ofaDossierOpen *self )
{
	gboolean stop = FALSE;

	g_return_val_if_fail( OFA_IS_DOSSIER_OPEN( self ), FALSE );

	if( !self->private->dispose_has_run ){

		if( event->keyval == GDK_KEY_Escape && pref_quit_on_escape ){

				self->private->escape_key_pressed = TRUE;
				g_signal_emit_by_name( self->private->dialog, "cancel", self );
				stop = TRUE;
		}
	}

	return( stop );
}
#endif

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaDossierOpen *self, gint code )
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
			quit = do_open( self );
			break;
	}

	return( quit );
}

static void
do_initialize_dialog( ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_do_initialize_dialog";
	GtkDialog *dialog;
	GtkTreeView *listview;
	GtkTreeModel *model;
	GtkCellRenderer *text_cell;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GSList *dossiers, *id;
	GtkTreeSelection *select;
	GtkEntry *entry;

	g_debug( "%s: self=%p (%s)",
			thisfn,
			( void * ) self, G_OBJECT_TYPE_NAME( self ));

	dialog = self->private->dialog;

#if 0
	/* deals with 'Esc' key */
	g_signal_connect( assistant,
			"key-press-event", G_CALLBACK( on_key_pressed_event ), self );
#endif

	dossiers = ofa_settings_get_dossiers();

	listview = GTK_TREE_VIEW( my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "treeview" ));
	model = GTK_TREE_MODEL( gtk_list_store_new( N_COLUMNS, G_TYPE_STRING ));
	gtk_tree_view_set_model( listview, model );
	g_object_unref( model );

	text_cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"label",
			text_cell,
			"text", COL_NAME,
			NULL );
	gtk_tree_view_append_column( listview, column );

	select = gtk_tree_view_get_selection( listview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
	g_signal_connect(G_OBJECT( select ), "changed", G_CALLBACK( on_dossier_selected ), self );

	for( id=dossiers ; id ; id=id->next ){
		gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
		gtk_list_store_set(
				GTK_LIST_STORE( model ),
				&iter,
				COL_NAME, ( const gchar * ) id->data,
				-1 );
	}

	g_slist_free_full( dossiers, ( GDestroyNotify ) g_free );

	gtk_tree_model_get_iter_first( model, &iter );
	gtk_tree_selection_select_iter( select, &iter );

	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "account" ));
	g_signal_connect(G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );

	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( dialog ), "password" ));
	g_signal_connect(G_OBJECT( entry ), "changed", G_CALLBACK( on_password_changed ), self );

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( dialog ));
}

static void
on_dossier_selected( GtkTreeSelection *selection, ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_on_dossier_selected";
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_debug( "%s: selection=%p, self=%p", thisfn, ( void * ) selection, ( void * ) self );

	if( gtk_tree_selection_get_selected( selection, &model, &iter )){
		g_free( self->private->name );
		gtk_tree_model_get( model, &iter, COL_NAME, &self->private->name, -1 );
		g_debug( "%s: name=%s", thisfn, self->private->name );
	}

	check_for_enable_dlg( self );
}

static void
on_account_changed( GtkEntry *entry, ofaDossierOpen *self )
{
	g_free( self->private->account );
	self->private->account = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEntry *entry, ofaDossierOpen *self )
{
	g_free( self->private->password );
	self->private->password = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierOpen *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "bt-open" );
	gtk_widget_set_sensitive( button,
			self->private->name && self->private->account && self->private->password );
}

/*
 * is called when the user click on the 'Open' button
 * return %TRUE if we can open a connection, %FALSE else
 */
static gboolean
do_open( ofaDossierOpen *self )
{
	static const gchar *thisfn = "ofa_dossier_open_do_open";
	gboolean connected;
	ofaOpenDossier *sod;
	MYSQL mysql;

	connected = FALSE;
	sod = g_new0( ofaOpenDossier, 1 );
	ofa_settings_get_dossier( self->private->name, &sod->host, &sod->port, &sod->socket, &sod->dbname );

	mysql_init( &mysql );

	if( !mysql_real_connect( &mysql,
			sod->host,
			self->private->account,
			self->private->password,
			sod->dbname,
			sod->port,
			sod->socket,
			CLIENT_MULTI_RESULTS )){

		connect_error( GTK_WINDOW( self->private->dialog),
				sod->host, self->private->account, sod->dbname, sod->port, sod->socket, &mysql );

		g_free( sod->host );
		g_free( sod->socket );
		g_free( sod->dbname );
		g_free( sod );

	} else {
		g_debug( "%s: connection successfully opened", thisfn );
		mysql_close( &mysql );
		connected = TRUE;
		sod->dossier = g_strdup( self->private->name );
		sod->account = g_strdup( self->private->account );
		sod->password = g_strdup( self->private->password );
		self->private->ood = sod;
	}

	return( connected );
}

static void
connect_error( GtkWindow *parent,
		gchar *host, gchar *account, gchar *dbname, gint port, gchar *socket,
		MYSQL *mysql )
{
	GtkMessageDialog *dlg;
	GString *str;

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				parent,
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", _( "Unable to connect to the database")));

	str = g_string_new( "" );
	if( host ){
		g_string_append_printf( str, "Host: %s\n", host );
	}
	if( port > 0 ){
		g_string_append_printf( str, "Port: %d\n", port );
	}
	if( socket ){
		g_string_append_printf( str, "Socket: %s\n", socket );
	}
	if( dbname ){
		g_string_append_printf( str, "Database: %s\n", dbname );
	}
	if( account ){
		g_string_append_printf( str, "Account: %s\n", account );
	}
	gtk_message_dialog_format_secondary_text( dlg, "%s", str->str );
	g_string_free( str, TRUE );

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
}
