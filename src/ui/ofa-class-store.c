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

#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-class-store.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaHub     *hub;
	GList      *hub_handlers;
}
	ofaClassStorePrivate;

static GType st_col_types[CLASS_N_COLUMNS] = {
	G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING,		/* num_id, string_id, label */
	G_TYPE_STRING, 0,								/* notes, notes_png */
	G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
	G_TYPE_OBJECT									/* the #ofoClass object */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/core/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/core/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassStore *self );
static void     load_dataset( ofaClassStore *self );
static void     insert_row( ofaClassStore *self, ofoClass *class );
static void     set_row_by_iter( ofaClassStore *self, GtkTreeIter *iter, ofoClass *class );
static gboolean find_row_by_id( ofaClassStore *self, gint id, GtkTreeIter *iter );
static void     hub_connect_to_signaling_system( ofaClassStore *self );
static void     hub_on_new_object( ofaHub *hub, ofoBase *object, ofaClassStore *self );
static void     hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaClassStore *self );
static void     hub_on_updated_class( ofaClassStore *self, ofaHub *hub, const gchar *prev_id, ofoClass *class );
static void     hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaClassStore *self );
static void     hub_on_reload_dataset( ofaHub *hub, GType type, ofaClassStore *self );

G_DEFINE_TYPE_EXTENDED( ofaClassStore, ofa_class_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaClassStore ))

static void
class_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_class_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLASS_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_store_parent_class )->finalize( instance );
}

static void
class_store_dispose( GObject *instance )
{
	ofaClassStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLASS_STORE( instance ));

	priv = ofa_class_store_get_instance_private( OFA_CLASS_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_store_parent_class )->dispose( instance );
}

static void
ofa_class_store_init( ofaClassStore *self )
{
	static const gchar *thisfn = "ofa_class_store_init";
	ofaClassStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CLASS_STORE( self ));

	priv = ofa_class_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->hub_handlers = NULL;
}

static void
ofa_class_store_class_init( ofaClassStoreClass *klass )
{
	static const gchar *thisfn = "ofa_class_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_store_finalize;
}

/**
 * ofa_class_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaClassStore and attached it to the @hub
 * if not already done. Else get the already allocated #ofaClassStore
 * from the @hub.
 *
 * A weak notify reference is put on this same @hub, so that the
 * instance will be unreffed when the @hub will be destroyed.
 *
 * Note that the #myICollector associated to the @hub maintains its own
 * reference to the #ofaClassStore object, reference which will be
 * freed on @hub finalization.
 *
 * Returns: a new reference to the #ofaClassStore object.
 */
ofaClassStore *
ofa_class_store_new( ofaHub *hub )
{
	ofaClassStore *store;
	ofaClassStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	collector = ofa_hub_get_collector( hub );
	store = ( ofaClassStore * ) my_icollector_single_get_object( collector, OFA_TYPE_CLASS_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_CLASS_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_CLASS_STORE, NULL );

		priv = ofa_class_store_get_instance_private( store );
		priv->hub = hub;

		st_col_types[CLASS_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), CLASS_N_COLUMNS, st_col_types );

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
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaClassStore *self )
{
	gint cmp, ida, idb;

	gtk_tree_model_get( tmodel, a, CLASS_COL_ID, &ida, -1 );
	gtk_tree_model_get( tmodel, b, CLASS_COL_ID, &idb, -1 );

	cmp = ( ida < idb ? -1 : ( ida > idb ? 1 : 0 ));

	return( cmp );
}

static void
load_dataset( ofaClassStore *self )
{
	ofaClassStorePrivate *priv;
	const GList *dataset, *it;
	ofoClass *class;

	priv = ofa_class_store_get_instance_private( self );

	dataset = ofo_class_get_dataset( priv->hub );

	for( it=dataset ; it ; it=it->next ){
		class = OFO_CLASS( it->data );
		insert_row( self, class );
	}
}

static void
insert_row( ofaClassStore *self, ofoClass *class )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, &iter, class );
}

static void
set_row_by_iter( ofaClassStore *self, GtkTreeIter *iter, ofoClass *class )
{
	static const gchar *thisfn = "ofa_class_store_set_row_by_iter";
	gchar *sid, *stamp;
	const gchar *notes;
	GError *error;
	GdkPixbuf *notes_png;

	sid = g_strdup_printf( "%u", ofo_class_get_number( class ));
	stamp  = my_stamp_to_str( ofo_class_get_upd_stamp( class ), MY_STAMP_DMYYHM );

	error = NULL;
	notes = ofo_class_get_notes( class );
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			CLASS_COL_ID,              ofo_class_get_number( class ),
			CLASS_COL_NUMBER,          sid,
			CLASS_COL_LABEL,           ofo_class_get_label( class ),
			CLASS_COL_NOTES,           notes,
			CLASS_COL_NOTES_PNG,       notes_png,
			CLASS_COL_UPD_USER,        ofo_class_get_upd_user( class ),
			CLASS_COL_UPD_STAMP,       stamp,
			CLASS_COL_OBJECT,          class,
			-1 );

	g_object_unref( notes_png );
	g_free( stamp );
	g_free( sid );
}

/*
 * setup store iter
 */
static gboolean
find_row_by_id( ofaClassStore *self, gint id, GtkTreeIter *iter )
{
	gint row_id;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, CLASS_COL_ID, &row_id, -1 );
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
hub_connect_to_signaling_system( ofaClassStore *self )
{
	ofaClassStorePrivate *priv;
	gulong handler;

	priv = ofa_class_store_get_instance_private( self );

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
hub_on_new_object( ofaHub *hub, ofoBase *object, ofaClassStore *self )
{
	static const gchar *thisfn = "ofa_class_store_hub_on_new_object";

	g_debug( "%s: hub=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CLASS( object )){
		insert_row( self, OFO_CLASS( object ));
	}
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
hub_on_updated_object( ofaHub *hub, ofoBase *object, const gchar *prev_id, ofaClassStore *self )
{
	static const gchar *thisfn = "ofa_class_store_hub_on_updated_object";

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_CLASS( object )){
		hub_on_updated_class( self, hub, prev_id, OFO_CLASS( object ));
	}
}

static void
hub_on_updated_class( ofaClassStore *self, ofaHub *hub, const gchar *prev_id, ofoClass *class )
{
	GtkTreeIter iter;
	gint prev_num, class_num;

	prev_num = atoi( prev_id );
	class_num = ofo_class_get_number( class );

	if( find_row_by_id( self, prev_num, &iter )){
		if( prev_num != class_num ){
			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
			insert_row( self, class );

		} else if( find_row_by_id( self, class_num, &iter )){
			set_row_by_iter( self, &iter, class );
		}
	}
}

/*
 * SIGNAL_HUB_DELETED signal handler
 */
static void
hub_on_deleted_object( ofaHub *hub, ofoBase *object, ofaClassStore *self )
{
	static const gchar *thisfn = "ofa_class_store_hub_on_deleted_object";
	GtkTreeIter iter;

	g_debug( "%s: hub=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_CLASS( object )){
		if( find_row_by_id( self, ofo_class_get_number( OFO_CLASS( object )), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNAL_HUB_RELOAD signal handler
 */
static void
hub_on_reload_dataset( ofaHub *hub, GType type, ofaClassStore *self )
{
	static const gchar *thisfn = "ofa_class_store_hub_on_reload_dataset";

	g_debug( "%s: hub=%p, type=%lu, self=%p",
			thisfn, ( void * ) hub, type, ( void * ) self );

	if( type == OFO_TYPE_CLASS ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
