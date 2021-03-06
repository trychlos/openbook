/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-dossier-collection.h"
#include "api/ofa-dossier-store.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-dossier.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
}
	ofaDossierStorePrivate;

static GType st_col_types[DOSSIER_N_COLUMNS] = {
		G_TYPE_STRING, 					/* dossier name */
		G_TYPE_STRING, 					/* DBMS provider name */
		G_TYPE_STRING,					/* period name */
		G_TYPE_STRING,					/* localized exercice label */
		G_TYPE_STRING, 					/* end date (user display) */
		G_TYPE_STRING, 					/* begin date (user display) */
		G_TYPE_STRING, 					/* localized status */
		G_TYPE_BOOLEAN, 				/* is_current */
		G_TYPE_OBJECT,					/* ofaIDBDossierMeta */
		G_TYPE_OBJECT					/* ofaIDBExerciceMeta */
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ] = { 0 };

static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *self );
static void on_dossier_collection_changed( ofaDossierCollection *collection, guint count, ofaDossierStore *self );
static void load_dataset( ofaDossierStore *self, ofaDossierCollection *collection );
static void insert_row( ofaDossierStore *self, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period );
static void set_row( ofaDossierStore *self, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period, GtkTreeIter *iter );

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
 * @getter: a #ofaIGetter instance.
 *
 * The #ofaDossierStore class implements a singleton. Each returned
 * pointer is a new reference to the same instance of the class.
 * This unique instance is allocated on demand, when the
 * #ofa_dossier_store_new() method is called for the first time.
 *
 * The unique #ofaDossierStore instance is *not* managed by the #myICollector
 * interface, because this later releses all its resources when a dossier is
 * closed.
 * Instead, the instance is first instanciated by the #ofaHub, and then made
 * accessible through the #ofaIGetter interface.
 *
 * Returns: a new reference on the #ofaDossierStore instance, which
 * must be g_object_unref() by the caller.
 */
ofaDossierStore *
ofa_dossier_store_new( ofaIGetter *getter )
{
	ofaDossierStore *store;
	ofaDossierStorePrivate *priv;
	ofaDossierCollection *collection;

	store = ofa_igetter_get_dossier_store( getter );
	if( store ){
		store = g_object_ref( store );

	} else {
		g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

		store = g_object_new( OFA_TYPE_DOSSIER_STORE, NULL );

		priv = ofa_dossier_store_get_instance_private( store );

		priv->getter = getter;

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), DOSSIER_N_COLUMNS, st_col_types );

		collection = ofa_igetter_get_dossier_collection( getter );
		load_dataset( store, collection );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );

		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_signal_connect( collection, "changed", G_CALLBACK( on_dossier_collection_changed ), store );
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
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *self )
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
on_dossier_collection_changed( ofaDossierCollection *collection, guint count, ofaDossierStore *self )
{
	static const gchar *thisfn = "ofa_dossier_store_on_dossier_collection_changed";

	g_debug( "%s: collection=%p, count=%u, self=%p",
			thisfn, ( void * ) collection, count, ( void * ) self );

	gtk_list_store_clear( GTK_LIST_STORE( self ));
	load_dataset( self, collection );

	/* dump the dossier collection after having refreshed the store so
	 * that the ref_counts are up to date */
	ofa_dossier_collection_dump( collection );
}

/*
 * load the dataset
 */
static void
load_dataset( ofaDossierStore *self, ofaDossierCollection *collection )
{
	GList *dossier_list, *itd;
	ofaIDBDossierMeta *dossier_meta;
	const GList *period_list, *itp;
	ofaIDBExerciceMeta *period;

	dossier_list = ofa_dossier_collection_get_list( collection );

	for( itd=dossier_list ; itd ; itd=itd->next ){
		dossier_meta = ( ofaIDBDossierMeta * ) itd->data;
		//g_debug( "ofa_dossier_store_load_dataset: dossier_meta=%p", dossier_meta );
		g_return_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ));

		period_list = ofa_idbdossier_meta_get_periods( dossier_meta );
		for( itp=period_list ; itp ; itp=itp->next ){
			period = ( ofaIDBExerciceMeta * ) itp->data;
			g_return_if_fail( period && OFA_IS_IDBEXERCICE_META( period ));

			insert_row( self, dossier_meta, period );
		}
	}
}

static void
insert_row( ofaDossierStore *self, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_dossier_store_insert_row";
	GtkTreeIter iter;

	g_debug( "%s: self=%p, dossier_meta=%p, period=%p",
			thisfn, ( void * ) self, ( void * ) dossier_meta, ( void * ) period );

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row( self, dossier_meta, period, &iter );
}

static void
set_row( ofaDossierStore *self, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period, GtkTreeIter *iter )
{
	ofaDossierStorePrivate *priv;
	gchar *begin, *end, *status, *pername, *provname, *exelabel;
	ofaIDBProvider *provider;
	const gchar *dosname;

	priv = ofa_dossier_store_get_instance_private( self );

	dosname = ofa_idbdossier_meta_get_dossier_name( dossier_meta );
	provider = ofa_idbdossier_meta_get_provider( dossier_meta );
	provname = ofa_idbprovider_get_canon_name( provider );

	begin = my_date_to_str( ofa_idbexercice_meta_get_begin_date( period ), ofa_prefs_date_get_display_format( priv->getter ));
	end = my_date_to_str( ofa_idbexercice_meta_get_end_date( period ), ofa_prefs_date_get_display_format( priv->getter ));
	status = ofa_idbexercice_meta_get_status( period );
	pername = ofa_idbexercice_meta_get_name( period );
	exelabel = ofa_idbexercice_meta_get_label( period );

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			DOSSIER_COL_DOSNAME,  dosname,
			DOSSIER_COL_PROVNAME, provname,
			DOSSIER_COL_PERNAME,  pername,
			DOSSIER_COL_EXELABEL, exelabel,
			DOSSIER_COL_BEGIN,    begin,
			DOSSIER_COL_END,      end,
			DOSSIER_COL_STATUS,   status,
			DOSSIER_COL_CURRENT,  ofa_idbexercice_meta_get_current( period ),
			DOSSIER_COL_DOS_META, dossier_meta,
			DOSSIER_COL_EXE_META, period,
			-1 );

	g_free( exelabel );
	g_free( provname );
	g_free( pername );
	g_free( begin );
	g_free( end );
	g_free( status );
}
