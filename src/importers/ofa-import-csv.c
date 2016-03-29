/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "my/my-iident.h"

#include "api/ofa-iimporter.h"

#include "importers/ofa-import-csv.h"

/* private instance data
 */
struct _ofaImportCSVPrivate {
	gboolean dispose_has_run;
};

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void         instance_finalize( GObject *object );
static void         instance_dispose( GObject *object );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         class_init( ofaImportCSVClass *klass );
static void         iident_iface_init( myIIdentInterface *iface );
static gchar       *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar       *iident_get_version( const myIIdent *instance, void *user_data );
static void         iimporter_iface_init( ofaIImporterInterface *iface );
static gchar       *iimporter_get_label( const ofaIImporter *instance );
static const GList *iimporter_get_accepted_contents( const ofaIImporter *instance );

GType
ofa_import_csv_get_type( void )
{
	return( st_module_type );
}

void
ofa_import_csv_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_import_csv_register_type";

	static GTypeInfo info = {
		sizeof( ofaImportCSVClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaImportCSV ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iident_iface_info = {
		( GInterfaceInitFunc ) iident_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iimporter_iface_info = {
		( GInterfaceInitFunc ) iimporter_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaImportCSV", &info, 0 );

	g_type_module_add_interface( module, st_module_type, MY_TYPE_IIDENT, &iident_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IIMPORTER, &iimporter_iface_info );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_import_csv_instance_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && OFA_IS_IMPORT_CSV( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
instance_dispose( GObject *object )
{
	ofaImportCSVPrivate *priv;

	g_return_if_fail( object && OFA_IS_IMPORT_CSV( object ));

	priv = OFA_IMPORT_CSV( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_import_csv_instance_init";
	ofaImportCSV *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) klass );

	g_return_if_fail( instance && OFA_IS_IMPORT_CSV( instance ));

	self = OFA_IMPORT_CSV( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_IMPORT_CSV, ofaImportCSVPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
class_init( ofaImportCSVClass *klass )
{
	static const gchar *thisfn = "ofa_import_csv_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaImportCSVPrivate ));
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_import_csv_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "Text/CSV importer" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "v 2016.1" ));
}

/*
 * ofaIImporter interface management
 */
static void
iimporter_iface_init( ofaIImporterInterface *iface )
{
	static const gchar *thisfn = "ofa_import_csv_iimporter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_label = iimporter_get_label;
	iface->get_accepted_contents = iimporter_get_accepted_contents;
}

static gchar *
iimporter_get_label( const ofaIImporter *instance )
{
	return( iident_get_canon_name( MY_IIDENT( instance ), NULL ));
}

static const GList *
iimporter_get_accepted_contents( const ofaIImporter *instance )
{
	GList *content;

	content = g_list_prepend( NULL, "text/csv" );

	return( content );
}
