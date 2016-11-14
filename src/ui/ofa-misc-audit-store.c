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

#include <math.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"

#include "ui/ofa-misc-audit-store.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaHub     *hub;

	/* runtime
	 */
	guint       page_size;
	guint       pages_count;
}
	ofaMiscAuditStorePrivate;

/* audit record data structure
 */
typedef struct {
	gchar    *stamp;
	gchar    *query;
}
	sAudit;

static GType st_col_types[AUDIT_N_COLUMNS] = {
	G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,			/* date, command, linenum */
	G_TYPE_INT												/* linenum_i */
};

static GList *load_dataset( ofaMiscAuditStore *self, guint pageno );
static void   insert_row( ofaMiscAuditStore *self, guint lineno, sAudit *audit );
static void   set_row_by_iter( ofaMiscAuditStore *self, GtkTreeIter *iter, guint lineno, sAudit *audit );
static void   audit_free( sAudit *audit );

G_DEFINE_TYPE_EXTENDED( ofaMiscAuditStore, ofa_misc_audit_store, OFA_TYPE_LIST_STORE, 0,
		G_ADD_PRIVATE( ofaMiscAuditStore ))

static void
misc_audit_store_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_misc_audit_store_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_STORE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_store_parent_class )->finalize( instance );
}

static void
misc_audit_store_dispose( GObject *instance )
{
	ofaMiscAuditStorePrivate *priv;

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_STORE( instance ));

	priv = ofa_misc_audit_store_get_instance_private( OFA_MISC_AUDIT_STORE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_store_parent_class )->dispose( instance );
}

static void
ofa_misc_audit_store_init( ofaMiscAuditStore *self )
{
	static const gchar *thisfn = "ofa_misc_audit_store_init";
	ofaMiscAuditStorePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MISC_AUDIT_STORE( self ));

	priv = ofa_misc_audit_store_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_misc_audit_store_class_init( ofaMiscAuditStoreClass *klass )
{
	static const gchar *thisfn = "ofa_misc_audit_store_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = misc_audit_store_dispose;
	G_OBJECT_CLASS( klass )->finalize = misc_audit_store_finalize;
}

/**
 * ofa_misc_audit_store_new:
 * @hub: the current #ofaHub object.
 *
 * Instanciates a new #ofaMiscAuditStore.
 *
 * Returns: a new reference to the #ofaMiscAuditStore object, which should be
 * #g_object_unref() by the caller after use.
 */
ofaMiscAuditStore *
ofa_misc_audit_store_new( ofaHub *hub )
{
	ofaMiscAuditStore *store;
	ofaMiscAuditStorePrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	store = g_object_new( OFA_TYPE_MISC_AUDIT_STORE, NULL );

	priv = ofa_misc_audit_store_get_instance_private( store );
	priv->hub = hub;

	gtk_list_store_set_column_types(
			GTK_LIST_STORE( store ), AUDIT_N_COLUMNS, st_col_types );

	return( store );
}

/**
 * ofa_misc_audit_store_get_pages_count:
 * @store: this #ofaMiscAuditStore instance.
 * @page_size: the count of lines to display per page.
 *
 * Returns: the count of pages needed to display the whole content of
 * the @store, giving the provided count @page_size.
 */
guint
ofa_misc_audit_store_get_pages_count( ofaMiscAuditStore *store, guint page_size )
{
	ofaMiscAuditStorePrivate *priv;
	const ofaIDBConnect *connect;
	gint rows;

	g_return_val_if_fail( store && OFA_IS_MISC_AUDIT_STORE( store ), 0 );

	priv = ofa_misc_audit_store_get_instance_private( store );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	priv->page_size = page_size;

	connect = ofa_hub_get_connect( priv->hub );
	ofa_idbconnect_query_int( connect, "SELECT COUNT(*) FROM OFA_T_AUDIT", &rows, TRUE );
	priv->pages_count = ( guint ) ceil(( gdouble ) rows/ page_size );

	return( priv->pages_count );
}

/**
 * ofa_misc_audit_store_load_lines:
 * @store: this #ofaMiscAuditStore instance.
 * @page_num: the number of the page to be displayed, counted from 1.
 *
 * Loads the lines needed to display the @page_num page.
 */
void
ofa_misc_audit_store_load_lines( ofaMiscAuditStore *store, guint page_num )
{
	ofaMiscAuditStorePrivate *priv;
	GList *dataset, *it;
	guint lineno;

	g_return_if_fail( store && OFA_IS_MISC_AUDIT_STORE( store ));

	priv = ofa_misc_audit_store_get_instance_private( store );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_list_store_clear( GTK_LIST_STORE( store ));

	dataset = load_dataset( store, page_num );
	lineno = ( page_num - 1 ) * priv->page_size;

	for( it=dataset ; it ; it=it->next ){
		insert_row( store, lineno++, ( sAudit * ) it->data );
	}

	g_list_free_full( dataset, ( GDestroyNotify ) audit_free );
}

static GList *
load_dataset( ofaMiscAuditStore *self, guint pageno )
{
	ofaMiscAuditStorePrivate *priv;
	GList *dataset;
	const ofaIDBConnect *connect;
	gchar *query;
	GSList *result, *irow, *icol;
	sAudit *audit;

	priv = ofa_misc_audit_store_get_instance_private( self );

	dataset = NULL;
	connect = ofa_hub_get_connect( priv->hub );

	query = g_strdup_printf(
					"SELECT AUD_STAMP,AUD_QUERY FROM OFA_T_AUDIT ORDER BY AUD_STAMP ASC LIMIT %u,%u",
					(( pageno-1) * priv->page_size ), priv->page_size );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			audit = g_new0( sAudit, 1 );
			icol = ( GSList * ) irow->data;
			audit->stamp = g_strdup(( const gchar * ) icol->data );
			icol = icol->next;
			audit->query = g_strdup(( const gchar * ) icol->data );
			dataset = g_list_prepend( dataset, audit );
		}
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	return( g_list_reverse( dataset ));
}

static void
insert_row( ofaMiscAuditStore *self, guint lineno, sAudit *audit )
{
	GtkTreeIter iter;

	gtk_list_store_insert( GTK_LIST_STORE( self ), &iter, -1 );
	set_row_by_iter( self, &iter, lineno, audit );
}

static void
set_row_by_iter( ofaMiscAuditStore *self, GtkTreeIter *iter, guint lineno, sAudit *audit )
{
	gchar *snum;

	snum = g_strdup_printf( "%u", lineno );

	gtk_list_store_set(
			GTK_LIST_STORE( self ),
			iter,
			AUDIT_COL_DATE,      audit->stamp,
			AUDIT_COL_QUERY,     audit->query,
			AUDIT_COL_LINENUM,   snum,
			AUDIT_COL_LINENUM_I, lineno,
			-1 );

	g_free( snum );
}

static void
audit_free( sAudit *audit )
{
	g_free( audit->stamp );
	g_free( audit->query );
	g_free( audit );
}
