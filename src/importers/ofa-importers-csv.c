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

#include "importers/ofa-importers-csv.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImportersCSVPrivate;

static GList *st_accepted_contents      = NULL;

static void         iident_iface_init( myIIdentInterface *iface );
static gchar       *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar       *iident_get_version( const myIIdent *instance, void *user_data );
static void         iimporter_iface_init( ofaIImporterInterface *iface );
static gchar       *iimporter_get_label( const ofaIImporter *instance );
static const GList *iimporter_get_accepted_contents( const ofaIImporter *instance );

G_DEFINE_TYPE_EXTENDED( ofaImportersCSV, ofa_importers_csv, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaImportersCSV )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

static void
importers_csv_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importers_csv_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_IMPORTERS_CSV( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importers_csv_parent_class )->finalize( instance );
}

static void
importers_csv_dispose( GObject *instance )
{
	ofaImportersCSVPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTERS_CSV( instance ));

	priv = ofa_importers_csv_get_instance_private( OFA_IMPORTERS_CSV( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importers_csv_parent_class )->dispose( instance );
}

static void
ofa_importers_csv_init( ofaImportersCSV *self )
{
	static const gchar *thisfn = "ofa_importers_csv_init";
	ofaImportersCSVPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTERS_CSV( self ));

	priv = ofa_importers_csv_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importers_csv_class_init( ofaImportersCSVClass *klass )
{
	static const gchar *thisfn = "ofa_importers_csv_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importers_csv_dispose;
	G_OBJECT_CLASS( klass )->finalize = importers_csv_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_importers_csv_iident_iface_init";

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
	static const gchar *thisfn = "ofa_importers_csv_iimporter_iface_init";

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
	if( !st_accepted_contents ){
		st_accepted_contents = g_list_prepend( NULL, "text/csv" );
	}

	return( st_accepted_contents );
}
