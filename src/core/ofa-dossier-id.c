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
#include "api/ofa-idbms.h"
#include "api/ofa-ifile-id.h"

#include "ofa-dossier-id.h"

/* priv instance data
 */
struct _ofaDossierIdPrivate {
	gboolean  dispose_has_run;

	/* runtime data
	 */
	gchar    *dos_name;
	gchar    *prov_name;
	ofaIDbms *prov_instance;
	GList    *periods;
};

static void      ifile_id_iface_init( ofaIFileIdInterface *iface );
static guint     ifile_id_get_interface_version( const ofaIFileId *instance );
static gchar    *ifile_id_get_dossier_name( const ofaIFileId *instance );
static gchar    *ifile_id_get_provider_name( const ofaIFileId *instance );
static ofaIDbms *ifile_id_get_provider_instance( const ofaIFileId *instance );
static GList    *ifile_id_get_periods( const ofaIFileId *instance );

G_DEFINE_TYPE_EXTENDED( ofaDossierId, ofa_dossier_id, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IFILE_ID, ifile_id_iface_init ));

static void
dossier_id_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_id_finalize";
	ofaDossierIdPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_ID( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_ID( instance )->priv;

	g_free( priv->dos_name );
	g_free( priv->prov_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_id_parent_class )->finalize( instance );
}

static void
dossier_id_dispose( GObject *instance )
{
	ofaDossierIdPrivate *priv;

	priv = OFA_DOSSIER_ID( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->prov_instance );
		ofa_ifile_id_free_periods( priv->periods );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_id_parent_class )->dispose( instance );
}

static void
ofa_dossier_id_init( ofaDossierId *self )
{
	static const gchar *thisfn = "ofa_dossier_id_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_ID, ofaDossierIdPrivate );
}

static void
ofa_dossier_id_class_init( ofaDossierIdClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_id_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_id_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_id_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierIdPrivate ));
}

/*
 * ofaIFileId interface management
 */
static void
ifile_id_iface_init( ofaIFileIdInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_id_ifile_id_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ifile_id_get_interface_version;
	iface->get_dossier_name = ifile_id_get_dossier_name;
	iface->get_provider_name = ifile_id_get_provider_name;
	iface->get_provider_instance = ifile_id_get_provider_instance;
	iface->get_periods = ifile_id_get_periods;
}

static guint
ifile_id_get_interface_version( const ofaIFileId *instance )
{
	return( 1 );
}

static gchar *
ifile_id_get_dossier_name( const ofaIFileId *instance )
{
	ofaDossierIdPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_ID( instance ), NULL );

	priv = OFA_DOSSIER_ID( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->dos_name ));
	}

	return( NULL );
}

static gchar *
ifile_id_get_provider_name( const ofaIFileId *instance )
{
	ofaDossierIdPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_ID( instance ), NULL );

	priv = OFA_DOSSIER_ID( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_strdup( priv->prov_name ));
	}

	return( NULL );
}

static ofaIDbms *
ifile_id_get_provider_instance( const ofaIFileId *instance )
{
	ofaDossierIdPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_ID( instance ), NULL );

	priv = OFA_DOSSIER_ID( instance )->priv;

	if( !priv->dispose_has_run ){
		return( g_object_ref( priv->prov_instance ));
	}

	return( NULL );
}

static GList *
ifile_id_get_periods( const ofaIFileId *instance )
{
#if 0
	ofaDossierIdPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_DOSSIER_ID( instance ), NULL );

	priv = OFA_DOSSIER_ID( instance )->priv;

	if( !priv->dispose_has_run ){
		return( ofa_idbms_get_periods( priv->prov_instance, priv->dos_name, priv->collection ));
	}
#endif

	return( NULL );
}

/**
 * ofa_dossier_id_new:
 * @collection: the #ofaIFileCollection this creation originates from.
 *
 * Returns: a new reference to a #ofaDossierId instance.
 *
 * The returned reference should be #g_object_unref() by the caller.
 */
ofaDossierId *
ofa_dossier_id_new( void )
{
	ofaDossierId *id;

	//g_return_val_if_fail( collection && OFA_IS_IFILE_COLLECTION( collection ), NULL );
	//g_return_val_if_fail( collection && OFA_IS_DOSSIER_DIR( collection ), NULL );

	id = g_object_new( OFA_TYPE_DOSSIER_ID, NULL );
	//id->priv->collection = collection;

	return( id );
}

/**
 * ofa_dossier_id_set_dossier_name:
 * @instance: this #ofaDossierId instance.
 * @name: the identifier name of the dossier.
 *
 * Set the name of the dossier.
 */
void
ofa_dossier_id_set_dossier_name( ofaDossierId *id, const gchar *name )
{
	ofaDossierIdPrivate *priv;

	g_return_if_fail( id && OFA_IS_DOSSIER_ID( id ));

	priv = id->priv;

	if( !priv->dispose_has_run ){
		g_free( priv->dos_name );
		priv->dos_name = g_strdup( name );
	}
}

/**
 * ofa_dossier_id_set_provider_name:
 * @instance: this #ofaDossierId instance.
 * @name: the provider name for the dossier.
 *
 * Set the name of the provider.
 */
void
ofa_dossier_id_set_provider_name( ofaDossierId *id, const gchar *name )
{
	ofaDossierIdPrivate *priv;

	g_return_if_fail( id && OFA_IS_DOSSIER_ID( id ));

	priv = id->priv;

	if( !priv->dispose_has_run ){

		g_free( priv->prov_name );
		priv->prov_name = g_strdup( name );

		g_clear_object( &priv->prov_instance );
		if( my_strlen( name )){
			priv->prov_instance = ofa_idbms_get_instance_by_name( priv->prov_name );
		}
	}
}
