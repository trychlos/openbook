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

#include "ui/ofa-accounts-filter-vv-bin.h"
#include "ui/ofa-iaccounts-filter.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountsFilterVVBinPrivate {
	gboolean       dispose_has_run;
};

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-accounts-filter-vv-bin.ui";

static void  iaccounts_filter_iface_init( ofaIAccountsFilterInterface *iface );
static guint iaccounts_filter_get_interface_version( const ofaIAccountsFilter *instance );

G_DEFINE_TYPE_EXTENDED( ofaAccountsFilterVVBin, ofa_accounts_filter_vv_bin, GTK_TYPE_BIN, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACCOUNTS_FILTER, iaccounts_filter_iface_init ))

static void
accounts_filter_vv_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_filter_vv_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_FILTER_VV_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_filter_vv_bin_parent_class )->finalize( instance );
}

static void
accounts_filter_vv_bin_dispose( GObject *instance )
{
	ofaAccountsFilterVVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNTS_FILTER_VV_BIN( instance ));

	priv = OFA_ACCOUNTS_FILTER_VV_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_accounts_filter_vv_bin_parent_class )->dispose( instance );
}

static void
ofa_accounts_filter_vv_bin_init( ofaAccountsFilterVVBin *self )
{
	static const gchar *thisfn = "ofa_accounts_filter_vv_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNTS_FILTER_VV_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_ACCOUNTS_FILTER_VV_BIN, ofaAccountsFilterVVBinPrivate );
}

static void
ofa_accounts_filter_vv_bin_class_init( ofaAccountsFilterVVBinClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_filter_vv_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = accounts_filter_vv_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = accounts_filter_vv_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaAccountsFilterVVBinPrivate ));
}

/**
 * ofa_accounts_filter_vv_bin_new:
 * @xml_name: the radical of the XML file which defines the UI.
 *
 * Returns: a newly allocated #ofaAccountsFilterVVBin object.
 */
ofaAccountsFilterVVBin *
ofa_accounts_filter_vv_bin_new( ofaMainWindow *main_window )
{
	ofaAccountsFilterVVBin *bin;

	bin = g_object_new( OFA_TYPE_ACCOUNTS_FILTER_VV_BIN, NULL );

	ofa_iaccounts_filter_setup_bin(
			OFA_IACCOUNTS_FILTER( bin ), st_bin_xml, main_window );

	return( bin );
}

static void
iaccounts_filter_iface_init( ofaIAccountsFilterInterface *iface )
{
	static const gchar *thisfn = "ofa_dates_filter_iaccounts_filter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iaccounts_filter_get_interface_version;
}

static guint
iaccounts_filter_get_interface_version( const ofaIAccountsFilter *instance )
{
	return( 1 );
}
