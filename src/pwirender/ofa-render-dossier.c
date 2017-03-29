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

#include <glib/gi18n.h>

#include "api/ofa-hub.h"
#include "api/ofa-iextender-setter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irenderable.h"
#include "api/ofa-irenderer.h"
#include "api/ofo-dossier.h"

#include "pwirender/ofa-render-dossier.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	ofaIGetter *getter;
}
	ofaRenderDossierPrivate;

static void        iextender_setter_iface_init( ofaIExtenderSetterInterface *iface );
static ofaIGetter *iextender_setter_get_getter( ofaIExtenderSetter *instance );
static void        iextender_setter_set_getter( ofaIExtenderSetter *instance, ofaIGetter *getter );
static void        irenderer_iface_init( ofaIRendererInterface *iface );
static void        irenderer_draw_page_header_dossier( ofaIRenderer *instance, ofaIRenderable *renderable );

G_DEFINE_TYPE_EXTENDED( ofaRenderDossier, ofa_render_dossier, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaRenderDossier )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXTENDER_SETTER, iextender_setter_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRENDERER, irenderer_iface_init ))

static void
render_dossier_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_render_dossier_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RENDER_DOSSIER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_dossier_parent_class )->finalize( instance );
}

static void
render_dossier_dispose( GObject *instance )
{
	ofaRenderDossierPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RENDER_DOSSIER( instance ));

	priv = ofa_render_dossier_get_instance_private( OFA_RENDER_DOSSIER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_dossier_parent_class )->dispose( instance );
}

static void
ofa_render_dossier_init( ofaRenderDossier *self )
{
	static const gchar *thisfn = "ofa_render_dossier_init";
	ofaRenderDossierPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_render_dossier_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_render_dossier_class_init( ofaRenderDossierClass *klass )
{
	static const gchar *thisfn = "ofa_render_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = render_dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = render_dossier_finalize;
}

/*
 * #ofaIExtenderSetter interface setup
 */
static void
iextender_setter_iface_init( ofaIExtenderSetterInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dbprovider_iextender_setter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_getter = iextender_setter_get_getter;
	iface->set_getter = iextender_setter_set_getter;
}

static ofaIGetter *
iextender_setter_get_getter( ofaIExtenderSetter *instance )
{
	ofaRenderDossierPrivate *priv;

	priv = ofa_render_dossier_get_instance_private( OFA_RENDER_DOSSIER( instance ));

	return( priv->getter );
}

static void
iextender_setter_set_getter( ofaIExtenderSetter *instance, ofaIGetter *getter )
{
	ofaRenderDossierPrivate *priv;

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	priv = ofa_render_dossier_get_instance_private( OFA_RENDER_DOSSIER( instance ));

	priv->getter = getter;
}

/*
 * #ofaIRenderer interface setup
 */
static void
irenderer_iface_init( ofaIRendererInterface *iface )
{
	static const gchar *thisfn = "ofa_render_dossier_irenderer_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->draw_page_header_dossier = irenderer_draw_page_header_dossier;
}

static void
irenderer_draw_page_header_dossier( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	ofaRenderDossierPrivate *priv;
	gdouble r, g, b, y, height, lh, x;
	ofaHub *hub;
	ofoDossier *dossier;
	gchar *str;

	priv = ofa_render_dossier_get_instance_private( OFA_RENDER_DOSSIER( instance ));

	ofa_irenderable_get_dossier_color( renderable, &r, &g, &b );
	ofa_irenderable_set_color( renderable, r, g, b );
	ofa_irenderable_set_font( renderable, ofa_irenderable_get_dossier_font( renderable, 0 ));
	y = 0;

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );

	height = ofa_irenderable_set_text( renderable, 0, y,
			ofo_dossier_get_label( dossier ), PANGO_ALIGN_LEFT );
	y += height;

	ofa_irenderable_set_font( renderable, "Sans Italic 4" );
	lh = ofa_irenderable_get_text_height( renderable );

	x = ofa_irenderable_get_render_width( renderable );
	x /= 2.0;
	y = ofa_irenderable_get_render_height( renderable );
	y -= lh;
	str = g_strdup_printf( _( "SIRET %s" ), ofo_dossier_get_siret( dossier ));
	ofa_irenderable_set_text( renderable, x, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	ofa_irenderable_set_last_y( renderable, height );
}
