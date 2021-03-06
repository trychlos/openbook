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

#include <glib/gi18n.h>

#include "my/my-icollector.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-isignaler.h"
#include "api/ofo-ope-template.h"

#include "tva/ofa-tva-form-store.h"
#include "tva/ofo-tva-form.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;

	/* runtime
	 */
	GList      *signaler_handlers;
}
	ofaTVAFormStorePrivate;

static GType st_col_types[TVA_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING,					/* mnemo, label */
		G_TYPE_STRING, G_TYPE_STRING,					/* cre_user, cre_stamp */
		G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING,	/* enabled, enabled_b, has_corresp. */
		G_TYPE_STRING, 0,								/* notes, notes_png */
		G_TYPE_STRING, G_TYPE_STRING,					/* upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoTVAForm itself */
};

static const gchar *st_resource_filler_png  = "/org/trychlos/openbook/vat/filler.png";
static const gchar *st_resource_notes_png   = "/org/trychlos/openbook/vat/notes.png";

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVAFormStore *self );
static void     load_dataset( ofaTVAFormStore *self );
static void     insert_row( ofaTVAFormStore *self, const ofoTVAForm *form );
static void     set_row_by_iter( ofaTVAFormStore *self, const ofoTVAForm *form, GtkTreeIter *iter );
static gboolean find_form_by_mnemo( ofaTVAFormStore *self, const gchar *code, GtkTreeIter *iter );
static void     set_ope_template_new_id( ofaTVAFormStore *self, const gchar *prev_id, const gchar *new_id );
static void     signaler_connect_to_signaling_system( ofaTVAFormStore *self );
static void     signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaTVAFormStore *self );
static void     signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaTVAFormStore *self );
static void     signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaTVAFormStore *self );
static void     signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaTVAFormStore *self );

G_DEFINE_TYPE_EXTENDED( ofaTVAFormStore, ofa_tva_form_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaTVAFormStore ))

static void
tva_form_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_form_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_FORM_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_store_parent_class )->finalize( instance );
}

static void
tva_form_store_dispose( GObject *instance )
{
	ofaTVAFormStorePrivate *priv;
	ofaISignaler *signaler;

	g_return_if_fail( instance && OFA_IS_TVA_FORM_STORE( instance ));

	priv = ofa_tva_form_store_get_instance_private( OFA_TVA_FORM_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* disconnect from ofaISignaler signaling system */
		signaler = ofa_igetter_get_signaler( priv->getter );
		ofa_isignaler_disconnect_handlers( signaler, &priv->signaler_handlers );

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_store_parent_class )->dispose( instance );
}

static void
ofa_tva_form_store_init( ofaTVAFormStore *self )
{
	static const gchar *thisfn = "ofa_tva_form_store_init";
	ofaTVAFormStorePrivate *priv;

	g_return_if_fail( OFA_IS_TVA_FORM_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tva_form_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->signaler_handlers = NULL;
}

static void
ofa_tva_form_store_class_init( ofaTVAFormStoreClass *klass )
{
	static const gchar *thisfn = "ofa_tva_form_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_store_finalize;
}

/**
 * ofa_tva_form_store_new:
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new #ofaTVAFormStore and attached it to the @dossier
 * if not already done. Else get the already allocated #ofaTVAFormStore
 * from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 *
 * Returns: a new reference to the #ofaTVAFormStore object.
 */
ofaTVAFormStore *
ofa_tva_form_store_new( ofaIGetter *getter )
{
	ofaTVAFormStore *store;
	ofaTVAFormStorePrivate *priv;
	myICollector *collector;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	collector = ofa_igetter_get_collector( getter );
	store = ( ofaTVAFormStore * ) my_icollector_single_get_object( collector, OFA_TYPE_TVA_FORM_STORE );

	if( store ){
		g_return_val_if_fail( OFA_IS_TVA_FORM_STORE( store ), NULL );

	} else {
		store = g_object_new( OFA_TYPE_TVA_FORM_STORE, NULL );

		priv = ofa_tva_form_store_get_instance_private( store );

		priv->getter = getter;

		st_col_types[TVA_FORM_COL_NOTES_PNG] = GDK_TYPE_PIXBUF;
		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), TVA_N_COLUMNS, st_col_types );

		load_dataset( store );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		my_icollector_single_set_object( collector, store );
		signaler_connect_to_signaling_system( store );
	}

	return( store );
}

/*
 * sorting the store per form code
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaTVAFormStore *self )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, TVA_FORM_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, TVA_FORM_COL_MNEMO, &bmnemo, -1 );

	cmp = my_collate( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

static void
load_dataset( ofaTVAFormStore *self )
{
	ofaTVAFormStorePrivate *priv;
	const GList *dataset, *it;
	ofoTVAForm *form;

	priv = ofa_tva_form_store_get_instance_private( self );

	dataset = ofo_tva_form_get_dataset( priv->getter );

	for( it=dataset ; it ; it=it->next ){
		form = OFO_TVA_FORM( it->data );
		insert_row( self, form );
	}
}

static void
insert_row( ofaTVAFormStore *self, const ofoTVAForm *form )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, form, &iter );
}

static void
set_row_by_iter( ofaTVAFormStore *self, const ofoTVAForm *form, GtkTreeIter *iter )
{
	static const gchar *thisfn = "ofa_tva_form_store_set_row";
	gchar *screstamp, *supdstamp;
	gboolean is_enabled;
	const gchar *notes, *chascorresp, *cenabled;
	GError *error;
	GdkPixbuf *notes_png;

	screstamp  = my_stamp_to_str( ofo_tva_form_get_cre_stamp( form ), MY_STAMP_DMYYHM );
	supdstamp  = my_stamp_to_str( ofo_tva_form_get_upd_stamp( form ), MY_STAMP_DMYYHM );

	chascorresp = ofo_tva_form_get_has_correspondence( form ) ? _( "Yes" ) : _( "No" );

	is_enabled = ofo_tva_form_get_is_enabled( form );
	cenabled = is_enabled ? _( "Yes" ) : _( "No" );

	notes = ofo_tva_form_get_notes( form );
	error = NULL;
	notes_png = gdk_pixbuf_new_from_resource( my_strlen( notes ) ? st_resource_notes_png : st_resource_filler_png, &error );
	if( error ){
		g_warning( "%s: gdk_pixbuf_new_from_resource: %s", thisfn, error->message );
		g_error_free( error );
	}

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			TVA_FORM_COL_MNEMO,              ofo_tva_form_get_mnemo( form ),
			TVA_FORM_COL_LABEL,              ofo_tva_form_get_label( form ),
			TVA_FORM_COL_CRE_USER,           ofo_tva_form_get_cre_user( form ),
			TVA_FORM_COL_CRE_STAMP,          screstamp,
			TVA_FORM_COL_ENABLED,            cenabled,
			TVA_FORM_COL_ENABLED_B,          is_enabled,
			TVA_FORM_COL_HAS_CORRESPONDENCE, chascorresp,
			TVA_FORM_COL_NOTES,              notes,
			TVA_FORM_COL_NOTES_PNG,          notes_png,
			TVA_FORM_COL_UPD_USER,           ofo_tva_form_get_upd_user( form ),
			TVA_FORM_COL_UPD_STAMP,          supdstamp,
			TVA_FORM_COL_OBJECT,             form,
			-1 );

	g_object_unref( notes_png );
	g_free( supdstamp );
	g_free( screstamp );
}

static gboolean
find_form_by_mnemo( ofaTVAFormStore *self, const gchar *code, GtkTreeIter *iter )
{
	gchar *str;
	gint cmp;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), iter, TVA_FORM_COL_MNEMO, &str, -1 );
			cmp = g_utf8_collate( str, code );
			g_free( str );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), iter )){
				break;
			}
		}
	}

	return( FALSE );
}

static void
set_ope_template_new_id( ofaTVAFormStore *self, const gchar *prev_id, const gchar *new_id )
{
	GtkTreeIter iter;
	ofoTVAForm *form;

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( self ), &iter )){
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( self ), &iter, TVA_FORM_COL_OBJECT, &form, -1 );
			g_return_if_fail( form && OFO_IS_TVA_FORM( form ));
			g_object_unref( form );

			ofo_tva_form_update_ope_template( form, prev_id, new_id );

			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( self ), &iter )){
				break;
			}
		}
	}
}

/*
 * connect to the hub signaling system
 */
static void
signaler_connect_to_signaling_system( ofaTVAFormStore *self )
{
	ofaTVAFormStorePrivate *priv;
	ofaISignaler *signaler;
	gulong handler;

	priv = ofa_tva_form_store_get_instance_private( self );

	signaler = ofa_igetter_get_signaler( priv->getter );

	handler = g_signal_connect( signaler, SIGNALER_BASE_NEW, G_CALLBACK( signaler_on_new_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_BASE_DELETED, G_CALLBACK( signaler_on_deleted_base ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );

	handler = g_signal_connect( signaler, SIGNALER_COLLECTION_RELOAD, G_CALLBACK( signaler_on_reload_collection ), self );
	priv->signaler_handlers = g_list_prepend( priv->signaler_handlers, ( gpointer ) handler );
}

/*
 * SIGNALER_BASE_NEW signal handler
 */
static void
signaler_on_new_base( ofaISignaler *signaler, ofoBase *object, ofaTVAFormStore *self )
{
	static const gchar *thisfn = "ofa_tva_form_store_signaler_on_new_base";

	g_debug( "%s: signaler=%p, object=%p (%s), instance=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_TVA_FORM( object )){
		insert_row( self, OFO_TVA_FORM( object ));
	}
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, ofaTVAFormStore *self )
{
	static const gchar *thisfn = "ofa_tva_form_store_signaler_on_updated_base";
	GtkTreeIter iter;
	const gchar *code, *new_id;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) self );

	if( OFO_IS_TVA_FORM( object )){
		new_id = ofo_tva_form_get_mnemo( OFO_TVA_FORM( object ));
		code = prev_id ? prev_id : new_id;
		if( find_form_by_mnemo( self, code, &iter )){
			set_row_by_iter( self, OFO_TVA_FORM( object ), &iter);
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		new_id = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
		if( prev_id && my_collate( prev_id, new_id )){
			set_ope_template_new_id( self, prev_id, new_id );
		}
	}
}

/*
 * SIGNALER_BASE_DELETED signal handler
 */
static void
signaler_on_deleted_base( ofaISignaler *signaler, ofoBase *object, ofaTVAFormStore *self )
{
	static const gchar *thisfn = "ofa_tva_form_store_signaler_on_deleted_base";
	GtkTreeIter iter;

	g_debug( "%s: signaler=%p, object=%p (%s), self=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) self );

	if( OFO_IS_TVA_FORM( object )){
		if( find_form_by_mnemo( self, ofo_tva_form_get_mnemo( OFO_TVA_FORM( object )), &iter )){
			gtk_list_store_remove( GTK_LIST_STORE( self ), &iter );
		}
	}
}

/*
 * SIGNALER_COLLECTION_RELOAD signal handler
 */
static void
signaler_on_reload_collection( ofaISignaler *signaler, GType type, ofaTVAFormStore *self )
{
	static const gchar *thisfn = "ofa_tva_form_store_signaler_on_reload_collection";

	g_debug( "%s: signaler=%p, type=%lu, self=%p",
			thisfn, ( void * ) signaler, type, ( void * ) self );

	if( type == OFO_TYPE_TVA_FORM ){
		gtk_list_store_clear( GTK_LIST_STORE( self ));
		load_dataset( self );
	}
}
