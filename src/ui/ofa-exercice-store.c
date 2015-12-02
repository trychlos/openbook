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
#include "api/ofa-ifile-period.h"
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
		G_TYPE_STRING,					/* localized status */
		G_TYPE_STRING, 					/* begin date (user display) */
		G_TYPE_STRING,					/* end date (user display) */
		G_TYPE_STRING,				 	/* localized label */
		G_TYPE_OBJECT					/* ofaIFilePeriod */
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
	ofaIFilePeriod *a_period, *b_period;
	gint cmp;

	gtk_tree_model_get( tmodel, a, EXERCICE_COL_PERIOD, &a_period, -1 );
	g_object_unref( a_period );
	gtk_tree_model_get( tmodel, b, EXERCICE_COL_PERIOD, &b_period, -1 );
	g_object_unref( b_period );

	cmp = my_date_compare_ex(
				ofa_ifile_period_get_begin_date( a_period ),
				ofa_ifile_period_get_begin_date( b_period ),
				TRUE );

	return( -cmp );
}

/**
 * ofa_exercice_store_set_dossier:
 * @store: this #ofaExerciceStore instance.
 * @meta: the #ofaIFileMeta dossier.
 *
 * Set the store with defined financial periods for the @meta dossier.
 */
void
ofa_exercice_store_set_dossier( ofaExerciceStore *store, ofaIFileMeta *meta )
{
	ofaExerciceStorePrivate *priv;
	GList *period_list, *it;
	ofaIFilePeriod *period;
	GtkTreeIter iter;
	gchar *begin, *end, *status, *label;

	g_return_if_fail( store && OFA_IS_EXERCICE_STORE( store ));
	g_return_if_fail( meta && OFA_IS_IFILE_META( meta ));

	priv = store->priv;

	if( !priv->dispose_has_run ){

		gtk_list_store_clear( GTK_LIST_STORE( store ));
		period_list = ofa_ifile_meta_get_periods( meta );

		for( it=period_list; it ; it=it->next ){
			period = ( ofaIFilePeriod * ) it->data;

			label = ofa_ifile_period_get_label( period );
			status = ofa_ifile_period_get_status( period );
			begin = my_date_to_str(
							ofa_ifile_period_get_begin_date( period ),
							ofa_prefs_date_display());
			end = my_date_to_str(
							ofa_ifile_period_get_end_date( period ),
							ofa_prefs_date_display());

			gtk_list_store_insert_with_values(
					GTK_LIST_STORE( store ),
					&iter,
					-1,
					EXERCICE_COL_LABEL,  label,
					EXERCICE_COL_BEGIN,  begin,
					EXERCICE_COL_END,    end,
					EXERCICE_COL_STATUS, status,
					EXERCICE_COL_PERIOD, period,
					-1 );

			g_free( begin );
			g_free( end );
			g_free( status );
			g_free( label );
		}

		ofa_ifile_meta_free_periods( period_list );
	}
}
