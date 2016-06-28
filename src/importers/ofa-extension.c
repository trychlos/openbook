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

#include "my/my-iident.h"

#include "api/ofa-extension.h"

#include "importers/ofa-importer-csv.h"
#include "importers/ofa-importer-pdf-bourso.h"
#include "importers/ofa-importer-pdf-lcl.h"
#include "importers/ofa-importer-txt-bourso.h"
#include "importers/ofa-importer-txt-lcl.h"

/*
 * The part below defines and implements the GTypeModule-derived class
 *  for this library.
 * See infra for the software extension API implementation.
 */
#define OFA_TYPE_IMPORTERS_ID                ( ofa_importers_id_get_type())
#define OFA_IMPORTERS_ID( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTERS_ID, ofaImportersId ))
#define OFA_IMPORTERS_ID_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTERS_ID, ofaImportersIdClass ))
#define OFA_IS_IMPORTERS_ID( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTERS_ID ))
#define OFA_IS_IMPORTERS_ID_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTERS_ID ))
#define OFA_IMPORTERS_ID_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTERS_ID, ofaImportersIdClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaImportersId;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaImportersIdClass;

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImportersIdPrivate;

GType ofa_importers_id_get_type( void );

static void   iident_iface_init( myIIdentInterface *iface );
static gchar *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar *iident_get_version( const myIIdent *instance, void *user_data );

G_DEFINE_DYNAMIC_TYPE_EXTENDED( ofaImportersId, ofa_importers_id, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE_DYNAMIC( ofaImportersId )
		G_IMPLEMENT_INTERFACE_DYNAMIC( MY_TYPE_IIDENT, iident_iface_init ))

static void
ofa_importers_id_class_finalize( ofaImportersIdClass *klass )
{
	static const gchar *thisfn = "ofa_importers_id_class_finalize";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
}

static void
importers_id_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importers_id_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_IMPORTERS_ID( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importers_id_parent_class )->finalize( instance );
}

static void
importers_id_dispose( GObject *instance )
{
	ofaImportersIdPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTERS_ID( instance ));

	priv = ofa_importers_id_get_instance_private( OFA_IMPORTERS_ID( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importers_id_parent_class )->dispose( instance );
}

static void
ofa_importers_id_init( ofaImportersId *self )
{
	static const gchar *thisfn = "ofa_importers_id_init";
	ofaImportersIdPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_importers_id_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importers_id_class_init( ofaImportersIdClass *klass )
{
	static const gchar *thisfn = "ofa_importers_id_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importers_id_dispose;
	G_OBJECT_CLASS( klass )->finalize = importers_id_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_importers_id_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "Stock importers" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "v 2016.1" ));
}

/*
 * The part below implements the software extension API
 */

/*
 * The count of GType types provided by this extension.
 * Each of these GType types must be addressed in #ofa_extension_list_types().
 * Only the GTypeModule has to be registered from #ofa_extension_startup().
 */
#define TYPES_COUNT	 6

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, ofaIGetter *getter )
{
	static const gchar *thisfn = "importers/ofa_extension_startup";

	g_debug( "%s: module=%p, getter=%p", thisfn, ( void * ) module, ( void * ) getter  );

	ofa_importers_id_register_type( module );

	return( TRUE );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with API v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "importers/ofa_extension_list_types";
	static GType types_list [1+TYPES_COUNT];
	gint i = 0;

	g_debug( "%s: types=%p, count=%u", thisfn, ( void * ) types, TYPES_COUNT );

	types_list[i++] = OFA_TYPE_IMPORTERS_ID;
	types_list[i++] = OFA_TYPE_IMPORTER_CSV;
	types_list[i++] = OFA_TYPE_IMPORTER_PDF_BOURSO;
	types_list[i++] = OFA_TYPE_IMPORTER_PDF_LCL;
	types_list[i++] = OFA_TYPE_IMPORTER_TXT_BOURSO;
	types_list[i++] = OFA_TYPE_IMPORTER_TXT_LCL;

	g_return_val_if_fail( i == TYPES_COUNT, 0 );
	types_list[i] = 0;
	*types = types_list;

	return( TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * optional as of API v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "importers/ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}
