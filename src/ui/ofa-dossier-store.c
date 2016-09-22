/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-idbmeta.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dossier-store.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaDossierStorePrivate;

static GType st_col_types[DOSSIER_N_COLUMNS] = {
		G_TYPE_STRING, 					/* dossier name */
		G_TYPE_STRING, 					/* DBMS provider name */
		G_TYPE_STRING,					/* period name */
		G_TYPE_STRING, 					/* end date (user display) */
		G_TYPE_STRING, 					/* begin date (user display) */
		G_TYPE_STRING, 					/* localized status */
		G_TYPE_BOOLEAN, 				/* is_current */
		G_TYPE_OBJECT,					/* ofaIDBMeta */
		G_TYPE_OBJECT					/* ofaIDBPeriod */
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static ofaDossierStore *st_store                = NULL;
static guint            st_signals[ N_SIGNALS ] = { 0 };

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *store );
static void     on_file_dir_changed( ofaPortfolioCollection *dir, guint count, const gchar *filename, ofaDossierStore *store );
static void     load_dataset( ofaDossierStore *store, ofaPortfolioCollection *dir );
static void     insert_row( ofaDossierStore *store, const ofaIDBMeta *meta, const ofaIDBPeriod *period );
static void     set_row( ofaDossierStore *store, const ofaIDBMeta *meta, const ofaIDBPeriod *period, GtkTreeIter *iter );

G_DEFINE_TYPE_EXTENDED( ofaDossierStore, ofa_dossier_store, GTK_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaDossierStore ))

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

	priv = ofa_dossier_store_get_instance_private( OFA_DOSSIER_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_store_parent_class )->dispose( instance );
}

static void
ofa_dossier_store_init( ofaDossierStore *self )
{
	static const gchar *thisfn = "ofa_dossier_store_init";
	ofaDossierStorePrivate *priv;

	g_return_if_fail( self && OFA_IS_DOSSIER_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_dossier_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_dossier_store_class_init( ofaDossierStoreClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_store_finalize;

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
 * @dir: [allow-none]: the #ofaPortfolioCollection instance which centralize the
 *  list of defined dossiers. This must be non-null at first call
 *  (instanciation time), while is not used on successive calls.
 *
 * The #ofaDossierStore class implements a singleton. Each returned
 * pointer is a new reference to the same instance of the class.
 * This unique instance is allocated on demand, when the
 * #ofa_dossier_store_new() method is called for the first time.
 *
 * Returns: a new reference on the #ofaDossierStore instance, which
 * must be g_object_unref() by the caller.
 */
ofaDossierStore *
ofa_dossier_store_new( ofaPortfolioCollection *dir )
{
	ofaDossierStore *store;

	if( st_store ){
		store = g_object_ref( st_store );

	} else {
		store = g_object_new( OFA_TYPE_DOSSIER_STORE, NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), DOSSIER_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );

		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_signal_connect( dir, "changed", G_CALLBACK( on_file_dir_changed ), store );
		load_dataset( store, dir );
		st_store = store;
	}

	return( store );
}

/*
 * sorting the store per:
 * - dossier name ascending
 * - exercice descending
 *
 * The result is visible in the dossier manager which displays both
 * dossier names and dates of exercices
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *store )
{
	gchar *adname, *abegin, *bdname, *bbegin;
	gint cmp;

	gtk_tree_model_get( tmodel, a, DOSSIER_COL_DOSNAME, &adname, DOSSIER_COL_BEGIN, &abegin, -1 );
	gtk_tree_model_get( tmodel, b, DOSSIER_COL_DOSNAME, &bdname, DOSSIER_COL_BEGIN, &bbegin, -1 );

	cmp = g_utf8_collate( adname, bdname );
	if( cmp == 0 ){
		if( !my_strlen( abegin )){
			cmp = +1;
		} else if( !my_strlen( bbegin )){
			cmp = -1;
		} else {
			cmp = -1 * g_utf8_collate( abegin, bbegin );
		}
	}

	g_free( adname );
	g_free( abegin );
	g_free( bdname );
	g_free( bbegin );

	return( cmp );
}

static void
on_file_dir_changed( ofaPortfolioCollection *dir, guint count, const gchar *filename, ofaDossierStore *store )
{
	gtk_list_store_clear( GTK_LIST_STORE( store ));
	load_dataset( store, dir );
}

/*
 * load the dataset
 */
static void
load_dataset( ofaDossierStore *store, ofaPortfolioCollection *dir )
{
	GList *dossier_list, *itd;
	ofaIDBMeta *meta;
	GList *period_list, *itp;
	ofaIDBPeriod *period;

	dossier_list = ofa_portfolio_collection_get_dossiers( dir );

	for( itd=dossier_list ; itd ; itd=itd->next ){
		meta = ( ofaIDBMeta * ) itd->data;
		g_return_if_fail( meta && OFA_IS_IDBMETA( meta ));

		period_list = ofa_idbmeta_get_periods( meta );
		for( itp=period_list ; itp ; itp=itp->next ){
			period = ( ofaIDBPeriod * ) itp->data;
			g_return_if_fail( period && OFA_IS_IDBPERIOD( period ));

			insert_row( store, meta, period );
		}
		ofa_idbmeta_free_periods( period_list );
	}

	ofa_portfolio_collection_free_dossiers( dossier_list );
}

static void
insert_row( ofaDossierStore *store, const ofaIDBMeta *meta, const ofaIDBPeriod *period )
{
	static const gchar *thisfn = "ofa_dossier_store_insert_row";
	GtkTreeIter iter;

	g_debug( "%s: store=%p, meta=%p, period=%p",
			thisfn, ( void * ) store, ( void * ) meta, ( void * ) period );

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, meta, period, &iter );
}

static void
set_row( ofaDossierStore *store, const ofaIDBMeta *meta, const ofaIDBPeriod *period, GtkTreeIter *iter )
{
	gchar *dosname, *begin, *end, *status, *pername, *provname;
	ofaIDBProvider *provider;

	dosname = ofa_idbmeta_get_dossier_name( meta );
	provider = ofa_idbmeta_get_provider( meta );
	provname = ofa_idbprovider_get_canon_name( provider );
	g_object_unref( provider );

	begin = my_date_to_str( ofa_idbperiod_get_begin_date( period ), ofa_prefs_date_display());
	end = my_date_to_str( ofa_idbperiod_get_end_date( period ), ofa_prefs_date_display());
	status = ofa_idbperiod_get_status( period );
	pername = ofa_idbperiod_get_name( period );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			DOSSIER_COL_DOSNAME,  dosname,
			DOSSIER_COL_PROVNAME, provname,
			DOSSIER_COL_PERNAME,  pername,
			DOSSIER_COL_BEGIN,    begin,
			DOSSIER_COL_END,      end,
			DOSSIER_COL_STATUS,   status,
			DOSSIER_COL_CURRENT,  ofa_idbperiod_get_current( period ),
			DOSSIER_COL_META,     meta,
			DOSSIER_COL_PERIOD,   period,
			-1 );

	g_free( provname );
	g_free( pername );
	g_free( begin );
	g_free( end );
	g_free( dosname );
	g_free( status );
}
