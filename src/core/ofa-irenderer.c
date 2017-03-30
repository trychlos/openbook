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

#include "api/ofa-irenderer.h"

/* data associated to each implementor object
 */
typedef struct {
	void *empty;
}
	sIRenderer;

#define IRENDERER_LAST_VERSION         1
#define IRENDERER_DATA                 "ofa-irenderer-data"

static guint st_initializations         =   0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( ofaIRendererInterface *klass );
static void        interface_base_finalize( ofaIRendererInterface *klass );
static sIRenderer *get_instance_data( const ofaIRenderer *instance );
static void        on_instance_finalized( sIRenderer *sdata, void *instance );

/**
 * ofa_irenderer_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_irenderer_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_irenderer_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_irenderer_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIRendererInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIRenderer", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIRendererInterface *klass )
{
	static const gchar *thisfn = "ofa_irenderer_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIRendererInterface *klass )
{
	static const gchar *thisfn = "ofa_irenderer_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_irenderer_get_interface_last_version:
 * @instance: this #ofaIRenderer instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_irenderer_get_interface_last_version( const ofaIRenderer *instance )
{
	return( IRENDERER_LAST_VERSION );
}

/**
 * ofa_irenderer_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_irenderer_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IRENDERER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIRendererInterface * ) iface )->get_interface_version ){
		version = (( ofaIRendererInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIRenderer::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_irenderer_draw_page_header_dossier:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * Returns: %TRUE if the interface implements the method and has executed it.
 */
gboolean
ofa_irenderer_draw_page_header_dossier( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->draw_page_header_dossier ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->draw_page_header_dossier( instance, renderable ));
		return( TRUE );
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_draw_page_footer:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * Returns: %TRUE if the interface implements the method and has executed it.
 */
gboolean
ofa_irenderer_draw_page_footer( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->draw_page_footer ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->draw_page_footer( instance, renderable ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

static sIRenderer *
get_instance_data( const ofaIRenderer *instance )
{
	sIRenderer *sdata;

	sdata = ( sIRenderer * ) g_object_get_data( G_OBJECT( instance ), IRENDERER_DATA );

	if( !sdata ){
		sdata = g_new0( sIRenderer, 1 );
		g_object_set_data( G_OBJECT( instance ), IRENDERER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sIRenderer *sdata, void *instance )
{
	static const gchar *thisfn = "ofa_irenderer_on_instance_finalized";

	g_debug( "%s: sdata=%p, instance=%p", thisfn, ( void * ) sdata, ( void * ) instance );

	g_free( sdata );
}
