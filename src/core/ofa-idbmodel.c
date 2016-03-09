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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-plugin.h"
#include "api/ofa-settings.h"

#define IDBMODEL_LAST_VERSION           1

static guint st_initializations         = 0;	/* interface initialization count */

#define OFA_TYPE_DBMODEL_WINDOW                ( ofa_dbmodel_window_get_type())
#define OFA_DBMODEL_WINDOW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBMODEL_WINDOW, ofaDBModelWindow ))
#define OFA_DBMODEL_WINDOW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBMODEL_WINDOW, ofaDBModelWindowClass ))
#define OFA_IS_DBMODEL_WINDOW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBMODEL_WINDOW ))
#define OFA_IS_DBMODEL_WINDOW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBMODEL_WINDOW ))
#define OFA_DBMODEL_WINDOW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBMODEL_WINDOW, ofaDBModelWindowClass ))

typedef struct _ofaDBModelWindowPrivate        ofaDBModelWindowPrivate;

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDBModelWindow;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDBModelWindowClass;

/* private instance data
 */
struct _ofaDBModelWindowPrivate {
		gboolean   dispose_has_run;

	/* runtime
	 */
	GList         *plugins_list;
	ofaHub        *hub;

	/* UI
	 */
	GtkWidget     *close_btn;
	GtkWidget     *paned;
	GtkWidget     *upper_viewport;
	GtkWidget     *grid;
	guint          row;
	GtkWidget     *textview;
	GtkTextBuffer *buffer;

	/* settings
	 */
	gint           paned_pos;

	/* returned value
	 */
	gboolean       updated;
};

static const gchar      *st_resource_ui = "/org/trychlos/openbook/core/ofa-idbmodel.ui";
static const gchar      *st_settings    = "DBModelWindow-settings";

/* interface management */
static GType    register_type( void );
static void     interface_base_init( ofaIDBModelInterface *klass );
static void     interface_base_finalize( ofaIDBModelInterface *klass );
static gboolean idbmodel_get_needs_update( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static gboolean idbmodel_ddl_update( const ofaIDBModel *instance, ofaHub *hub, myIWindow *dialog );

/* dialog management */
static GType    ofa_dbmodel_window_get_type( void ) G_GNUC_CONST;
static void     iwindow_iface_init( myIWindowInterface *iface );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static gboolean do_run( ofaDBModelWindow *dialog );
static void     on_grid_size_allocate( GtkWidget *grid, GdkRectangle *allocation, ofaDBModelWindow *dialog );
static void     load_settings( ofaDBModelWindow *dialog );
static void     write_settings( ofaDBModelWindow *dialog );

G_DEFINE_TYPE_EXTENDED( ofaDBModelWindow, ofa_dbmodel_window, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDBModelWindow )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

/**
 * ofa_idbmodel_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbmodel_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbmodel_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbmodel_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBModelInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBModel", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBModelInterface *klass )
{
	static const gchar *thisfn = "ofa_idbmodel_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBModelInterface *klass )
{
	static const gchar *thisfn = "ofa_idbmodel_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbmodel_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbmodel_get_interface_last_version( void )
{
	return( IDBMODEL_LAST_VERSION );
}

/**
 * ofa_idbmodel_get_interface_version:
 * @instance: this #ofaIDBModel instance.
 *
 * Returns: the version number of this interface the plugin implements.
 */
guint
ofa_idbmodel_get_interface_version( const ofaIDBModel *instance )
{
	static const gchar *thisfn = "ofa_idbmodel_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBMODEL( instance ), 0 );

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIDBModel instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_idbmodel_update:
 * @hub: the #ofaHub object.
 *
 * Ask all ofaIDBModel implentations whether they need to update their
 * current DB model, and run them.
 * A modal dialog box is displayed if any DDL update has to be run.
 *
 * Returns: %TRUE if the DDL updates are all OK (or not needed), %FALSE
 * else.
 */
gboolean
ofa_idbmodel_update( ofaHub *hub )
{
	GList *plugins_list, *it;
	gboolean need_update, ok;
	ofaDBModelWindow *window;
	ofaDBModelWindowPrivate *priv;
	const ofaIDBConnect *connect;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	ok = TRUE;
	need_update = FALSE;
	plugins_list = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBMODEL );
	connect = ofa_hub_get_connect( hub );

	for( it=plugins_list ; it && !need_update ; it=it->next ){
		need_update = idbmodel_get_needs_update( OFA_IDBMODEL( it->data ), connect );
		if( need_update ){
			break;
		}
	}

	if( need_update ){
		window = g_object_new( OFA_TYPE_DBMODEL_WINDOW, NULL );
		my_iwindow_set_main_window( MY_IWINDOW( window ), NULL );

		priv = ofa_dbmodel_window_get_instance_private( window );

		priv->plugins_list = plugins_list;
		priv->hub = hub;

		ok = FALSE;
		load_settings( window );
		if( my_idialog_run( MY_IDIALOG( window )) == GTK_RESPONSE_OK ){
			ok = TRUE;
			my_iwindow_close( MY_IWINDOW( window ));
		}
		write_settings( window );
	}

	ofa_plugin_free_extensions( plugins_list );

	return( ok );
}

/**
 * ofa_idbmodel_init_hub_signaling_system:
 * @hub: the #ofaHub object.
 *
 * Propose to all ofaIDBModel implentations to connect themselves to
 * the hub signaling system.
 */
void
ofa_idbmodel_init_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofa_idbmodel_init_hub_signaling_system";
	GList *plugins_list, *it;
	ofaIDBModel *instance;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	plugins_list = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBMODEL );

	for( it=plugins_list ; it ; it=it->next ){
		instance = OFA_IDBMODEL( it->data );
		if( OFA_IDBMODEL_GET_INTERFACE( instance )->connect_handlers ){
			OFA_IDBMODEL_GET_INTERFACE( instance )->connect_handlers( instance, hub );
		} else {
			g_info( "%s: ofaIDBModel instance %p does not provide 'connect_handlers()' method",
					thisfn, ( void * ) instance );
		}
	}

	ofa_plugin_free_extensions( plugins_list );
}

/**
 * ofa_idbmodel_get_by_name:
 * @name: the searched for identification name.
 *
 * Returns: a new reference to the #ofaIDBModel instance which delivers
 * this @name, or %NULL.
 *
 * When non %NULL, the returned reference should be g_object_unref() by
 * the caller.
 */
ofaIDBModel *
ofa_idbmodel_get_by_name( const gchar *name )
{
	static const gchar *thisfn = "ofa_idbmodel_get_by_name";
	GList *plugins_list, *it;
	ofaIDBModel *instance, *found;
	const gchar *cstr;

	g_return_val_if_fail( my_strlen( name ), NULL );

	found = NULL;
	plugins_list = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBMODEL );

	for( it=plugins_list ; it ; it=it->next ){
		instance = OFA_IDBMODEL( it->data );
		if( OFA_IDBMODEL_GET_INTERFACE( instance )->get_name ){
			cstr = OFA_IDBMODEL_GET_INTERFACE( instance )->get_name( instance );
			if( !g_utf8_collate( cstr, name )){
				found = g_object_ref( instance );
				break;
			}
		} else {
			g_info( "%s: ofaIDBModel instance %p does not provide 'get_name()' method",
					thisfn, ( void * ) instance );
		}
	}

	ofa_plugin_free_extensions( plugins_list );
	return( found );
}

/**
 * ofa_idbmodel_get_current_version:
 * @instance:
 * @connect:
 *
 * Returns: the current version of the DB model.
 */
guint
ofa_idbmodel_get_current_version( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_idbmodel_get_current_version";

	g_return_val_if_fail( instance && OFA_IS_IDBMODEL( instance ), G_MAXUINT );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), G_MAXUINT );

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->get_current_version ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->get_current_version( instance, connect ));
	}

	g_info( "%s: ofaIDBModel instance %p does not provide 'get_current_version()' method",
			thisfn, ( void * ) instance );
	return( G_MAXUINT );
}

/**
 * ofa_idbmodel_get_last_version:
 * @instance:
 * @dossier:
 *
 * Returns: the last version available for this DB model.
 */
guint
ofa_idbmodel_get_last_version( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_idbmodel_get_last_version";

	g_return_val_if_fail( instance && OFA_IS_IDBMODEL( instance ), G_MAXUINT );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), G_MAXUINT );

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->get_last_version ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->get_last_version( instance, connect ));
	}

	g_info( "%s: ofaIDBModel instance %p does not provide 'get_last_version()' method",
			thisfn, ( void * ) instance );
	return( G_MAXUINT );
}

static gboolean
idbmodel_get_needs_update( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	guint cur_version, last_version;

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->needs_update ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->needs_update( instance, connect ));
	}

	cur_version = ofa_idbmodel_get_current_version( instance, connect );
	last_version = ofa_idbmodel_get_last_version( instance, connect );

	return( cur_version < last_version );
}

static gboolean
idbmodel_ddl_update( const ofaIDBModel *instance, ofaHub *hub, myIWindow *dialog )
{
	static const gchar *thisfn = "ofa_idbmodel_ddl_update";

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->ddl_update ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->ddl_update( instance, hub, dialog ));
	}

	g_info( "%s: ofaIDBModel instance %p does not provide 'ddl_update()' method",
			thisfn, ( void * ) instance );
	return( TRUE );
}

static void
dbmodel_window_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbmodel_window_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBMODEL_WINDOW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbmodel_window_parent_class )->finalize( instance );
}

static void
dbmodel_window_dispose( GObject *instance )
{
	ofaDBModelWindowPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DBMODEL_WINDOW( instance ));

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbmodel_window_parent_class )->dispose( instance );
}

static void
ofa_dbmodel_window_init( ofaDBModelWindow *self )
{
	static const gchar *thisfn = "ofa_dbmodel_window_init";
	ofaDBModelWindowPrivate *priv;

	g_return_if_fail( self && OFA_IS_DBMODEL_WINDOW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_dbmodel_window_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_dbmodel_window_class_init( ofaDBModelWindowClass *klass )
{
	static const gchar *thisfn = "ofa_dbmodel_window_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbmodel_window_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbmodel_window_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_idbmodel_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_idbmodel_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this is a two-panes dialog
 * - labels are set on upper side
 *    Main DB model:
 *       last version: 23
 *       current version: 21
 *          upgrade to v22: ok
 *          upgrade to v23: ok
 *       last version: 23
 *         is up to date.
 * - downside is a textview which displays the DDL commands
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_idbmodel_idialog_init";
	ofaDBModelWindowPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	priv->close_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "close-btn" );
	g_return_if_fail( priv->close_btn && GTK_IS_BUTTON( priv->close_btn ));
	gtk_widget_set_sensitive( priv->close_btn, FALSE );

	priv->paned = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dud-paned" );
	g_return_if_fail( priv->paned && GTK_IS_PANED( priv->paned ));
	gtk_paned_set_position( GTK_PANED( priv->paned ), priv->paned_pos );

	priv->upper_viewport = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dud-upperviewport" );
	g_return_if_fail( priv->upper_viewport && GTK_IS_VIEWPORT( priv->upper_viewport ));

	priv->grid = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dud-grid" );
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));
	priv->row = 0;
	g_signal_connect( priv->grid, "size-allocate", G_CALLBACK( on_grid_size_allocate ), instance );

	priv->textview = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dud-textview" );
	g_return_if_fail( priv->textview && GTK_IS_TEXT_VIEW( priv->textview ));
	priv->buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( priv->buffer, "", -1 );
	gtk_text_view_set_buffer( GTK_TEXT_VIEW( priv->textview ), priv->buffer );

	g_idle_add(( GSourceFunc ) do_run, instance );
}

static gboolean
do_run( ofaDBModelWindow *self )
{
	ofaDBModelWindowPrivate *priv;
	GList *it;
	gboolean ok;
	gchar *str;
	GtkMessageType type;

	priv = ofa_dbmodel_window_get_instance_private( self );

	ok = TRUE;

	for( it=priv->plugins_list ; it && ok ; it=it->next ){
		ok = idbmodel_ddl_update( OFA_IDBMODEL( it->data ), priv->hub, MY_IWINDOW( self ));
	}

	priv->updated = ok;

	if( ok ){
		str = g_strdup( _( "The database has been successfully updated" ));
		type = GTK_MESSAGE_INFO;
	} else {
		str = g_strdup( _( "An error has occured while upgrading the database model" ));
		type = GTK_MESSAGE_WARNING;
	}

	my_iwindow_msg_dialog( MY_IWINDOW( self ), type, str );
	g_free( str );

	gtk_widget_set_sensitive( priv->close_btn, TRUE );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/**
 * ofa_idbmodel_add_row_widget:
 * @instance: the #ofaIDBModel implementation instance.
 * @window: the #ofaDBModelWindow dialog.
 * @widget: a widget to be added to the upper grid.
 *
 * Adds a row to the grid.
 */
void
ofa_idbmodel_add_row_widget( const ofaIDBModel *instance, myIWindow *window, GtkWidget *widget )
{
	ofaDBModelWindowPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IDBMODEL( instance ));
	g_return_if_fail( window && OFA_IS_DBMODEL_WINDOW( window ));
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( window ));

	g_return_if_fail( !priv->dispose_has_run );

	gtk_grid_attach( GTK_GRID( priv->grid ), widget, 0, priv->row, 1, 1 );
	priv->row += 1;
	gtk_widget_show_all( priv->grid );
}

/**
 * ofa_idbmodel_add_text:
 * @instance: the #ofaIDBModel implementation instance.
 * @window: the #ofaDBModelWindow window.
 * @text: the text to be added.
 *
 * Adds some text to the lower textview.
 */
void
ofa_idbmodel_add_text( const ofaIDBModel *instance, myIWindow *window, const gchar *text )
{
	ofaDBModelWindowPrivate *priv;
	GtkTextIter iter;
	gchar *str;

	g_return_if_fail( instance && OFA_IS_IDBMODEL( instance ));
	g_return_if_fail( window && OFA_IS_DBMODEL_WINDOW( window ));

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( window ));

	g_return_if_fail( !priv->dispose_has_run );

	gtk_text_buffer_get_end_iter( priv->buffer, &iter );
	str = g_strdup_printf( "%s\n", text );
	gtk_text_buffer_insert( priv->buffer, &iter, str, -1 );
	g_free( str );
	gtk_text_buffer_get_end_iter( priv->buffer, &iter );
	gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( priv->textview ), &iter, 0, FALSE, 0, 0 );
}

/*
 * thanks to http://stackoverflow.com/questions/2683531/stuck-on-scrolling-gtkviewport-to-end
 */
static void
on_grid_size_allocate( GtkWidget *grid, GdkRectangle *allocation, ofaDBModelWindow *window )
{
	ofaDBModelWindowPrivate *priv;
	GtkAdjustment* adjustment;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( window ));

	adjustment = gtk_scrollable_get_vadjustment( GTK_SCROLLABLE( priv->upper_viewport ));
	gtk_adjustment_set_value( adjustment, gtk_adjustment_get_upper( adjustment ));
}

/*
 * settings are a string list, with:
 * - paned pos
 */
static void
load_settings( ofaDBModelWindow *window )
{
	ofaDBModelWindowPrivate *priv;
	GList *slist, *it;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( window ));

	slist = ofa_settings_user_get_string_list( st_settings );
	it = slist ? slist : NULL;
	priv->paned_pos = it ? atoi( it->data ) : 50;

	ofa_settings_free_string_list( slist );
}

static void
write_settings( ofaDBModelWindow *window )
{
	ofaDBModelWindowPrivate *priv;
	gchar *str;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( window ));

	str = g_strdup_printf( "%d;",
			gtk_paned_get_position( GTK_PANED( priv->paned )));

	ofa_settings_user_set_string( st_settings, str );

	g_free( str );
}
