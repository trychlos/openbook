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

#include <glib/gi18n.h>

#include "api/ofa-hub.h"
#include "api/ofa-istore.h"
#include "api/ofa-itree-adder.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-ope-template-store.h"
#include "api/ofo-ope-template.h"
#include "api/ofa-tvbin.h"

#include "tva/ofa-tva-tree-adder.h"
#include "tva/ofo-tva-form.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;

	/* runtime data
	 */
	GList   *stores;
}
	ofaTvaTreeAdderPrivate;

/* the managed stores
 */
typedef struct {
	ofaIStore *store;
	guint      orig_count;
}
	sStore;

/* OpeTemplateStore added columns
 */
enum {
	VAT_OPE_TEMPLATE_COL_VAT = 0,
	VAT_OPE_TEMPLATE_N_COLUMNS
};

static GType st_ope_template_store_types[VAT_OPE_TEMPLATE_N_COLUMNS] = {
		0										/* gdk_type_pixbuf */
};

static const gchar *st_resource_filler_png = "/org/trychlos/openbook/tva/filler.png";
static const gchar *st_resource_vat_png    = "/org/trychlos/openbook/tva/ofa-vat-icon-16x16.png";

static void     itree_adder_iface_init( ofaITreeAdderInterface *iface );
static GType   *itree_adder_get_column_types( ofaITreeAdder *instance, ofaIStore *store, guint orig_cols_count, guint *add_cols );
static void     itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeIter *iter, void *object );
static void     ope_template_set_is_vat( ofaTvaTreeAdder *self, sStore *store_data, ofaHub *hub, GtkTreeIter *iter, guint col_id, ofoOpeTemplate *template );
static gboolean itree_adder_sort( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, gint *cmp );
static gint     ope_template_sort( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, sStore *store_data );
static void     itree_adder_add_columns( ofaITreeAdder *instance, ofaIStore *store, ofaTVBin *bin );
static void     ope_template_add_columns( ofaTvaTreeAdder *self, ofaTVBin *bin, sStore *store_data );
static sStore  *get_store_data( ofaTvaTreeAdder *self, void *store, gboolean create );
static void     on_store_finalized( ofaTvaTreeAdder *self, GObject *finalized_store );
static void     free_store( sStore *sdata );

G_DEFINE_TYPE_EXTENDED( ofaTvaTreeAdder, ofa_tva_tree_adder, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaTvaTreeAdder )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITREE_ADDER, itree_adder_iface_init ))

static void
tva_tree_adder_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_finalize";
	ofaTvaTreeAdderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_TREE_ADDER( instance ));

	/* free data members here */
	priv = ofa_tva_tree_adder_get_instance_private( OFA_TVA_TREE_ADDER( instance ));

	g_list_free_full( priv->stores, ( GDestroyNotify ) free_store );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_tree_adder_parent_class )->finalize( instance );
}

static void
tva_tree_adder_dispose( GObject *instance )
{
	ofaTvaTreeAdderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_TREE_ADDER( instance ));

	priv = ofa_tva_tree_adder_get_instance_private( OFA_TVA_TREE_ADDER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_tree_adder_parent_class )->dispose( instance );
}

static void
ofa_tva_tree_adder_init( ofaTvaTreeAdder *self )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_init";
	ofaTvaTreeAdderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_TREE_ADDER( self ));

	priv = ofa_tva_tree_adder_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_tva_tree_adder_class_init( ofaTvaTreeAdderClass *klass )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_tree_adder_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_tree_adder_finalize;
}

/*
 * #ofaITreeAdder interface management
 */
static void
itree_adder_iface_init( ofaITreeAdderInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_itree_adder_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_column_types = itree_adder_get_column_types;
	iface->set_values = itree_adder_set_values;
	iface->sort = itree_adder_sort;
	iface->add_columns = itree_adder_add_columns;
}

static GType *
itree_adder_get_column_types( ofaITreeAdder *instance, ofaIStore *store, guint orig_cols_count, guint *add_cols )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_get_column_types";
	sStore *store_data;

	g_debug( "%s: instance=%p, store=%p, orig_cols_count=%u, add_cols=%p",
			thisfn, ( void * ) instance, ( void * ) store, orig_cols_count, ( void * ) add_cols );

	*add_cols = 0;
	store_data = get_store_data( OFA_TVA_TREE_ADDER( instance ), store, TRUE );

	if( OFA_IS_OPE_TEMPLATE_STORE( store )){
		st_ope_template_store_types[VAT_OPE_TEMPLATE_COL_VAT] = GDK_TYPE_PIXBUF;
		*add_cols = VAT_OPE_TEMPLATE_N_COLUMNS;
		store_data->orig_count = orig_cols_count;
		return( st_ope_template_store_types );
	}

	return( NULL );
}

static void
itree_adder_set_values( ofaITreeAdder *instance, ofaIStore *store, ofaHub *hub, GtkTreeIter *iter, void *object )
{
	sStore *store_data;
	gint i;

	store_data = get_store_data( OFA_TVA_TREE_ADDER( instance ), store, FALSE );
	if( store_data ){
		if( OFA_IS_OPE_TEMPLATE_STORE( store )){
			for( i=0 ; i<VAT_OPE_TEMPLATE_N_COLUMNS ; ++i ){
				if( i==VAT_OPE_TEMPLATE_COL_VAT ){
					ope_template_set_is_vat(
							OFA_TVA_TREE_ADDER( instance ), store_data, hub, iter, i, OFO_OPE_TEMPLATE( object ));
				}
			}
		}
	}
}

/*
 * set a small graphic indicator if the operation template is used in
 * a VAT form
 */
static void
ope_template_set_is_vat( ofaTvaTreeAdder *self, sStore *store_data, ofaHub *hub, GtkTreeIter *iter, guint col_id, ofoOpeTemplate *template )
{
	gboolean is_vat;
	GdkPixbuf *png;
	const gchar *mnemo;

	mnemo = ofo_ope_template_get_mnemo( template );
	is_vat = ofo_tva_form_use_ope_template( hub, mnemo );
	png = gdk_pixbuf_new_from_resource( is_vat ? st_resource_vat_png : st_resource_filler_png, NULL );

	if( GTK_IS_LIST_STORE( store_data->store )){
		gtk_list_store_set( GTK_LIST_STORE( store_data->store ), iter, col_id+store_data->orig_count, png, -1 );
	} else {
		gtk_tree_store_set( GTK_TREE_STORE( store_data->store ), iter, col_id+store_data->orig_count, png, -1 );
	}
}

static gboolean
itree_adder_sort( ofaITreeAdder *instance, ofaIStore *store,
		ofaHub *hub, GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column_id, gint *cmp )
{
	sStore *store_data;

	store_data = get_store_data( OFA_TVA_TREE_ADDER( instance ), store, FALSE );
	if( store_data ){
		if( OFA_IS_OPE_TEMPLATE_STORE( store ) &&
			column_id >= store_data->orig_count && column_id < store_data->orig_count+VAT_OPE_TEMPLATE_N_COLUMNS ){
				*cmp = ope_template_sort( model, a, b, column_id-store_data->orig_count, store_data );
				return( TRUE );
		}
	}

	return( FALSE );
}

static gint
ope_template_sort( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id, sStore *store_data )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_ope_template_sort";
	gint cmp;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			store_data->orig_count+VAT_OPE_TEMPLATE_COL_VAT, &pnga,
			-1 );

	gtk_tree_model_get( tmodel, b,
			store_data->orig_count+VAT_OPE_TEMPLATE_COL_VAT, &pngb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case VAT_OPE_TEMPLATE_COL_VAT:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
		break;
	}

	g_clear_object( &pnga );
	g_clear_object( &pngb );

	return( cmp );
}

static void
itree_adder_add_columns( ofaITreeAdder *instance, ofaIStore *store, ofaTVBin *bin )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_add_columns";
	sStore *store_data;

	g_debug( "%s: instance=%p, store=%p, bin=%p",
			thisfn, ( void * ) instance, ( void * ) store, ( void * ) bin );

	store_data = get_store_data( OFA_TVA_TREE_ADDER( instance ), store, FALSE );
	if( store_data ){
		if( OFA_IS_OPE_TEMPLATE_STORE( store )){
			ope_template_add_columns( OFA_TVA_TREE_ADDER( instance ), bin, store_data );
		}
	}
}

static void
ope_template_add_columns( ofaTvaTreeAdder *self, ofaTVBin *bin, sStore *store_data )
{
	ofa_tvbin_add_column_pixbuf ( bin, store_data->orig_count+VAT_OPE_TEMPLATE_COL_VAT, _( "V" ), _( "VAT indicator" ));
}

static sStore *
get_store_data( ofaTvaTreeAdder *self, void *store, gboolean create )
{
	ofaTvaTreeAdderPrivate *priv;
	GList *it;
	sStore *sdata;

	priv = ofa_tva_tree_adder_get_instance_private( self );

	if( priv->dispose_has_run ){
		return( NULL );
	}

	for( it=priv->stores ; it ; it=it->next ){
		sdata = ( sStore * ) it->data;
		if( sdata->store == store ){
			return( sdata );
		}
	}

	sdata = NULL;

	if( create ){
		sdata = g_new0( sStore, 1 );
		sdata->store = store;
		priv->stores = g_list_prepend( priv->stores, sdata );
		g_object_weak_ref( G_OBJECT( store ), ( GWeakNotify ) on_store_finalized, self );
	}

	return( sdata );
}

static void
on_store_finalized( ofaTvaTreeAdder *self, GObject *finalized_store )
{
	static const gchar *thisfn = "ofa_tva_tree_adder_on_store_finalized";
	ofaTvaTreeAdderPrivate *priv;
	sStore *sdata;

	g_debug( "%s: self=%p, finalized_store=%p (%s)",
			thisfn, ( void * ) self, ( void * ) finalized_store, G_OBJECT_TYPE_NAME( finalized_store ));

	priv = ofa_tva_tree_adder_get_instance_private( self );

	sdata = get_store_data( self, finalized_store, FALSE );

	if( sdata ){
		priv->stores = g_list_remove( priv->stores, sdata );
		free_store( sdata );
	}
}

static void
free_store( sStore *sdata )
{
	g_free( sdata );
}
