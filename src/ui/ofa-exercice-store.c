/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-preferences.h"

#include "ui/ofa-exercice-store.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* initialization
	 */
	ofaHub   *hub;
}
	ofaExerciceStorePrivate;

static GType st_col_types[EXERCICE_N_COLUMNS] = {
		G_TYPE_STRING,					/* localized status */
		G_TYPE_STRING, 					/* begin date (user display) */
		G_TYPE_STRING,					/* end date (user display) */
		G_TYPE_STRING,				 	/* localized label */
		G_TYPE_OBJECT					/* ofaIDBExerciceMeta */
};

static gint on_sort_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaExerciceStore *store );

G_DEFINE_TYPE_EXTENDED( ofaExerciceStore, ofa_exercice_store, GTK_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaExerciceStore ))

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

	priv = ofa_exercice_store_get_instance_private( OFA_EXERCICE_STORE( instance ));

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
	ofaExerciceStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXERCICE_STORE( self ));

	priv = ofa_exercice_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_exercice_store_class_init( ofaExerciceStoreClass *klass )
{
	static const gchar *thisfn = "ofa_exercice_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = exercice_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = exercice_store_finalize;
}

/**
 * ofa_exercice_store_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a new #ofaExerciceStore instance.
 */
ofaExerciceStore *
ofa_exercice_store_new( ofaHub *hub )
{
	ofaExerciceStore *store;
	ofaExerciceStorePrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = g_object_new( OFA_TYPE_EXERCICE_STORE, NULL );

	priv = ofa_exercice_store_get_instance_private( store );

	priv->hub = hub;

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
	ofaIDBExerciceMeta *a_period, *b_period;
	gint cmp;

	gtk_tree_model_get( tmodel, a, EXERCICE_COL_EXE_META, &a_period, -1 );
	g_object_unref( a_period );
	gtk_tree_model_get( tmodel, b, EXERCICE_COL_EXE_META, &b_period, -1 );
	g_object_unref( b_period );

	cmp = my_date_compare_ex(
				ofa_idbexercice_meta_get_begin_date( a_period ),
				ofa_idbexercice_meta_get_begin_date( b_period ),
				TRUE );

	return( -cmp );
}

/**
 * ofa_exercice_store_set_dossier:
 * @store: this #ofaExerciceStore instance.
 * @meta: the #ofaIDBDossierMeta dossier.
 *
 * Set the store with defined financial periods for the @meta dossier.
 */
void
ofa_exercice_store_set_dossier( ofaExerciceStore *store, ofaIDBDossierMeta *meta )
{
	ofaExerciceStorePrivate *priv;
	const GList *period_list, *it;
	ofaIDBExerciceMeta *period;
	GtkTreeIter iter;
	gchar *begin, *end, *status, *label;

	g_return_if_fail( store && OFA_IS_EXERCICE_STORE( store ));
	g_return_if_fail( meta && OFA_IS_IDBDOSSIER_META( meta ));

	priv = ofa_exercice_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_list_store_clear( GTK_LIST_STORE( store ));
	period_list = ofa_idbdossier_meta_get_periods( meta );

	for( it=period_list; it ; it=it->next ){
		period = ( ofaIDBExerciceMeta * ) it->data;

		label = ofa_idbexercice_meta_get_label( period );
		status = ofa_idbexercice_meta_get_status( period );
		begin = my_date_to_str(
						ofa_idbexercice_meta_get_begin_date( period ),
						ofa_prefs_date_display( priv->hub ));
		end = my_date_to_str(
						ofa_idbexercice_meta_get_end_date( period ),
						ofa_prefs_date_display( priv->hub ));

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( store ),
				&iter,
				-1,
				EXERCICE_COL_LABEL,  label,
				EXERCICE_COL_BEGIN,  begin,
				EXERCICE_COL_END,    end,
				EXERCICE_COL_STATUS, status,
				EXERCICE_COL_EXE_META, period,
				-1 );

		g_free( begin );
		g_free( end );
		g_free( status );
		g_free( label );
	}
}
