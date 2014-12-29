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

#include "core/ofa-preferences.h"

#include "ui/ofa-dossier-misc.h"
#include "ui/ofa-dossier-store.h"

/* private instance data
 */
struct _ofaDossierStorePrivate {
	gboolean    dispose_has_run;

	/* runtime data
	 */
};

static GType st_col_types[DOSSIER_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING					/* dname, dbms */
};

static gint     on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaDossierStore *store );
static void     load_dataset( ofaDossierStore *store );
static void     insert_row( ofaDossierStore *store, const gchar *dname );
static void     set_row( ofaDossierStore *store, const gchar *dname, GtkTreeIter *iter );

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
}

/**
 * ofa_dossier_store_new:
 */
ofaDossierStore *
ofa_dossier_store_new( void )
{
	ofaDossierStore *store;

	store = g_object_new( OFA_TYPE_DOSSIER_STORE, NULL );

	gtk_list_store_set_column_types(
			GTK_LIST_STORE( store ), DOSSIER_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	load_dataset( store );

	return( store );
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
static void
load_dataset( ofaDossierStore *store )
{
	GSList *dataset, *it;

	dataset = ofa_dossier_misc_get_dossiers();

	for( it=dataset ; it ; it=it->next ){
		insert_row( store, ( const gchar * ) it->data );
	}

	ofa_dossier_misc_free_dossiers( dataset );
}

static void
insert_row( ofaDossierStore *store, const gchar *strlist )
{
	static const gchar *thisfn = "ofa_dossier_store_insert_row";
	GtkTreeIter iter;

	g_debug( "%s: store=%p, strlist=%s", thisfn, ( void * ) store, strlist );

	gtk_list_store_insert( GTK_LIST_STORE( store ), &iter, -1 );
	set_row( store, strlist, &iter );
}

static void
set_row( ofaDossierStore *store, const gchar *strlist, GtkTreeIter *iter )
{
	gchar **array;

	array = g_strsplit( strlist, ";", -1 );

	gtk_list_store_set(
			GTK_LIST_STORE( store ),
			iter,
			DOSSIER_COL_DNAME, *array,
			DOSSIER_COL_DBMS, *(array+1),
			-1 );

	g_strfreev( array );
}

/**
 * ofa_dossier_store_add_row:
 */
void
ofa_dossier_store_add_row( ofaDossierStore *store, const gchar *dname, const gchar *dbms )
{
	ofaDossierStorePrivate *priv;
	GtkTreeIter iter;

	g_return_if_fail( store && OFA_IS_DOSSIER_STORE( store ));

	priv = store->priv;

	if( !priv->dispose_has_run ){

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( store ),
				&iter,
				-1,
				DOSSIER_COL_DNAME, dname,
				DOSSIER_COL_DBMS,  dbms,
				-1 );
	}
}
