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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-preferences.h"

#include "ui/ofa-exercice-store.h"

/* private instance data
 */
struct _ofaExerciceStorePrivate {
	gboolean         dispose_has_run;

	/*
	 */
};

static GType st_col_types[EXERCICE_N_COLUMNS] = {
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* status, begin, end */
		G_TYPE_STRING, G_TYPE_STRING				 	/* dbname, label */
};

G_DEFINE_TYPE( ofaExerciceStore, ofa_exercice_store, GTK_TYPE_LIST_STORE )

static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaExerciceStore *store );

static void
exercice_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_exercice_store_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXERCICE_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_store_parent_class )->finalize( instance );
}

static void
exercice_store_dispose( GObject *instance )
{
	ofaExerciceStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXERCICE_STORE( instance ));

	priv = ( OFA_EXERCICE_STORE( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_exercice_store_parent_class )->dispose( instance );
}

static void
ofa_exercice_store_init( ofaExerciceStore *self )
{
	static const gchar *thisfn = "ofa_exercice_store_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_STORE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_EXERCICE_STORE, ofaExerciceStorePrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_exercice_store_class_init( ofaExerciceStoreClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_store_finalize;

	g_type_class_add_private( klass, sizeof( ofaExerciceStorePrivate ));
}

/**
 * ofa_exercice_store_new:
 */
ofaExerciceStore *
ofa_exercice_store_new( void )
{
	ofaExerciceStore *store;

	store = g_object_new( OFA_TYPE_EXERCICE_STORE, NULL );

	gtk_list_store_set_column_types(
			GTK_LIST_STORE( store ), EXERCICE_N_COLUMNS, st_col_types );

	gtk_tree_sortable_set_default_sort_func(
			GTK_TREE_SORTABLE( store ), ( GtkTreeIterCompareFunc ) on_sort_model, store, NULL );
	gtk_tree_sortable_set_sort_column_id(
			GTK_TREE_SORTABLE( store ),
			GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return( store );
}

/*
 * sorting the store per descending exercice so that the current (the
 * most recent) will be the first
 */
static gint
on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaExerciceStore *store )
{
	gchar *abegin, *bbegin;
	GDate ad, bd;
	gint cmp;

	gtk_tree_model_get( tmodel, a, EXERCICE_COL_BEGIN, &abegin, -1 );
	gtk_tree_model_get( tmodel, b, EXERCICE_COL_BEGIN, &bbegin, -1 );

	my_date_set_from_str( &ad, abegin, ofa_prefs_date_display());
	my_date_set_from_str( &bd, bbegin, ofa_prefs_date_display());

	if( my_date_is_valid( &ad )){
		if( my_date_is_valid( &bd )){
			cmp = my_date_compare( &ad, &bd );
		} else {
			cmp = -1;
		}
	} else {
		cmp = 1;
	}

	g_free( abegin );
	g_free( bbegin );

	return( -cmp );
}

/**
 * ofa_exercice_store_set_dossier:
 */
void
ofa_exercice_store_set_dossier( ofaExerciceStore *store, const gchar *dname )
{
	ofaExerciceStorePrivate *priv;
	GSList *list, *it;
	gchar **array;
	GtkTreeIter iter;
	GDate date;
	gchar *sdbegin, *sdend;

	g_return_if_fail( store && OFA_IS_EXERCICE_STORE( store ));
	g_return_if_fail( dname && g_utf8_strlen( dname, -1 ));

	priv = store->priv;

	if( !priv->dispose_has_run ){

		gtk_list_store_clear( GTK_LIST_STORE( store ));

		list = ofa_dossier_misc_get_exercices( dname );

		for( it=list ; it ; it=it->next ){
			array = g_strsplit(( const gchar * ) it->data, ";", -1 );

			my_date_set_from_sql( &date, *(array+2));
			sdbegin = my_date_to_str( &date, ofa_prefs_date_display());

			my_date_set_from_sql( &date, *(array+3));
			sdend = my_date_to_str( &date, ofa_prefs_date_display());

			gtk_list_store_insert_with_values(
					GTK_LIST_STORE( store ),
					&iter,
					-1,
					EXERCICE_COL_LABEL,  *array,
					EXERCICE_COL_DBNAME, *(array+1),
					EXERCICE_COL_BEGIN,  sdbegin,
					EXERCICE_COL_END,    sdend,
					EXERCICE_COL_STATUS, *(array+4),
					-1 );
			g_strfreev( array );
			g_free( sdbegin );
			g_free( sdend );
		}

		ofa_dossier_misc_free_exercices( list );
	}
}
