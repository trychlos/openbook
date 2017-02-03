/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-counter.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-store.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	ofaHub     *hub;
	GList      *hub_handlers;
}
	ofaBatStorePrivate;

static GType st_col_types[BAT_N_COLUMNS] = {
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* id, uri, format */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* begin, end, rib */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN,	/* currency, begin_solde, begin_solde_set */
	G_TYPE_STRING, G_TYPE_BOOLEAN,					/* end_solde, end_solde_set */
	G_TYPE_STRING, 0,								/* notes, notes_png */
	G_TYPE_STRING, G_TYPE_STRING,					/* count, unused */
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* acccount, upd_user, upd_stamp */
	G_TYPE_OBJECT									/* the #ofoBat itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatStore *self );
static void     load_dataset( ofaBatStore *self );
static void     insert_row( ofaBatStore *self, ofoBat *bat );
static void     set_row_by_iter( ofaBatStore *self, GtkTreeIter *iter, ofoBat *bat );
static gboolean find_bat_by_id( ofaBatStore *self, ofxCounter id, GtkTreeIter *iter );
static void     hub_connect_to_signaling_system( ofaBatStore *self );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaBatStore *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaBatStore *self );
static void     hub_on_updated_bat( ofaBatStore *self, ofoBat *bat );
static void     hub_on_updated_concil( ofaBatStore *self, ofaHub *hub, ofoConcil *concil );
static void     hub_on_updated_account( ofaBatStore *self, const gchar *prev_id, const gchar *new_id );
static void     hub_on_updated_currency( ofaBatStore *self, const gchar *prev_id, const gchar *new_id );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaBatStore *self );
static void     hub_on_deleted_concil( ofaBatStore *self, ofaHub *hub, ofoConcil *concil );
static void     hub_on_deleted_concil_enumerate_cb( ofoConcil *concil, const gchar *type, ofxCounter id, ofaBatStore *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaBatStore *self );

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
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_store_parent_class )->dispose( instance );
}

static void
ofa_bat_store_init( ofaBatStore *self )
{
	static const gchar *thisfn = "ofa_bat_store_init";
	ofaBatStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_STORE( self ));

	priv = ofa_bat_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
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
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaBatStore and attached it to the @hub
 * if not already done. Else get the already allocated #ofaBatStore
 * from the @hub.
 *
 * A weak notify reference is put on this same @hub, so that the
 * instance will be unreffed when the @hub will be destroyed.
 *
 * Note that the #myICollector associated to the @hub maintains its own
 * reference to the #ofaBatStore object, reference which will be
 * released on @hub finalization.
 *
 * Returns: a new reference to the #ofaBatStore object, which should be
 * #g_object_unref() by the caller after use.
 */
ofaBatStore *
ofa_bat_store_new( ofaIGetter *getter )
{
	ofaBatStore *store;
	ofaBatStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaBatStore * ) my_icollector_single_get_object( collector, OFA_TYPE_BAT_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_BAT_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_BAT_STORE, NULL );

		priv = ofa_bat_store_get_instance_private( store );

		priv->getter = getter;
		priv->hub = ofa_igetter_get_hub( getter );

		st_col_types[BAT_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), BAT_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		hub_connect_to_signaling_system( store );
		load_dataset( store );
	}

	return( g_object_ref( store ));
}

/*
 * sorting the self per descending identifier to get the most recent
 * in the top of the list
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaBatStore *self )
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

	return( cmp );
}

static void
load_dataset( ofaBatStore *self )
{
	ofaBatStorePrivate *priv;
	const GList *dataset, *it;
	ofoBat *bat;

	priv = ofa_bat_store_get_instance_private( self );

	dataset = ofo_bat_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		bat = OFO_BAT( it->data );
		insert_row( self, bat );
	}
}

static void
insert_row( ofaBatStore *self, ofoBat *bat )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, &iter, bat );
}

static void
set_row_by_iter( ofaBatStore *self, GtkTreeIter *iter, ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_store_set_row";
	ofaBatStorePrivate *priv;
	gchar *sid, *sbegin, *send, *sbeginsolde, *sendsolde, *scount, *stamp, *sunused;
	const GDate *date;
	const gchar *cscurrency, *caccount, *notes;
	gint count, used;
	ofoCurrency *cur_obj;
	GError *error;
	GdkPixbuf *notes_png;

	priv = ofa_bat_store_get_instance_private( self );

	sid = ofa_counter_to_str( ofo_bat_get_id( bat ), priv->getter );
	date = ofo_bat_get_begin_date( bat );
	if( my_date_is_valid( date )){
		sbegin = my_date_to_str( date, ofa_prefs_date_display( priv->getter ));
	} else {
		sbegin = g_strdup( "" );
	}
	date = ofo_bat_get_end_date( bat );
	if( my_date_is_valid( date )){
		send = my_date_to_str( date, ofa_prefs_date_display( priv->getter ));
	} else {
		send = g_strdup( "" );
	}
	cscurrency = ofo_bat_get_currency( bat );
	if( !cscurrency ){
		cscurrency = "";
	}
	cur_obj = my_strlen( cscurrency ) ? ofo_currency_get_by_code( priv->getter, cscurrency ) : NULL;
	g_return_if_fail( !cur_obj || OFO_IS_CURRENCY( cur_obj ));
	if( ofo_bat_get_begin_solde_set( bat )){
		sbeginsolde = ofa_amount_to_str( ofo_bat_get_begin_solde( bat ), cur_obj, priv->getter );
	} else {
		sbeginsolde = g_strdup( "" );
	}
	if( ofo_bat_get_end_solde_set( bat )){
		sendsolde = ofa_amount_to_str( ofo_bat_get_end_solde( bat ), cur_obj, priv->getter );
	} else {
		sendsolde = g_strdup( "" );
	}
	caccount = ofo_bat_get_account( bat );
	if( !caccount ){
		caccount = "";
	}
	count = ofo_bat_get_lines_count( bat );
	scount = g_strdup_printf( "%u", count );
	used = ofo_bat_get_used_count( bat );
	sunused = g_strdup_printf( "%u", count-used );
	stamp  = my_stamp_to_str( ofo_bat_get_upd_stamp( bat ), MY_STAMP_DMYYHM );

	error = NULL;
	notes = ofo_bat_get_notes( bat );
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
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
			BAT_COL_NOTES,           notes,
			BAT_COL_NOTES_PNG,       notes_png,
			BAT_COL_COUNT,           scount,
			BAT_COL_UNUSED,          sunused,
			BAT_COL_ACCOUNT,         caccount,
			BAT_COL_UPD_USER,        ofo_bat_get_upd_user( bat ),
			BAT_COL_UPD_STAMP,       stamp,
			BAT_COL_OBJECT,          bat,
			-1 );

	g_debug( "%s: count=%s, unused=%s", thisfn, scount, sunused );

	g_object_unref( notes_png );
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
find_bat_by_id( ofaBatStore *self, ofxCounter id, GtkTreeIter *iter )
{
	gchar *sid;
	ofxCounter row_id;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, BAT_COL_ID, &sid, -1 );
			row_id = atol( sid );
			g_free( sid );
			if( row_id == id ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

/*
 * connect to the dossier signaling system
 */
static void
hub_connect_to_signaling_system( ofaBatStore *self )
{
	ofaBatStorePrivate *priv;
	gulong handler;

	priv = ofa_bat_store_get_instance_private( self );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_NEW, G_CALLBACK( hub_on_new_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_UPDATED, G_CALLBACK( hub_on_updated_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_DELETED, G_CALLBACK( hub_on_deleted_object ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect( priv->hub, SIGNAL_HUB_RELOAD, G_CALLBACK( hub_on_reload_dataset ), self );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );
}

/*
 * SIGNAL_HUB_NEW signal handler
 */
static void
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaBatStore *self )
{
	static const gchar *thisfn = "ofa_bat_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_BAT( object )){
		insert_row( self, OFO_BAT( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaBatStore *self )
{
	static const gchar *thisfn = "ofa_bat_store_hub_on_updated_object";
	const gchar *new_id;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_BAT( object )){
		hub_on_updated_bat( self, OFO_BAT( object ));

	} else if( OFO_IS_CONCIL( object )){
		hub_on_updated_concil( self, hub, OFO_CONCIL( object ));

	} else if( OFO_IS_ACCOUNT( object )){
		new_id = ofo_account_get_number( OFO_ACCOUNT( object ));
		if( prev_id && g_utf8_collate( prev_id, new_id )){
			hub_on_updated_account( self, prev_id, new_id );
		}

	} else if( OFO_IS_CURRENCY( object )){
		new_id = ofo_currency_get_code( OFO_CURRENCY( object ));
		if( prev_id && g_utf8_collate( prev_id, new_id )){
			hub_on_updated_currency( self, prev_id, new_id );
		}
	}
}

static void
hub_on_updated_bat( ofaBatStore *self, ofoBat *bat )
{
	GtkTreeIter iter;

	if( find_bat_by_id( self, ofo_bat_get_id( bat ), &iter )){
		set_row_by_iter( self, &iter, bat );
	}
}

/*
 * a conciliation group has been updated
 * most of the time, this means that a bat line or an entry has been
 *  added to the group - So update the counts for each batline of the
 *  group
 */
static void
hub_on_updated_concil( ofaBatStore *self, ofaHub *hub, ofoConcil *concil )
{
	ofo_concil_for_each_member( concil, ( ofoConcilEnumerate ) hub_on_deleted_concil_enumerate_cb, self );
}

static void
hub_on_updated_account( ofaBatStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;
	ofoBat *bat;
	gchar *row_account;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, BAT_COL_ACCOUNT, &row_account, BAT_COL_OBJECT, &bat, -1 );

			g_return_if_fail( bat && OFO_IS_BAT( bat ));
			g_object_unref( bat );

			if( row_account ){
				cmp = g_utf8_collate( row_account, prev_id );
				g_free( row_account );

				if( cmp == 0 ){
					ofo_bat_set_account( bat, new_id );
					gtk_list_store_set( GTK_LIST_STORE( self ), &iter, BAT_COL_ACCOUNT, new_id, -1 );
				}
			}

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

static void
hub_on_updated_currency( ofaBatStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;
	ofoBat *bat;
	gchar *row_currency;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, BAT_COL_CURRENCY, &row_currency, BAT_COL_OBJECT, &bat, -1 );

			g_return_if_fail( bat && OFO_IS_BAT( bat ));
			g_object_unref( bat );

			if( row_currency ){
				cmp = g_utf8_collate( row_currency, prev_id );
				g_free( row_currency );

				if( cmp == 0 ){
					ofo_bat_set_currency( bat, new_id );
					gtk_list_store_set( GTK_LIST_STORE( self ), &iter, BAT_COL_CURRENCY, new_id, -1 );
				}
			}

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaBatStore *self )
{
	static const gchar *thisfn = "ofa_bat_store_hub_on_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_BAT( object )){
		if( find_bat_by_id( self, ofo_bat_get_id( OFO_BAT( object )), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	} else if( OFO_IS_CONCIL( object )){
		hub_on_deleted_concil( self, hub, OFO_CONCIL( object ));
	}
}

static void
hub_on_deleted_concil( ofaBatStore *self, ofaHub *hub, ofoConcil *concil )
{
	ofo_concil_for_each_member( concil, ( ofoConcilEnumerate ) hub_on_deleted_concil_enumerate_cb, self );
}

static void
hub_on_deleted_concil_enumerate_cb( ofoConcil *concil, const gchar *type, ofxCounter id, ofaBatStore *self )
{
	ofaBatStorePrivate *priv;
	ofxCounter bat_id;
	GtkTreeIter iter;
	ofoBat *bat;

	priv = ofa_bat_store_get_instance_private( self );

	if( !g_utf8_collate( type, CONCIL_TYPE_BAT )){
		bat_id = ofo_bat_line_get_bat_id_from_bat_line_id( priv->getter, id );
		if( find_bat_by_id( self, bat_id, &iter )){
			bat = ofo_bat_get_by_id( priv->getter, bat_id );
			set_row_by_iter( self, &iter, bat );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaBatStore *self )
{
	static const gchar *thisfn = "ofa_bat_store_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_BAT ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
