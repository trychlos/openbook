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

#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
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
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN,	/* currency, begin_solde, begin_solde_set */
		G_TYPE_STRING, G_TYPE_BOOLEAN,					/* end_solde, end_solde_set */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* notes, count, unused */
		G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoBat itself */
};

/* the key which is attached to the dossier in order to identify this
 * store
 */
#define STORE_DATA_DOSSIER                   "ofa-bat-store"

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatStore *store );
static void     load_dataset( ofaBatStore *store, ofaHub *hub );
static void     insert_row( ofaBatStore *store, ofaHub *hub, const ofoBat *bat );
static void     set_row_by_store_iter( ofaBatStore *store, GtkTreeIter *iter, const ofoBat *bat );
static gboolean find_bat_by_id( ofaBatStore *store, ofxCounter id, GtkTreeIter *iter );
static void     connect_to_hub_signaling_system( ofaBatStore *store, ofaHub *hub );
static void     on_hub_new_object( ofaHub *hub, ofoBase *object, ofaBatStore *store );
static void     on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaBatStore *store );
static void     on_updated_bat( ofaBatStore *store, ofoBat *bat );
static void     on_updated_concil( ofaBatStore *store, ofaHub *hub, ofoConcil *concil );
static void     on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaBatStore *store );
static void     on_deleted_concil( ofaBatStore *store, ofaHub *hub, ofoConcil *concil );
static void     concil_enumerate_cb( ofoConcil *concil, const gchar *type, ofxCounter id, ofaBatStore *store );
static void     on_hub_reload_dataset( ofaHub *hub, GType type, ofaBatStore *store );

G_DEFINE_TYPE_EXTENDED( ofaBatStore, ofa_bat_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaBatStore ))

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

	priv = ofa_bat_store_get_instance_private( OFA_BAT_STORE( instance ));

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
}

static void
ofa_bat_store_class_init( ofaBatStoreClass *klass )
{
	static const gchar *thisfn = "ofa_bat_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_store_finalize;
}

/**
 * ofa_bat_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaBatStore and attached it to the @hub
 * if not already done. Else get the already allocated #ofaBatStore
 * from the @hub.
 *
 * A weak notify reference is put on this same @hub, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaBatStore *
ofa_bat_store_new( ofaHub *hub )
{
	ofaBatStore *store;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = ( ofaBatStore * ) g_object_get_data( G_OBJECT( hub ), STORE_DATA_DOSSIER );

	if( store ){
		g_return_val_if_fail( OFA_IS_BAT_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_BAT_STORE,
						OFA_PROP_HUB,       hub,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), BAT_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_object_set_data( G_OBJECT( hub ), STORE_DATA_DOSSIER, store );

		load_dataset( store, hub );
		connect_to_hub_signaling_system( store, hub );
	}

	return( store );
}

/*
 * sorting the store per descending identifier to get the most recent
 * in the top of the list
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatStore *store )
{
	gchar *asid, *bsid;
	ofxCounter aid, bid;
	gint cmp;

	gtk_tree_model_get( tmodel, a, BAT_COL_ID, &asid, -1 );
	aid = atol( asid );
	gtk_tree_model_get( tmodel, b, BAT_COL_ID, &bsid, -1 );
	bid = atol( bsid );

	cmp = ( aid < bid ? -1 : ( aid > bid ? 1 : 0 ));

	g_free( asid );
	g_free( bsid );

	return( -cmp );
}

static void
load_dataset( ofaBatStore *store, ofaHub *hub )
{
	const GList *dataset, *it;
	ofoBat *bat;

	dataset = ofo_bat_get_dataset( hub );

	for( it=dataset ; it ; it=it->next ){
		bat = OFO_BAT( it->data );
		insert_row( store, hub, bat );
	}
}

static void
insert_row( ofaBatStore *store, ofaHub *hub, const ofoBat *bat )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row_by_store_iter( store, &iter, bat );
}

static void
set_row_by_store_iter( ofaBatStore *store, GtkTreeIter *iter, const ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_store_set_row_by_store_iter";
	gchar *sid, *sbegin, *send, *sbeginsolde, *sendsolde, *scount, *stamp, *sunused;
	const GDate *date;
	const gchar *cscurrency;
	gint count, used;

	sid = g_strdup_printf( "%lu", ofo_bat_get_id( bat ));
	date = ofo_bat_get_begin_date( bat );
	if( my_date_is_valid( date )){
		sbegin = my_date_to_str( date, ofa_prefs_date_display());
	} else {
		sbegin = g_strdup( "" );
	}
	date = ofo_bat_get_end_date( bat );
	if( my_date_is_valid( date )){
		send = my_date_to_str( date, ofa_prefs_date_display());
	} else {
		send = g_strdup( "" );
	}
	if( ofo_bat_get_begin_solde_set( bat )){
		sbeginsolde = my_double_to_str( ofo_bat_get_begin_solde( bat ));
	} else {
		sbeginsolde = g_strdup( "" );
	}
	if( ofo_bat_get_end_solde_set( bat )){
		sendsolde = my_double_to_str( ofo_bat_get_end_solde( bat ));
	} else {
		sendsolde = g_strdup( "" );
	}
	cscurrency = ofo_bat_get_currency( bat );
	if( !cscurrency ){
		cscurrency = "";
	}
	count = ofo_bat_get_lines_count( bat );
	scount = g_strdup_printf( "%u", count );
	used = ofo_bat_get_used_count( bat );
	sunused = g_strdup_printf( "%u", count-used );
	stamp  = my_utils_stamp_to_str( ofo_bat_get_upd_stamp( bat ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			BAT_COL_ID,              sid,
			BAT_COL_URI,             ofo_bat_get_uri( bat ),
			BAT_COL_FORMAT,          ofo_bat_get_format( bat ),
			BAT_COL_BEGIN,           sbegin,
			BAT_COL_END,             send,
			BAT_COL_RIB,             ofo_bat_get_rib( bat ),
			BAT_COL_CURRENCY,        cscurrency,
			BAT_COL_BEGIN_SOLDE,     sbeginsolde,
			BAT_COL_BEGIN_SOLDE_SET, ofo_bat_get_begin_solde_set( bat ),
			BAT_COL_END_SOLDE,       sendsolde,
			BAT_COL_END_SOLDE_SET,   ofo_bat_get_end_solde_set( bat ),
			BAT_COL_NOTES,           ofo_bat_get_notes( bat ),
			BAT_COL_COUNT,           scount,
			BAT_COL_UNUSED,          sunused,
			BAT_COL_UPD_USER,        ofo_bat_get_upd_user( bat ),
			BAT_COL_UPD_STAMP,       stamp,
			BAT_COL_OBJECT,          bat,
			-1 );

	g_debug( "%s: count=%s, unused=%s", thisfn, scount, sunused );

	g_free( stamp );
	g_free( scount );
	g_free( sunused );
	g_free( sbeginsolde );
	g_free( sendsolde );
	g_free( send );
	g_free( sbegin );
	g_free( sid );
}

/*
 * setup store iter
 */
static gboolean
find_bat_by_id( ofaBatStore *store, ofxCounter id, GtkTreeIter *iter )
{
	gchar *sid;
	ofxCounter row_id;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, BAT_COL_ID, &sid, -1 );
			row_id = atol( sid );
			g_free( sid );
			if( row_id == id ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this store is equal to those of the dossier
 */
static void
connect_to_hub_signaling_system( ofaBatStore *store, ofaHub *hub )
{
	g_signal_connect( hub, SIGNAL_HUB_NEW, G_CALLBACK( on_hub_new_object ), store );
	g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), store );
	g_signal_connect( hub, SIGNAL_HUB_DELETED, G_CALLBACK( on_hub_deleted_object ), store );
	g_signal_connect( hub, SIGNAL_HUB_RELOAD, G_CALLBACK( on_hub_reload_dataset ), store );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
on_hub_new_object( ofaHub *hub, ofoBase *object, ofaBatStore *store )
{
	static const gchar *thisfn = "ofa_bat_store_on_hub_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_BAT( object )){
		insert_row( store, hub, OFO_BAT( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaBatStore *store )
{
	static const gchar *thisfn = "ofa_bat_store_on_hub_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( OFO_IS_BAT( object )){
		on_updated_bat( store, OFO_BAT( object ));

	} else if( OFO_IS_CONCIL( object )){
		on_updated_concil( store, hub, OFO_CONCIL( object ));
	}
}

static void
on_updated_bat( ofaBatStore *store, ofoBat *bat )
{
	GtkTreeIter iter;

	if( find_bat_by_id( store, ofo_bat_get_id( bat ), &iter )){
		set_row_by_store_iter( store, &iter, bat );
	}
}

/*
 * a conciliation group has been updated
 * most of the time, this means that a bat line or an entry has been
 *  added to the group - So update the counts for each batline of the
 *  group
 */
static void
on_updated_concil( ofaBatStore *store, ofaHub *hub, ofoConcil *concil )
{
	ofo_concil_for_each_member( concil, ( ofoConcilEnumerate ) concil_enumerate_cb, store );
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
on_hub_deleted_object( ofaHub *hub, ofoBase *object, ofaBatStore *store )
{
	static const gchar *thisfn = "ofa_bat_store_on_hub_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_BAT( object )){
		if( find_bat_by_id( store, ofo_bat_get_id( OFO_BAT( object )), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( store ), &iter );
		}
	} else if( OFO_IS_CONCIL( object )){
		on_deleted_concil( store, hub, OFO_CONCIL( object ));
	}
}

static void
on_deleted_concil( ofaBatStore *store, ofaHub *hub, ofoConcil *concil )
{
	ofo_concil_for_each_member( concil, ( ofoConcilEnumerate ) concil_enumerate_cb, store );
}

static void
concil_enumerate_cb( ofoConcil *concil, const gchar *type, ofxCounter id, ofaBatStore *store )
{
	ofxCounter bat_id;
	ofaHub *hub;
	GtkTreeIter iter;
	ofoBat *bat;

	if( !g_utf8_collate( type, CONCIL_TYPE_BAT )){
		hub = ofa_list_store_get_hub( OFA_LIST_STORE( store ));
		bat_id = ofo_bat_line_get_bat_id_from_bat_line_id( hub, id );
		if( find_bat_by_id( store, bat_id, &iter )){
			bat = ofo_bat_get_by_id( hub, bat_id );
			set_row_by_store_iter( store, &iter, bat );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
on_hub_reload_dataset( ofaHub *hub, GType type, ofaBatStore *store )
{
	static const gchar *thisfn = "ofa_bat_store_on_hub_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, store=%p",
			thisfn, ( void * ) hub, type, ( void * ) store );

	if( type == OFO_TYPE_BAT ){
		gtk_list_store_clear( GTK_LIST_STORE( store ));
		load_dataset( store, hub );
	}
}
