/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-ope-template-store.h"

/* private instance data
 */
struct _ofaOpeTemplateStorePrivate {
	gboolean    dispose_has_run;

	/* runtime data
	 */
};

static GType st_col_types[OPE_TEMPLATE_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* mnemo, label, ledger */
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,	/* notes, upd_user, upd_stamp */
		G_TYPE_OBJECT									/* the #ofoOpeTemplate itself */
};

/* the key which is attached to the dossier in order to identify this
 * store
 */
#define STORE_DATA_DOSSIER                   "ofa-ope-template-store"

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplateStore *store );
static void     load_dataset( ofaOpeTemplateStore *store, ofoDossier *dossier );
static void     insert_row( ofaOpeTemplateStore *store, ofoDossier *dossier, const ofoOpeTemplate *ope );
static void     set_row( ofaOpeTemplateStore *store, ofoDossier *dossier, const ofoOpeTemplate *ope, GtkTreeIter *iter );
static gboolean find_row_by_mnemo( ofaOpeTemplateStore *store, const gchar *mnemo, GtkTreeIter *iter, gboolean *bvalid );
static void     remove_row_by_mnemo( ofaOpeTemplateStore *store, const gchar *mnemo );
static void     setup_signaling_connect( ofaOpeTemplateStore *store, ofoDossier *dossier );
static void     on_new_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplateStore *store );
static void     on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaOpeTemplateStore *store );
static void     on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplateStore *store );
static void     on_reload_dataset( ofoDossier *dossier, GType type, ofaOpeTemplateStore *store );

G_DEFINE_TYPE( ofaOpeTemplateStore, ofa_ope_template_store, OFA_TYPE_LIST_STORE )

static void
ope_template_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_store_parent_class )->finalize( instance );
}

static void
ope_template_store_dispose( GObject *instance )
{
	ofaOpeTemplateStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_STORE( instance ));

	priv = OFA_OPE_TEMPLATE_STORE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_store_parent_class )->dispose( instance );
}

static void
ofa_ope_template_store_init( ofaOpeTemplateStore *self )
{
	static const gchar *thisfn = "ofa_ope_template_store_init";

	g_return_if_fail( OFA_IS_OPE_TEMPLATE_STORE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATE_STORE, ofaOpeTemplateStorePrivate );
}

static void
ofa_ope_template_store_class_init( ofaOpeTemplateStoreClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_store_finalize;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplateStorePrivate ));
}

/**
 * ofa_ope_template_store_new:
 * @dossier: the currently opened #ofoDossier.
 *
 * Instanciates a new #ofaOpeTemplateStore and attached it to the
 * @dossier if not already done. Else get the already allocated
 * #ofaOpeTemplateStore from the @dossier.
 *
 * A weak notify reference is put on this same @dossier, so that the
 * instance will be unreffed when the @dossier will be destroyed.
 */
ofaOpeTemplateStore *
ofa_ope_template_store_new( ofoDossier *dossier )
{
	ofaOpeTemplateStore *store;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	store = ( ofaOpeTemplateStore * ) g_object_get_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER );

	if( store ){
		g_return_val_if_fail( OFA_IS_OPE_TEMPLATE_STORE( store ), NULL );

	} else {
		store = g_object_new(
						OFA_TYPE_OPE_TEMPLATE_STORE,
						OFA_PROP_DOSSIER, dossier,
						NULL );

		gtk_list_store_set_column_types(
				GTK_LIST_STORE( store ), OPE_TEMPLATE_N_COLUMNS, st_col_types );

		gtk_tree_sortable_set_default_sort_func(
				GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
		gtk_tree_sortable_set_sort_column_id(
				GTK_TREE_SORTABLE( store ),
				GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

		g_object_set_data( G_OBJECT( dossier ), STORE_DATA_DOSSIER, store );

		setup_signaling_connect( store, dossier );
	}

	return( store );
}

/**
 * ofa_ope_template_store_load_dataset:
 * @store: this #ofaOpeTemplateStore instance.
 *
 * Load the dataset into the store.
 */
void
ofa_ope_template_store_load_dataset( ofaOpeTemplateStore *store )
{
	ofaOpeTemplateStorePrivate *priv;
	ofoDossier *dossier;

	g_return_if_fail( store && OFA_IS_OPE_TEMPLATE_STORE( store ));

	priv = store->priv;

	if( !priv->dispose_has_run ){

		g_object_get( store, OFA_PROP_DOSSIER, &dossier, NULL );
		g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

		load_dataset( store, dossier );
	}
}

/*
 * sorting the store per account number
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaOpeTemplateStore *store )
{
	gchar *amnemo, *bmnemo;
	gint cmp;

	gtk_tree_model_get( tmodel, a, OPE_TEMPLATE_COL_MNEMO, &amnemo, -1 );
	gtk_tree_model_get( tmodel, b, OPE_TEMPLATE_COL_MNEMO, &bmnemo, -1 );

	cmp = g_utf8_collate( amnemo, bmnemo );

	g_free( amnemo );
	g_free( bmnemo );

	return( cmp );
}

/*
 * load the dataset when columns and dossier have been both set
 */
static void
load_dataset( ofaOpeTemplateStore *store, ofoDossier *dossier )
{
	const GList *dataset, *it;
	ofoOpeTemplate *ope;

	dataset = ofo_ope_template_get_dataset( dossier );

	for( it=dataset ; it ; it=it->next ){
		ope = OFO_OPE_TEMPLATE( it->data );
		insert_row( store, dossier, ope );
	}
}

static void
insert_row( ofaOpeTemplateStore *store, ofoDossier *dossier, const ofoOpeTemplate *ope )
{
	GtkTreeIter iter;

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( store ),
			&iter,
			-1,
			OPE_TEMPLATE_COL_MNEMO, ofo_ope_template_get_mnemo( ope ),
			OPE_TEMPLATE_COL_OBJECT, ope,
			-1 );

	set_row( store, dossier, ope, &iter );
}

static void
set_row( ofaOpeTemplateStore *store, ofoDossier *dossier, const ofoOpeTemplate *ope, GtkTreeIter *iter )
{
	gchar *stamp;

	stamp  = my_utils_stamp_to_str( ofo_ope_template_get_upd_stamp( ope ), MY_STAMP_DMYYHM );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			OPE_TEMPLATE_COL_LABEL,     ofo_ope_template_get_label( ope ),
			OPE_TEMPLATE_COL_LEDGER,    ofo_ope_template_get_ledger( ope ),
			OPE_TEMPLATE_COL_NOTES,     ofo_ope_template_get_notes( ope ),
			OPE_TEMPLATE_COL_UPD_USER,  ofo_ope_template_get_upd_user( ope ),
			OPE_TEMPLATE_COL_UPD_STAMP, stamp,
			-1 );

	g_free( stamp );
}

/*
 * find_row_by_mnemo:
 * @store: this #ofaOpeTemplateStore
 * @number: [in]: the account number we are searching for.
 * @iter: [out]: the last iter equal or immediately greater than the
 *  searched value.
 * @bvalid: [allow-none][out]: set to TRUE if the returned iter is
 *  valid.
 *
 * rows are sorted by mnemonic
 * we exit the search as soon as we get a number greater than the
 * searched one
 *
 * Returns TRUE if we have found an exact match, and @iter addresses
 * this exact match.
 *
 * Returns FALSE if we do not have found a match, and @iter addresses
 * the first number greater than the searched value.
 */
static gboolean
find_row_by_mnemo( ofaOpeTemplateStore *store, const gchar *mnemo, GtkTreeIter *iter, gboolean *bvalid )
{
	gchar *cmp_mnemo;
	gint cmp;

	if( bvalid ){
		*bvalid = FALSE;
	}

	if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), iter )){
		if( bvalid ){
			*bvalid = TRUE;
		}
		while( TRUE ){
			gtk_tree_model_get( GTK_TREE_MODEL( store ), iter, OPE_TEMPLATE_COL_MNEMO, &cmp_mnemo, -1 );
			cmp = g_utf8_collate( cmp_mnemo, mnemo );
			g_free( cmp_mnemo );
			if( cmp == 0 ){
				return( TRUE );
			}
			if( cmp > 0 ){
				return( FALSE );
			}
			if( !gtk_tree_model_iter_next( GTK_TREE_MODEL( store ), iter )){
				if( bvalid ){
					*bvalid = FALSE;
				}
				return( FALSE );
			}
		}
	}

	return( FALSE );
}

static void
remove_row_by_mnemo( ofaOpeTemplateStore *store, const gchar *number )
{
	GtkTreeIter iter;

	if( find_row_by_mnemo( store, number, &iter, NULL )){
		gtk_list_store_remove( GTK_LIST_STORE( store ), &iter );
	}
}

/*
 * connect to the dossier signaling system
 * there is no need to keep trace of the signal handlers, as the lifetime
 * of this store is equal to those of the dossier
 */
static void
setup_signaling_connect( ofaOpeTemplateStore *store, ofoDossier *dossier )
{
	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_DELETED_OBJECT, G_CALLBACK( on_deleted_object ), store );

	g_signal_connect(
			G_OBJECT( dossier ),
			SIGNAL_DOSSIER_RELOAD_DATASET, G_CALLBACK( on_reload_dataset ), store );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplateStore *store )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_OPE_TEMPLATE( object )){
		insert_row( store, dossier, OFO_OPE_TEMPLATE( object ));
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaOpeTemplateStore *store )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_updated_object";
	GtkTreeIter iter;
	const gchar *mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) store );

	if( OFO_IS_OPE_TEMPLATE( object )){
		mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));

		if( prev_id && g_utf8_collate( prev_id, mnemo )){
			remove_row_by_mnemo( store, prev_id );
			insert_row( store, dossier, OFO_OPE_TEMPLATE( object ));

		} else if( find_row_by_mnemo( store, mnemo, &iter, NULL )){
			set_row( store, dossier, OFO_OPE_TEMPLATE( object ), &iter);

		} else {
			g_debug( "%s: not found: mnemo=%s", thisfn, mnemo );
		}
	}
}

static void
on_deleted_object( ofoDossier *dossier, ofoBase *object, ofaOpeTemplateStore *store )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_deleted_object";

	g_debug( "%s: dossier=%p, object=%p (%s), store=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) store );

	if( OFO_IS_OPE_TEMPLATE( object )){
		remove_row_by_mnemo( store, ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object )));
	}
}

static void
on_reload_dataset( ofoDossier *dossier, GType type, ofaOpeTemplateStore *store )
{
	static const gchar *thisfn = "ofa_ope_template_store_on_reload_dataset";

	g_debug( "%s: dossier=%p, type=%lu, store=%p",
			thisfn, ( void * ) dossier, type, ( void * ) store );

	if( type == OFO_TYPE_OPE_TEMPLATE ){
		gtk_list_store_clear( GTK_LIST_STORE( store ));
		load_dataset( store, dossier );
	}
}

/**
 * ofa_ope_template_store_get_by_mnemo:
 * @instance:
 * @mnemo:
 * @iter:
 *
 * Set the iter to the specified row.
 *
 * Returns: %TRUE if found.
 */
gboolean
ofa_ope_template_store_get_by_mnemo( ofaOpeTemplateStore *store, const gchar *mnemo, GtkTreeIter *iter )
{
	ofaOpeTemplateStorePrivate *priv;
	gboolean found;

	g_return_val_if_fail( store && OFA_IS_OPE_TEMPLATE_STORE( store ), FALSE );

	priv = store->priv;
	found = FALSE;

	if( !priv->dispose_has_run ){

		found = find_row_by_mnemo( store, mnemo, iter, NULL );
	}

	return( found );
}
