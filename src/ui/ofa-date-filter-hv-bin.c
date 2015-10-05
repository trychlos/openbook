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

#include "api/my-utils.h"

#include "ui/ofa-date-filter-hv-bin.h"
#include "ui/ofa-idate-filter.h"

/* private instance data
 */
struct _ofaDateFilterHVBinPrivate {
	gboolean dispose_has_run;
};

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-date-filter-hv-bin.ui";

static void  idate_filter_iface_init( ofaIDateFilterInterface *iface );
static guint idate_filter_get_interface_version( const ofaIDateFilter *instance );
static void  idate_filter_add_widget( ofaIDateFilter *instance, GtkWidget *widget, gint where );

G_DEFINE_TYPE_EXTENDED( ofaDateFilterHVBin, ofa_date_filter_hv_bin, GTK_TYPE_BIN, 0, \
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

	priv = OFA_DATE_FILTER_HV_BIN( instance )->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DATE_FILTER_HV_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_DATE_FILTER_HV_BIN, ofaDateFilterHVBinPrivate );
}

static void
ofa_date_filter_hv_bin_class_init( ofaDateFilterHVBinClass *klass )
{
	static const gchar *thisfn = "ofa_date_filter_hv_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = date_filter_hv_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = date_filter_hv_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaDateFilterHVBinPrivate ));
}

/**
 * ofa_date_filter_hv_bin_new:
 * @xml_name: the radical of the XML file which defines the UI.
 *
 * Returns: a newly allocated #ofaDateFilterHVBin object.
 */
ofaDateFilterHVBin *
ofa_date_filter_hv_bin_new( void )
{
	ofaDateFilterHVBin *bin;

	bin = g_object_new( OFA_TYPE_DATE_FILTER_HV_BIN, NULL );

	ofa_idate_filter_setup_bin( OFA_IDATE_FILTER( bin ), st_bin_xml );

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
idate_filter_get_interface_version( const ofaIDateFilter *instance )
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