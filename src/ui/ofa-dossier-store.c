/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-settings-monitor.h"

#include "ui/ofa-dossier-store.h"

/* private instance data
 */
struct _ofaDossierStorePrivate {
	gboolean            dispose_has_run;

	/* runtime data
	 */
	ofaSettingsMonitor *settings_monitor;
	guint               rows_count;
};

static GType st_col_types[DOSSIER_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* dname, dbms, dbname */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* end, begin, displayable status */
		G_TYPE_STRING									/* code status */
};

static ofaDossierStore *st_store        = NULL;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void     on_dossier_settings_changed( ofaSettingsMonitor *monitor, guint target, ofaDossierStore *store );
static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *store );
static guint    load_dataset( ofaDossierStore *store );
static void     insert_row( ofaDossierStore *store, const gchar *dname, const gchar *provider, const gchar *strlist );
static void     set_row( ofaDossierStore *store, const gchar *dname, const gchar *provider, const gchar *strlist, GtkTreeIter *iter );
static gboolean get_iter_from_dbname( ofaDossierStore *store, const gchar *dname, const gchar *dbname, GtkTreeIter *iter );
static guint    dossier_store_reload( void );

G_DEFINE_TYPE( ofaDossierStore, ofa_dossier_store, GTK_TYPE_LIST_STORE )

static void
dossier_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_store_parent_class )->finalize( instance );
}

static void
dossier_store_dispose( GObject *instance )
{
	ofaDossierStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_STORE( instance ));

	priv = OFA_DOSSIER_STORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->settings_monitor ){
			g_object_unref( priv->settings_monitor );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_store_parent_class )->dispose( instance );
}

static void
ofa_dossier_store_init( ofaDossierStore *self )
{
	static const gchar *thisfn = "ofa_dossier_store_init";

	g_return_if_fail( OFA_IS_DOSSIER_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_STORE, ofaDossierStorePrivate );
}

static void
ofa_dossier_store_class_init( ofaDossierStoreClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_store_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierStorePrivate ));

	/**
	 * ofaDossierStore:
	 *
	 * This signal is sent when the content of the store has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierStore *store,
	 * 						guint          rows_count,
	 * 						gpointer       user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_DOSSIER_STORE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_UINT );
}

/**
 * ofa_dossier_store_new:
 *
 * The #ofaDossierStore class implements a singleton. Each returned
 * pointer is a new reference to the same instance of the class.
 * This unique instance is allocated on demand, when the
 * #ofa_dossier_store_new() method is called for the first time.
 *
 * Returns: the #ofaDossierStore instance.
 */
ofaDossierStore *
ofa_dossier_store_new( void )
{
	if( !st_store ){

		/* define the store */
		st_store = g_object_new( OFA_TYPE_DOSSIER_STORE, NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( st_store ), DOSSIER_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( st_store ), ( GtkTreeIterCompareFunc ) on_sort_model, st_store, NULL );

		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( st_store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		/* load the data */
		load_dataset( st_store );

		/* monitor the settings */
		st_store->priv->settings_monitor = ofa_settings_monitor_new( SETTINGS_TARGET_DOSSIER );

		g_signal_connect(
				st_store->priv->settings_monitor,
				"changed",
				G_CALLBACK( on_dossier_settings_changed ),
				st_store );
	}

	return( g_object_ref( st_store ));
}

/*
 * callback triggered when the dossier settings has changed
 */
static void
on_dossier_settings_changed( ofaSettingsMonitor *monitor, guint target, ofaDossierStore *store )
{
	static const gchar *thisfn = "ofa_dossier_store_on_dossier_settings_changed";
	guint count;

	g_debug( "%s: target=%u", thisfn, target );

	g_return_if_fail( target == SETTINGS_TARGET_DOSSIER );

	count = dossier_store_reload();

	g_signal_emit_by_name( store, "changed", count );
}

/**
 * ofa_dossier_store_free:
 *
 * This function is expected to be only called from application dispose.
 * It unrefs all left references on the ofaDossierStore singleton.
 */
void
ofa_dossier_store_free( void )
{
	guint i, count;

	if( st_store ){
		count = G_OBJECT( st_store )->ref_count;
		for( i=count ; i>0 ; --i ){
			g_object_unref( st_store );
		}
	}
}

/*
 * sorting the store per dossier name
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *store )
{
	gchar *adname, *bdname;
	gint cmp;

	gtk_tree_model_get( tmodel, a, DOSSIER_COL_DNAME, &adname, -1 );
	gtk_tree_model_get( tmodel, b, DOSSIER_COL_DNAME, &bdname, -1 );

	cmp = g_utf8_collate( adname, bdname );

	g_free( adname );
	g_free( bdname );

	return( cmp );
}

/*
 * load the dataset
 */
static guint
load_dataset( ofaDossierStore *store )
{
	GSList *dataset, *it;
	gchar **array;
	const gchar *dname, *provider;
	GSList *exeset, *ite;
	guint count;

	dataset = ofa_dossier_misc_get_dossiers();
	count = 0;

	for( it=dataset ; it ; it=it->next ){
		array = g_strsplit(( const gchar * ) it->data, ";", -1 );
		dname = ( const gchar * ) *array;
		provider = ( const gchar * ) *( array+1 );

		exeset = ofa_dossier_misc_get_exercices( dname );
		for( ite=exeset ; ite ; ite=ite->next ){
			insert_row( store, dname, provider, ( const gchar * ) ite->data );
			count += 1;
		}
		ofa_dossier_misc_free_exercices( exeset );

		g_strfreev( array );
	}

	ofa_dossier_misc_free_dossiers( dataset );
	store->priv->rows_count = count;

	return( count );
}

static void
insert_row( ofaDossierStore *store, const gchar *dname, const gchar *provider, const gchar *strlist )
{
	static const gchar *thisfn = "ofa_dossier_store_insert_row";
	GtkTreeIter iter;

	g_debug( "%s: store=%p, dname=%s, provider=%s, strlist=%s",
			thisfn, ( void * ) store, dname, provider, strlist );

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, dname, provider, strlist, &iter );
}

static void
set_row( ofaDossierStore *store, const gchar *dname, const gchar *provider, const gchar *strlist, GtkTreeIter *iter )
{
	gchar **array;
	const gchar *dbname, *sbegin, *send, *status, *code;
	gchar *sdbegin, *sdend;
	GDate dbegin, dend;

	array = g_strsplit( strlist, ";", -1 );
	dbname = ( const gchar * ) *( array+1 );
	sbegin = ( const gchar * ) *( array+2 );
	send = ( const gchar * ) *( array+3 );
	status = ( const gchar * ) *( array+4 );
	code = ( const gchar * ) *( array+5 );

	my_date_set_from_str( &dbegin, sbegin, MY_DATE_SQL );
	my_date_set_from_str( &dend, send, MY_DATE_SQL );

	sdbegin = my_date_to_str( &dbegin, ofa_prefs_date_display());
	sdend = my_date_to_str( &dend, ofa_prefs_date_display());

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			DOSSIER_COL_DNAME,  dname,
			DOSSIER_COL_DBMS,   provider,
			DOSSIER_COL_DBNAME, dbname,
			DOSSIER_COL_BEGIN,  sdbegin,
			DOSSIER_COL_END,    sdend,
			DOSSIER_COL_STATUS, status,
			DOSSIER_COL_CODE,   code,
			-1 );

	g_free( sdbegin );
	g_free( sdend );
	g_strfreev( array );
}

/*
 * ofa_dossier_store_reload:
 *
 * Reset and reload the datastore content.
 */
static guint
dossier_store_reload( void )
{
	guint count = 0;

	if( st_store ){
		gtk_list_store_clear( GTK_LIST_STORE( st_store ));
		count = load_dataset( st_store );
	}

	return( count );
}

/*
 * ofa_dossier_store_is_empty:
 * @store: the #ofaDossierStore instance.
 *
 * Returns: %TRUE if the store is empty, %FALSE else
 */
gboolean
ofa_dossier_store_is_empty( ofaDossierStore *store )
{
	ofaDossierStorePrivate *priv;
	gboolean is_empty;

	g_return_val_if_fail( store && OFA_IS_DOSSIER_STORE( store ), TRUE );

	priv = store->priv;
	is_empty = TRUE;

	if( !priv->dispose_has_run ){
		is_empty = ( priv->rows_count == 0 );
	}

	return( is_empty );
}

/**
 * ofa_dossier_store_remove_exercice:
 * @store:
 * @dname:
 * @dbname:
 *
 * Remove the row which corresponds to the given specs.
 */
void
ofa_dossier_store_remove_exercice( ofaDossierStore *store, const gchar *dname, const gchar *dbname )
{
	ofaDossierStorePrivate *priv;
	GtkTreeIter iter;

	g_return_if_fail( store && OFA_IS_DOSSIER_STORE( store ));

	priv = store->priv;

	if( !priv->dispose_has_run ){

		if( get_iter_from_dbname( store, dname, dbname, &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( store ), &iter );
		}
	}
}

static gboolean
get_iter_from_dbname( ofaDossierStore *store, const gchar *dname, const gchar *dbname, GtkTreeIter *iter )
{
	gchar *row_dname, *row_dbname;
	gboolean found;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		while( TRUE ){
			gtk_tree_model_get(
					GTK_TREE_MODEL( store ), iter,
					DOSSIER_COL_DNAME, &row_dname, DOSSIER_COL_DBNAME, &row_dbname, -1 );
			found = g_utf8_collate( row_dname, dname ) == 0 &&
					g_utf8_collate( row_dbname, dbname ) == 0;
			g_free( row_dname );
			g_free( row_dbname );
			if( found ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}
