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

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-iexe-closeable.h"

#include "ofa-tva-execloseable.h"
#include "ofa-tva-ident.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaTVAIdentPrivate;

static void   iident_iface_init( myIIdentInterface *iface );
static gchar *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar *iident_get_version( const myIIdent *instance, void *user_data );

G_DEFINE_TYPE_EXTENDED( ofaTVAIdent, ofa_tva_ident, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaTVAIdent )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXECLOSEABLE, ofa_tva_execloseable_iface_init ))

static void
tva_ident_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_ident_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_IDENT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_ident_parent_class )->finalize( instance );
}

static void
tva_ident_dispose( GObject *instance )
{
	ofaTVAIdentPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_IDENT( instance ));

	priv = ofa_tva_ident_get_instance_private( OFA_TVA_IDENT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_ident_parent_class )->dispose( instance );
}

static void
ofa_tva_ident_init( ofaTVAIdent *self )
{
	static const gchar *thisfn = "ofa_tva_ident_init";
	ofaTVAIdentPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tva_ident_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_tva_ident_class_init( ofaTVAIdentClass *klass )
{
	static const gchar *thisfn = "ofa_tva_ident_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_ident_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_ident_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_ident_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "VAT declarations" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( PACKAGE_VERSION ));
}
