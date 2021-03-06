/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-igetter.h"

#include "ui/ofa-account-filter-vv-bin.h"
#include "ui/ofa-iaccount-filter.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/* initialization
	 */
	ofaIGetter *getter;
}
	ofaAccountFilterVVBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-account-filter-vv-bin.ui";

static void  iaccount_filter_iface_init( ofaIAccountFilterInterface *iface );

G_DEFINE_TYPE_EXTENDED( ofaAccountFilterVVBin, ofa_account_filter_vv_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaAccountFilterVVBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACCOUNT_FILTER, iaccount_filter_iface_init ))

static void
account_filter_vv_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_filter_vv_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FILTER_VV_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_filter_vv_bin_parent_class )->finalize( instance );
}

static void
account_filter_vv_bin_dispose( GObject *instance )
{
	ofaAccountFilterVVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_FILTER_VV_BIN( instance ));

	priv = ofa_account_filter_vv_bin_get_instance_private( OFA_ACCOUNT_FILTER_VV_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_filter_vv_bin_parent_class )->dispose( instance );
}

static void
ofa_account_filter_vv_bin_init( ofaAccountFilterVVBin *self )
{
	static const gchar *thisfn = "ofa_account_filter_vv_bin_init";
	ofaAccountFilterVVBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_FILTER_VV_BIN( self ));

	priv = ofa_account_filter_vv_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_account_filter_vv_bin_class_init( ofaAccountFilterVVBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_filter_vv_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_filter_vv_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_filter_vv_bin_finalize;
}

/**
 * ofa_account_filter_vv_bin_new:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: a newly allocated #ofaAccountFilterVVBin object.
 */
ofaAccountFilterVVBin *
ofa_account_filter_vv_bin_new( ofaIGetter *getter )
{
	ofaAccountFilterVVBin *bin;
	ofaAccountFilterVVBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = g_object_new( OFA_TYPE_ACCOUNT_FILTER_VV_BIN, NULL );

	priv = ofa_account_filter_vv_bin_get_instance_private( bin );

	priv->getter = getter;

	ofa_iaccount_filter_setup_bin( OFA_IACCOUNT_FILTER( bin ), getter, st_resource_ui );

	return( bin );
}

static void
iaccount_filter_iface_init( ofaIAccountFilterInterface *iface )
{
	static const gchar *thisfn = "ofa_date_filter_iaccount_filter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}
