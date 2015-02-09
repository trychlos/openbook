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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-store.h"

/* private instance data
 */
struct _ofaBatStorePrivate {
	gboolean    dispose_has_run;

	/*
	 */
};

static GType st_col_types[BAT_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* id, uri, format */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* begin, end, rib */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN,	/* currency, end_solde, end_solde_set */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* notes, count, upd_user */
		G_TYPE_STRING,									/* upd_stamp */
		G_TYPE_OBJECT									/* the #ofoBat itself */
};

/* the key which is attached to the dossier in order to identify this
 * store
 */
#define STORE_DATA_DOSSIER                   "ofa-bat-store"

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatStore *store );
static void     load_dataset( ofaBatStore *store, ofoDossier *dossier );
static void     insert_row( ofaBatStore *store, ofoDossier *dossier, const ofoBat *bat );
static void     set_row( ofaBatStore *store, ofoDossier *dossier, const ofoBat *bat, GtkTreeIter *iter );
static void     setup_signaling_connect( ofaBatStore *store, ofoDossier *dossier );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaBatStore *store );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaBatStore *store );

G_DEFINE_TYPE( ofaBatStore, ofa_bat_store, OFA_TYPE_LIST_STORE )

static void
bat_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_store_parent_class )->finalize( instance );
}

static void
bat_store_dispose( GObject *instance )
{
	ofaBatStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_STORE( instance ));

	priv = OFA_BAT_STORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_store_parent_class )->dispose( instance );
}

static void
ofa_bat_store_init( ofaBatStore *self )
{
	static const gchar *thisfn = "ofa_bat_store_init";

	g_return_if_fail( OFA_IS_BAT_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_STORE, ofaBatStorePrivate );
}

static void
ofa_bat_store_class_init( ofaBatStoreClass *klass )
{
	static const gchar *thisfn = "ofa_bat_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_store_finalize;

	g_type_class_add_private( klass, sizeof( ofaBatStorePrivate ));
}

/**
 * ofa_bat_store_new:
 * @dossier: the currently opened #ofoDossier.
 *
 * Instanciates a new #ofaBatStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaBatStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaBatStore *
ofa_bat_store_new( ofoDossier *dossier )
{
	ofaBatStore *store;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	store = ( ofaBatStore * ) g_object_get_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER );

	if( store ){
		g_return_val_if_fail( OFA_IS_BAT_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_BAT_STORE,
						OFA_PROP_DOSSIER, dossier,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), BAT_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_object_set_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER, store );

		load_dataset( store, dossier );
		setup_signaling_connect( store, dossier );
	}

	return( store );
}

/*
 * sorting the store per identifier
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatStore *store )
{
	gchar *aid, *bid;
	gint cmp;

	gtk_tree_model_get( tmodel, a, BAT_COL_ID, &aid, -1 );
	gtk_tree_model_get( tmodel, b, BAT_COL_ID, &bid, -1 );

	cmp = g_utf8_collate( aid, bid );

	g_free( aid );
	g_free( bid );

	return( cmp );
}

static void
load_dataset( ofaBatStore *store, ofoDossier *dossier )
{
	const GList *dataset, *it;
	ofoBat *bat;

	dataset = ofo_bat_get_dataset( dossier );

	for( it=dataset ; it ; it=it->next ){
		bat = OFO_BAT( it->data );
		insert_row( store, dossier, bat );
	}
}

static void
insert_row( ofaBatStore *store, ofoDossier *dossier, const ofoBat *bat )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, dossier, bat, &iter );
}

static void
set_row( ofaBatStore *store, ofoDossier *dossier, const ofoBat *bat, GtkTreeIter *iter )
{
	gchar *sid, *sbegin, *send, *sendsolde, *scount, *stamp;
	const GDate *date;
	const gchar *cscurrency;

	sid = g_strdup_printf( "%lu", ofo_bat_get_id( bat ));
	date = ofo_bat_get_begin( bat );
	if( my_date_is_valid( date )){
		sbegin = my_date_to_str( date, MY_DATE_DMYY );
	} else {
		sbegin = g_strdup( "" );
	}
	date = ofo_bat_get_end( bat );
	if( my_date_is_valid( date )){
		send = my_date_to_str( date, MY_DATE_DMYY );
	} else {
		send = g_strdup( "" );
	}
	if( ofo_bat_get_solde_set( bat )){
		sendsolde = my_double_to_str( ofo_bat_get_solde( bat ));
	} else {
		sendsolde = g_strdup( "" );
	}
	cscurrency = ofo_bat_get_currency( bat );
	if( !cscurrency ){
		cscurrency = "";
	}
	scount = g_strdup_printf( "%u", ofo_bat_get_count( bat, dossier ));
	stamp  = my_utils_stamp_to_str( ofo_bat_get_upd_stamp( bat ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			BAT_COL_ID,            sid,
			BAT_COL_URI,           ofo_bat_get_uri( bat ),
			BAT_COL_FORMAT,        ofo_bat_get_format( bat ),
			BAT_COL_BEGIN,         sbegin,
			BAT_COL_END,           send,
			BAT_COL_RIB,           ofo_bat_get_rib( bat ),
			BAT_COL_CURRENCY,      cscurrency,
			BAT_COL_END_SOLDE,     sendsolde,
			BAT_COL_END_SOLDE_SET, ofo_bat_get_solde_set( bat ),
			BAT_COL_NOTES,         ofo_bat_get_notes( bat ),
			BAT_COL_COUNT,         scount,
			BAT_COL_UPD_USER,      ofo_bat_get_upd_user( bat ),
			BAT_COL_UPD_STAMP,     stamp,
			BAT_COL_OBJECT,        bat,
			-1 );

	g_free( stamp );
	g_free( scount );
	g_free( sendsolde );
	g_free( send );
	g_free( sbegin );
	g_free( sid );
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this store is equal to those of the dossier
 */
static void
setup_signaling_connect( ofaBatStore *store, ofoDossier *dossier )
{
	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), store );

	/* a BAT file is never updated, nor deleted */

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), store );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaBatStore *store )
{
	static const gchar *thisfn = "ofa_bat_store_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_BAT( object )){
		insert_row( store, dossier, OFO_BAT( object ));
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaBatStore *store )
{
	static const gchar *thisfn = "ofa_bat_store_on_reload_dataset";

	g_debug( "%s: dossier=%p, type=%lu, store=%p",
			thisfn, ( void * ) dossier, type, ( void * ) store );

	if( type == OFO_TYPE_BAT ){
		gtk_list_store_clear( GTK_LIST_STORE( store ));
		load_dataset( store, dossier );
	}
}
