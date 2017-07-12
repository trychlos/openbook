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

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "dgifec/ofa-fec-ident.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaFecIdentPrivate;

static void   iident_iface_init( myIIdentInterface *iface );
static gchar *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar *iident_get_version( const myIIdent *instance, void *user_data );

G_DEFINE_TYPE_EXTENDED( ofaFecIdent, ofa_fec_ident, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaFecIdent )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init ))

static void
fec_ident_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_fec_ident_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FEC_IDENT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_fec_ident_parent_class )->finalize( instance );
}

static void
fec_ident_dispose( GObject *instance )
{
	ofaFecIdentPrivate *priv;

	g_return_if_fail( instance && OFA_IS_FEC_IDENT( instance ));

	priv = ofa_fec_ident_get_instance_private( OFA_FEC_IDENT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_fec_ident_parent_class )->dispose( instance );
}

static void
ofa_fec_ident_init( ofaFecIdent *self )
{
	static const gchar *thisfn = "ofa_fec_ident_init";
	ofaFecIdentPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_fec_ident_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_fec_ident_class_init( ofaFecIdentClass *klass )
{
	static const gchar *thisfn = "ofa_fec_ident_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = fec_ident_dispose;
	G_OBJECT_CLASS( klass )->finalize = fec_ident_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_fec_ident_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "DGI FEC exoprter" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( PACKAGE_VERSION ));
}
