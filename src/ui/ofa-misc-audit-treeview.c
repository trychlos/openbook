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

#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"

#include "ui/ofa-misc-audit-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;

	/* UI
	 */
	ofaMiscAuditStore *store;
}
	ofaMiscAuditTreeviewPrivate;

static void setup_columns( ofaMiscAuditTreeview *view );
static gint tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaMiscAuditTreeview, ofa_misc_audit_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaMiscAuditTreeview ))

static void
misc_audit_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_treeview_parent_class )->finalize( instance );
}

static void
misc_audit_treeview_dispose( GObject *instance )
{
	ofaMiscAuditTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MISC_AUDIT_TREEVIEW( instance ));

	priv = ofa_misc_audit_treeview_get_instance_private( OFA_MISC_AUDIT_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->store );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_misc_audit_treeview_parent_class )->dispose( instance );
}

static void
ofa_misc_audit_treeview_init( ofaMiscAuditTreeview *self )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_init";
	ofaMiscAuditTreeviewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MISC_AUDIT_TREEVIEW( self ));

	priv = ofa_misc_audit_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_misc_audit_treeview_class_init( ofaMiscAuditTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = misc_audit_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = misc_audit_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = tvbin_v_sort;
}

/**
 * ofa_misc_audit_treeview_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a new #ofaMiscAuditTreeview instance.
 */
ofaMiscAuditTreeview *
ofa_misc_audit_treeview_new( ofaIGetter *getter )
{
	ofaMiscAuditTreeview *view;
	ofaMiscAuditTreeviewPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	view = g_object_new( OFA_TYPE_MISC_AUDIT_TREEVIEW,
					"ofa-tvbin-getter", getter,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	priv = ofa_misc_audit_treeview_get_instance_private( view );

	priv->getter = getter;

	return( view );
}

/**
 * ofa_misc_audit_treeview_set_settings_key:
 * @view: this #ofaMiscAuditTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_misc_audit_treeview_set_settings_key( ofaMiscAuditTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_set_settings_key";
	ofaMiscAuditTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_MISC_AUDIT_TREEVIEW( view ));

	priv = ofa_misc_audit_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_name( OFA_TVBIN( view ), key );
}

/**
 * ofa_misc_audit_treeview_setup_columns:
 * @view: this #ofaMiscAuditTreeview instance.
 *
 * Setup the treeview columns.
 */
void
ofa_misc_audit_treeview_setup_columns( ofaMiscAuditTreeview *view )
{
	ofaMiscAuditTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_MISC_AUDIT_TREEVIEW( view ));

	priv = ofa_misc_audit_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	setup_columns( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaMiscAuditTreeview *self )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), AUDIT_COL_DATE,    _( "Timestamp" ),  NULL);
	ofa_tvbin_add_column_text_rx( OFA_TVBIN( self ), AUDIT_COL_QUERY,   _( "Query" ),      NULL );
	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), AUDIT_COL_LINENUM, _( "Line" ),       NULL );

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), AUDIT_COL_QUERY );
}

/**
 * ofa_misc_audit_treeview_setup_store:
 * @view: this #ofaMiscAuditTreeview instance.
 *
 * Create the store which automatically loads the first page of the
 * dataset.
 *
 * Returns: the #ofaMiscAuditStore instance.
 */
ofaMiscAuditStore *
ofa_misc_audit_treeview_setup_store( ofaMiscAuditTreeview *view )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_setup_store";
	ofaMiscAuditTreeviewPrivate *priv;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_MISC_AUDIT_TREEVIEW( view ), NULL );

	priv = ofa_misc_audit_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( !priv->store ){
		priv->store = ofa_misc_audit_store_new( priv->getter );
		ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	}

	gtk_widget_show_all( GTK_WIDGET( view ));

	return( priv->store );
}

static gint
tvbin_v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_misc_audit_treeview_v_sort";
	gint cmp;
	gchar *data, *quea;
	gchar *datb, *queb;
	guint numa, numb;

	gtk_tree_model_get( tmodel, a,
			AUDIT_COL_DATE,      &data,
			AUDIT_COL_QUERY,     &quea,
			AUDIT_COL_LINENUM_I, &numa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			AUDIT_COL_DATE,      &datb,
			AUDIT_COL_QUERY,     &queb,
			AUDIT_COL_LINENUM_I, &numb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case AUDIT_COL_DATE:
			cmp = my_collate( data, datb );
			break;
		case AUDIT_COL_QUERY:
			cmp = my_collate( quea, queb );
			break;
		case AUDIT_COL_LINENUM:
			cmp = numa < numb ? -1 : ( numa > numb ? 1 : 0 );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( data );
	g_free( quea );

	g_free( datb );
	g_free( queb );

	return( cmp );
}
