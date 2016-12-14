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

#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-idate-filter.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaDateFilterHVBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-date-filter-hv-bin.ui";

static void  idate_filter_iface_init( ofaIDateFilterInterface *iface );
static guint idate_filter_get_interface_version( void );
static void  idate_filter_add_widget( ofaIDateFilter *instance, GtkWidget *widget, gint where );

G_DEFINE_TYPE_EXTENDED( ofaDateFilterHVBin, ofa_date_filter_hv_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDateFilterHVBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDATE_FILTER, idate_filter_iface_init ))

static void
date_filter_hv_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_date_filter_hv_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DATE_FILTER_HV_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_date_filter_hv_bin_parent_class )->finalize( instance );
}

static void
date_filter_hv_bin_dispose( GObject *instance )
{
	ofaDateFilterHVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DATE_FILTER_HV_BIN( instance ));

	priv = ofa_date_filter_hv_bin_get_instance_private( OFA_DATE_FILTER_HV_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_date_filter_hv_bin_parent_class )->dispose( instance );
}

static void
ofa_date_filter_hv_bin_init( ofaDateFilterHVBin *self )
{
	static const gchar *thisfn = "ofa_date_filter_hv_bin_init";
	ofaDateFilterHVBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DATE_FILTER_HV_BIN( self ));

	priv = ofa_date_filter_hv_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_date_filter_hv_bin_class_init( ofaDateFilterHVBinClass *klass )
{
	static const gchar *thisfn = "ofa_date_filter_hv_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = date_filter_hv_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = date_filter_hv_bin_finalize;
}

/**
 * ofa_date_filter_hv_bin_new:
 * @hub: the #ofaHub object of the application.
 *
 * Returns: a newly allocated #ofaDateFilterHVBin object.
 */
ofaDateFilterHVBin *
ofa_date_filter_hv_bin_new( ofaHub *hub )
{
	ofaDateFilterHVBin *bin;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	bin = g_object_new( OFA_TYPE_DATE_FILTER_HV_BIN, NULL );

	ofa_idate_filter_setup_bin( OFA_IDATE_FILTER( bin ), hub, st_resource_ui );

	return( bin );
}

static void
idate_filter_iface_init( ofaIDateFilterInterface *iface )
{
	static const gchar *thisfn = "ofa_date_filter_idate_filter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idate_filter_get_interface_version;
	iface->add_widget = idate_filter_add_widget;
}

static guint
idate_filter_get_interface_version( void )
{
	return( 1 );
}

static void
idate_filter_add_widget( ofaIDateFilter *instance, GtkWidget *widget, gint where )
{
	static const gchar *thisfn = "ofa_date_filter_hv_bin_idate_filter_add_widget";
	GtkWidget *grid;
	gint new_row;

	switch( where ){
		case IDATE_FILTER_BEFORE:
			new_row = 0;
			break;
		case IDATE_FILTER_BETWEEN:
			new_row = 1;
			break;
		case IDATE_FILTER_AFTER:
			new_row = 2;
			break;
		default:
			g_warning( "%s: unknown indicator where=%d", thisfn, where );
			return;
			break;
	}

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	gtk_grid_insert_row( GTK_GRID( grid ), new_row );
	gtk_grid_attach( GTK_GRID( grid ), widget, 1, new_row, 2, 1 );
}
