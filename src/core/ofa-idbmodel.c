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

#include "my/my-char.h"
#include "my/my-idialog.h"
#include "my/my-iident.h"
#include "my/my-iprogress.h"
#include "my/my-iwindow.h"
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

#define IDBMODEL_LAST_VERSION             1

/* default imported datas
 *
 * Note that the construct
 *   static const gchar *st_classes = INIT1DIR "/classes-h1.csv";
 * plus a reference to st_classes in the array initializer
 * is refused with the message "initializer element is not constant"
 */
typedef GType ( *fnType )( void );

typedef struct {
	const gchar *label;
	const gchar *table;
	const gchar *filename;
	gint         header_count;
	fnType       typefn;
}
	sImport;

static sImport st_imports[] = {
		{ N_( "Classes" ),
				"OFA_T_CLASSES",       "classes-h1.csv",       1, ofo_class_get_type },
		{ N_( "Currencies" ),
				"OFA_T_CURRENCIES",    "currencies-h1.csv",    1, ofo_currency_get_type },
		{ N_( "Accounts" ),
				"OFA_T_ACCOUNTS",      "accounts-h1.csv",      1, ofo_account_get_type },
		{ N_( "Ledgers" ),
				"OFA_T_LEDGERS",       "ledgers-h1.csv",       1, ofo_ledger_get_type },
		{ N_( "Operation templates" ),
				"OFA_T_OPE_TEMPLATES", "ope-templates-h2.csv", 2, ofo_ope_template_get_type },
		{ N_( "Rates" ),
				"OFA_T_RATES",         "rates-h2.csv",         2, ofo_rate_get_type },
		{ 0 }
};

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
	GList         *workers;
	gboolean       work_started;

	/* UI
	 */
	GtkWidget     *close_btn;
	GtkWidget     *paned;
	GtkWidget     *upper_viewport;
	GtkWidget     *objects_grid;
	guint          objects_row;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;

	/* settings
	 */
	gint           paned_pos;
};

/* a data structure for each myIProgress worker
 */
typedef struct {
	const void    *worker;
	GtkWidget     *grid1;
	GtkWidget     *grid2;
	GtkWidget     *grid3;
	gint           row2;
	myProgressBar *bar;
}
	sWorker;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-idbmodel.ui";

/* interface management */
static GType    register_type( void );
static void     interface_base_init( ofaIDBModelInterface *klass );
static void     interface_base_finalize( ofaIDBModelInterface *klass );
static gboolean idbmodel_get_needs_update( const ofaIDBModel *instance, const ofaIDBConnect *connect );
static gboolean idbmodel_ddl_update( ofaIDBModel *instance, ofaHub *hub, myIProgress *dialog );

/* dialog management */
static GType    ofa_dbmodel_window_get_type( void ) G_GNUC_CONST;
static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname );
static void     iwindow_write_settings( myIWindow *instance, myISettings *settings, const gchar *keyname );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static gboolean do_run( ofaDBModelWindow *dialog );
static gboolean do_update_model( ofaDBModelWindow *self );
static gboolean do_import_data( ofaDBModelWindow *self );
static gboolean import_utf8_comma_pipe_file( ofaDBModelWindow *self, sImport *import );
static gint     count_rows( ofaDBModelWindow *self, const gchar *table );
static void     on_grid_size_allocate( GtkWidget *grid, GdkRectangle *allocation, ofaDBModelWindow *dialog );
static void     iprogress_iface_init( myIProgressInterface *iface );
static void     iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget );
static void     iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar );
static void     iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total );
static void     iprogress_set_row( myIProgress *instance, const void *worker, GtkWidget *widget );
static void     iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count );
static void     iprogress_set_text( myIProgress *instance, const void *worker, const gchar *text );
static sWorker *get_worker_data( ofaDBModelWindow *self, const void *worker );

G_DEFINE_TYPE_EXTENDED( ofaDBModelWindow, ofa_dbmodel_window, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaDBModelWindow )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IPROGRESS, iprogress_iface_init )
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
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idbmodel_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBMODEL );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBModelInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBModelInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBModel::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbmodel_update:
 * @hub: the #ofaHub object.
 * @parent: the #GtkWindow parent window.
 *
 * Ask all ofaIDBModel implentations whether they need to update their
 * current DB model, and run them.
 * A modal dialog box is displayed if any DDL update has to be run.
 *
 * Returns: %TRUE if the DDL updates are all OK (or not needed), %FALSE
 * else.
 */
gboolean
ofa_idbmodel_update( ofaHub *hub, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_idbmodel_update";
	ofaExtenderCollection *extenders;
	GList *plugins_list, *it;
	gboolean need_update, ok;
	ofaDBModelWindow *window;
	ofaDBModelWindowPrivate *priv;
	const ofaIDBConnect *connect;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	ok = TRUE;
	need_update = FALSE;
	connect = ofa_hub_get_connect( hub );
	extenders = ofa_hub_get_extender_collection( hub );
	plugins_list = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IDBMODEL );
	g_debug( "%s: IDBModel plugins count=%u", thisfn, g_list_length( plugins_list ));

	for( it=plugins_list ; it && !need_update ; it=it->next ){
		need_update = idbmodel_get_needs_update( OFA_IDBMODEL( it->data ), connect );
	}

	if( need_update ){
		window = g_object_new( OFA_TYPE_DBMODEL_WINDOW, NULL );
		my_iwindow_set_parent( MY_IWINDOW( window ), parent );
		my_iwindow_set_settings( MY_IWINDOW( window ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

		priv = ofa_dbmodel_window_get_instance_private( window );

		priv->plugins_list = plugins_list;
		priv->hub = hub;

		ok = FALSE;
		if( my_idialog_run( MY_IDIALOG( window )) == GTK_RESPONSE_OK ){
			ok = TRUE;
			my_iwindow_close( MY_IWINDOW( window ));
		}
	}

	ofa_extender_collection_free_types( plugins_list );

	return( ok );
}

/**
 * ofa_idbmodel_get_is_deletable:
 * @object: the #ofoBase object to be checked.
 *
 * Returns: %TRUE if all the plugins accept that this object be deleted.
 */
gboolean
ofa_idbmodel_get_is_deletable( const ofaHub *hub, const ofoBase *object )
{
	return( TRUE );
}

/**
 * ofa_idbmodel_get_by_name:
 * @hub: the #ofaHub object.
 * @name: the searched for identification name.
 *
 * Returns: the #ofaIDBModel instance which delivers this canonical
 * @name, or %NULL.
 *
 * This method relies on the #myIIdent interface being implemented by
 * the #ofaIDBModel objects.
 *
 * The returned reference is owned by the application, and should not
 * be unreffed by the caller.
 */
ofaIDBModel *
ofa_idbmodel_get_by_name( const ofaHub *hub, const gchar *name )
{
	ofaExtenderCollection *extenders;
	GList *plugins_list, *it;
	ofaIDBModel *found;
	gchar *it_name;
	gint cmp;

	g_return_val_if_fail( my_strlen( name ), NULL );

	found = NULL;
	extenders = ofa_hub_get_extender_collection( hub );
	plugins_list = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IDBMODEL );

	for( it=plugins_list ; it ; it=it->next ){
		it_name = ofa_idbmodel_get_canon_name( OFA_IDBMODEL( it->data ));
		cmp = my_collate( it_name, name );
		g_free( it_name );
		if( cmp == 0 ){
			found = OFA_IDBMODEL( it->data );
			break;
		}
	}

	ofa_extender_collection_free_types( plugins_list );

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

	g_info( "%s: ofaIDBModel's %s implementation does not provide 'get_current_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
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

	g_info( "%s: ofaIDBModel's %s implementation does not provide 'get_last_version()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( G_MAXUINT );
}

static gboolean
idbmodel_get_needs_update( const ofaIDBModel *instance, const ofaIDBConnect *connect )
{
	static const gchar *thisfn = "ofa_idbmodel_get_needs_update";
	guint cur_version, last_version;

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->needs_update ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->needs_update( instance, connect ));
	}

	g_info( "%s: ofaIDBModel's %s implementation does not provide 'needs_update()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));

	cur_version = ofa_idbmodel_get_current_version( instance, connect );
	last_version = ofa_idbmodel_get_last_version( instance, connect );

	return( cur_version < last_version );
}

static gboolean
idbmodel_ddl_update( ofaIDBModel *instance, ofaHub *hub, myIProgress *window )
{
	static const gchar *thisfn = "ofa_idbmodel_ddl_update";

	if( OFA_IDBMODEL_GET_INTERFACE( instance )->ddl_update ){
		return( OFA_IDBMODEL_GET_INTERFACE( instance )->ddl_update( instance, hub, window ));
	}

	g_info( "%s: ofaIDBModel's %s implementation does not provide 'ddl_update()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( TRUE );
}

/*
 * ofa_idbmodel_get_canon_name:
 * @instance: this #ofaIDBModel instance.
 *
 * Returns: the canonical name of the @instance, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the @instance class.
 */
gchar *
ofa_idbmodel_get_canon_name( const ofaIDBModel *instance )
{
	g_return_val_if_fail( instance && OFA_IS_IDBMODEL( instance ), NULL );

	return( MY_IS_IIDENT( instance ) ? my_iident_get_canon_name( MY_IIDENT( instance ), NULL ) : NULL );
}

/*
 * ofa_idbmodel_get_version:
 * @instance: this #ofaIDBModel instance.
 * @connect: the #ofaIDBConnect object.
 *
 * Returns: the current version of the @instance, as a newly
 * allocated string which should be g_free() by the caller, or %NULL.
 *
 * This method relies on the #myIIdent identification interface,
 * which is expected to be implemented by the @instance class.
 */
gchar *
ofa_idbmodel_get_version( const ofaIDBModel *instance, ofaIDBConnect *connect )
{
	g_return_val_if_fail( instance && OFA_IS_IDBMODEL( instance ), NULL );

	return( MY_IS_IIDENT( instance ) ? my_iident_get_version( MY_IIDENT( instance ), connect ) : NULL );
}

static void
dbmodel_window_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbmodel_window_finalize";
	ofaDBModelWindowPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBMODEL_WINDOW( instance ));

	/* free data members here */
	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	g_list_free_full( priv->workers, ( GDestroyNotify ) g_free );

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

	iface->read_settings = iwindow_read_settings;
	iface->write_settings = iwindow_write_settings;
}

/*
 * settings are a string list, with:
 * - paned pos
 */
static void
iwindow_read_settings( myIWindow *instance, myISettings *settings, const gchar *keyname )
{
	ofaDBModelWindowPrivate *priv;
	GList *slist, *it;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	slist = my_isettings_get_string_list( settings, SETTINGS_GROUP_GENERAL, keyname );
	it = slist ? slist : NULL;
	priv->paned_pos = it ? atoi( it->data ) : 150;

	ofa_settings_free_string_list( slist );
}

static void
iwindow_write_settings( myIWindow *instance, myISettings *settings, const gchar *keyname )
{
	ofaDBModelWindowPrivate *priv;
	gchar *str;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	str = g_strdup_printf( "%d;", gtk_paned_get_position( GTK_PANED( priv->paned )));

	my_isettings_set_string( settings, SETTINGS_GROUP_GENERAL, keyname, str );

	g_free( str );
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

	priv->objects_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dud-grid" );
	g_return_if_fail( priv->objects_grid && GTK_IS_GRID( priv->objects_grid ));
	priv->objects_row = 0;
	g_signal_connect( priv->objects_grid, "size-allocate", G_CALLBACK( on_grid_size_allocate ), instance );

	priv->textview = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "dud-textview" );
	g_return_if_fail( priv->textview && GTK_IS_TEXT_VIEW( priv->textview ));
	priv->text_buffer = gtk_text_buffer_new( NULL );
	gtk_text_buffer_set_text( priv->text_buffer, "", -1 );
	gtk_text_view_set_buffer( GTK_TEXT_VIEW( priv->textview ), priv->text_buffer );

	g_idle_add(( GSourceFunc ) do_run, instance );
}

/*
 * First upgrade the CORE DBModel; only then upgrade other IDBModels.
 * Last import default datas.
 */
static gboolean
do_run( ofaDBModelWindow *self )
{
	ofaDBModelWindowPrivate *priv;
	gboolean ok;
	gchar *str;
	GtkMessageType type;

	priv = ofa_dbmodel_window_get_instance_private( self );

	ok = do_update_model( self ) &&
			do_import_data( self );

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

static gboolean
do_update_model( ofaDBModelWindow *self )
{
	ofaDBModelWindowPrivate *priv;
	ofaIDBModel *core_model;
	gboolean ok;
	GList *it;

	priv = ofa_dbmodel_window_get_instance_private( self );

	core_model = ofa_idbmodel_get_by_name( priv->hub, "CORE" );
	ok = core_model ? idbmodel_ddl_update( core_model, priv->hub, MY_IPROGRESS( self )) : TRUE;

	for( it=priv->plugins_list ; it && ok ; it=it->next ){
		if( it->data != ( void * ) core_model ){
			ok &= idbmodel_ddl_update( OFA_IDBMODEL( it->data ), priv->hub, MY_IPROGRESS( self ));
		}
	}

	return( ok );
}

static gboolean
do_import_data( ofaDBModelWindow *self )
{
	ofaDBModelWindowPrivate *priv;
	gboolean ok;
	gint i;

	priv = ofa_dbmodel_window_get_instance_private( self );

	ok = TRUE;
	priv->work_started = FALSE;

	for( i=0 ; st_imports[i].label && ok ; ++i ){
		ok &= import_utf8_comma_pipe_file( self, &st_imports[i] );
	}

	return( ok );
}

/*
 * only import provided default datas if target table is empty
 */
static gboolean
import_utf8_comma_pipe_file( ofaDBModelWindow *self, sImport *import )
{
	ofaDBModelWindowPrivate *priv;
	gboolean ok;
	guint count, errors;
	gchar *uri, *fname, *str;
	GType type;
	ofaIImporter *importer;
	ofsImporterParms parms;
	ofaStreamFormat *settings;
	GtkWidget *label;

	priv = ofa_dbmodel_window_get_instance_private( self );

	ok = TRUE;
	count = count_rows( self, import->table );
	if( count == 0 ){

		/* find an importer for these uri+type */
		fname = g_strdup_printf( "%s/%s", INIT1DIR, import->filename );
		uri = g_filename_to_uri( fname, NULL, NULL );
		g_free( fname );
		type = import->typefn();
		importer = ofa_hub_get_willing_to( priv->hub, uri, type );

		/* if found, then import data */
		if( importer ){
			g_return_val_if_fail( OFA_IS_IIMPORTER( importer ), FALSE );

			if( !priv->work_started ){
				priv->work_started = TRUE;
				label = gtk_label_new( _( " Setting default datas " ));
				my_iprogress_start_work( MY_IPROGRESS( self ), self, label );
			}

			str = g_strdup_printf( _( "Importing into %s :" ), import->table );
			label = gtk_label_new( str );
			g_free( str );
			my_iprogress_start_progress( MY_IPROGRESS( self ), self, label, FALSE );

			settings = ofa_stream_format_new( NULL, OFA_SFMODE_IMPORT );
			ofa_stream_format_set( settings,
										TRUE,  "UTF-8", 				/* charmap */
										TRUE,  MY_DATE_SQL, 			/* date format */
										FALSE, MY_CHAR_ZERO,			/* no thousand sep */
										TRUE,  MY_CHAR_COMMA,			/* comma decimal sep */
										TRUE,  MY_CHAR_PIPE, 			/* pipe field sep */
										FALSE, MY_CHAR_ZERO, 			/* no string delimiter */
										import->header_count );

			memset( &parms, '\0', sizeof( parms ));
			parms.version = 1;
			parms.hub = priv->hub;
			parms.empty = TRUE;
			parms.mode = OFA_IDUPLICATE_ABORT;
			parms.stop = FALSE;
			parms.uri = uri;
			parms.type = type;
			parms.format = settings;

			errors = ofa_iimporter_import( importer, &parms );

			if( errors ){
				str = g_strdup( _( "error detected" ));
				ok = FALSE;
			} else {
				str = g_strdup_printf( _( "%d successfully imported records" ), parms.inserted_count );
			}
			label = gtk_label_new( str );
			g_free( str );
			my_iprogress_set_row( MY_IPROGRESS( self ), self, label );

			g_object_unref( settings );
			g_object_unref( importer );
		}
	}

	return( ok );
}

static gint
count_rows( ofaDBModelWindow *self, const gchar *table )
{
	ofaDBModelWindowPrivate *priv;
	const ofaIDBConnect *connect;
	gint count;
	gchar *query;

	priv = ofa_dbmodel_window_get_instance_private( self );

	connect = ofa_hub_get_connect( priv->hub );
	query = g_strdup_printf( "SELECT COUNT(*) FROM %s", table );
	ofa_idbconnect_query_int( connect, query, &count, TRUE );
	g_free( query );

	return( count );
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
 * myIProgress interface management
 */
static void
iprogress_iface_init( myIProgressInterface *iface )
{
	static const gchar *thisfn = "ofa_idbmodel_iprogress_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->start_work = iprogress_start_work;
	iface->start_progress = iprogress_start_progress;
	iface->pulse = iprogress_pulse;
	iface->set_row = iprogress_set_row;
	iface->set_ok = iprogress_set_ok;
	iface->set_text = iprogress_set_text;
}

/*
 * expects a GtkLabel
 */
static void
iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget )
{
	ofaDBModelWindowPrivate *priv;
	GtkWidget *frame;
	sWorker *sdata;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	sdata = get_worker_data( OFA_DBMODEL_WINDOW( instance ), worker );

	/* the first time: create the frame and a first grid */
	if( !sdata->grid1 ){
		frame = gtk_frame_new( NULL );
		gtk_widget_set_hexpand( frame, TRUE );
		my_utils_widget_set_margin_right( frame, 16 );
		gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

		if( widget ){
			gtk_frame_set_label_widget( GTK_FRAME( frame ), widget );
		}

		sdata->grid1 = gtk_grid_new();
		my_utils_widget_set_margins( sdata->grid1, 4, 4, 20, 16 );
		gtk_container_add( GTK_CONTAINER( frame ), sdata->grid1 );
		gtk_grid_set_row_spacing( GTK_GRID( sdata->grid1 ), 3 );

		gtk_grid_attach( GTK_GRID( priv->objects_grid ), frame, 0, priv->objects_row++, 1, 1 );

	/* the second time: create a second grid */
	} else if( !sdata->grid2 ){
		gtk_grid_attach( GTK_GRID( sdata->grid1 ), widget, 0, 0, 1, 1 );

		sdata->grid2 = gtk_grid_new();
		gtk_grid_set_row_spacing( GTK_GRID( sdata->grid2 ), 3 );
		gtk_grid_set_column_spacing( GTK_GRID( sdata->grid2 ), 12 );

		gtk_grid_attach( GTK_GRID( sdata->grid1 ), sdata->grid2, 0, 1, 1, 1 );
		sdata->row2 = 0;
	}

	gtk_widget_show_all( priv->objects_grid );
}

static void
iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar )
{
	sWorker *sdata;
	GtkGrid *grid;

	sdata = get_worker_data( OFA_DBMODEL_WINDOW( instance ), worker );

	if( widget ){
		if( !with_bar ){
			sdata->grid3 = gtk_grid_new();
			gtk_grid_set_column_spacing( GTK_GRID( sdata->grid3 ), 4 );
			gtk_grid_attach( GTK_GRID( sdata->grid3 ), widget, 0, 0, 1, 1 );
			grid = GTK_GRID( sdata->grid2 ? sdata->grid2 : sdata->grid1 );
			gtk_grid_attach( grid, sdata->grid3, 0, sdata->row2, 3, 1 );

		} else {
			gtk_grid_attach( GTK_GRID( sdata->grid2 ), widget, 0, sdata->row2, 1, 1 );
		}
	}

	if( with_bar ){
		sdata->bar = my_progress_bar_new();
		gtk_grid_attach( GTK_GRID( sdata->grid2 ), GTK_WIDGET( sdata->bar ), 1, sdata->row2, 1, 1 );
	}

	if( widget || with_bar ){
		sdata->row2 += 1;
	}

	gtk_widget_show_all( sdata->grid1 );
}

static void
iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total )
{
	sWorker *sdata;
	gdouble progress;
	gchar *str;

	sdata = get_worker_data( OFA_DBMODEL_WINDOW( instance ), worker );

	if( sdata->bar ){
		progress = total ? ( gdouble ) count / ( gdouble ) total : 0;
		g_signal_emit_by_name( sdata->bar, "my-double", progress );

		str = g_strdup_printf( "%.0lf%%", 100*progress );
		g_signal_emit_by_name( sdata->bar, "my-text", str );
	}
}

static void
iprogress_set_row( myIProgress *instance, const void *worker, GtkWidget *widget )
{
	sWorker *sdata;

	sdata = get_worker_data( OFA_DBMODEL_WINDOW( instance ), worker );

	if( widget ){
		gtk_grid_attach( GTK_GRID( sdata->grid3 ), widget, 1, 0, 1, 1 );
	}

	gtk_widget_show_all( sdata->grid3 );
}

static void
iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count )
{
	sWorker *sdata;
	GtkWidget *label;

	sdata = get_worker_data( OFA_DBMODEL_WINDOW( instance ), worker );

	if( widget ){
		gtk_grid_attach( GTK_GRID( sdata->grid2 ), widget, 1, 0, 1, 1 );
	}

	if( sdata->bar ){
		label = gtk_label_new( errs_count ? _( "NOT OK" ) : _( "OK" ));
		gtk_widget_set_valign( label, GTK_ALIGN_END );
		my_utils_widget_set_style( label, errs_count == 0 ? "labelinfo" : "labelerror" );

		gtk_grid_attach( GTK_GRID( sdata->grid2 ), label, 2, sdata->row2-1, 1, 1 );
	}

	gtk_widget_show_all( sdata->grid1 );
}

static void
iprogress_set_text( myIProgress *instance, const void *worker, const gchar *text )
{
	ofaDBModelWindowPrivate *priv;
	GtkTextIter iter;
	gchar *str;
	GtkAdjustment* adjustment;

	priv = ofa_dbmodel_window_get_instance_private( OFA_DBMODEL_WINDOW( instance ));

	gtk_text_buffer_get_end_iter( priv->text_buffer, &iter );

	str = g_strdup_printf( "%s\n", text );
	gtk_text_buffer_insert( priv->text_buffer, &iter, str, -1 );
	g_free( str );

	adjustment = gtk_scrollable_get_vadjustment( GTK_SCROLLABLE( priv->textview ));
	gtk_adjustment_set_value( adjustment, gtk_adjustment_get_upper( adjustment ));
}

static sWorker *
get_worker_data( ofaDBModelWindow *self, const void *worker )
{
	ofaDBModelWindowPrivate *priv;
	GList *it;
	sWorker *sdata;

	priv = ofa_dbmodel_window_get_instance_private( self );

	for( it=priv->workers ; it ; it=it->next ){
		sdata = ( sWorker * ) it->data;
		if( sdata->worker == worker ){
			return( sdata );
		}
	}

	sdata = g_new0( sWorker, 1 );
	sdata->worker = worker;
	priv->workers = g_list_prepend( priv->workers, sdata );

	return( sdata );
}
