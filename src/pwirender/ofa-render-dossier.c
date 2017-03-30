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

#include "my/my-utils.h"

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

#define COLOR_HEADER_DOSSIER            0.5,    0,      0	/* dark red */

static const gchar  *st_line1_font      = "Sans Bold Italic 10";
static const gchar  *st_line2_font      = "Sans 6";
static const gchar  *st_bottom_font     = "Sans Italic 4";

static void        iextender_setter_iface_init( ofaIExtenderSetterInterface *iface );
static ofaIGetter *iextender_setter_get_getter( ofaIExtenderSetter *instance );
static void        iextender_setter_set_getter( ofaIExtenderSetter *instance, ofaIGetter *getter );
static void        irenderer_iface_init( ofaIRendererInterface *iface );
static gboolean    irenderer_draw_page_header_dossier( ofaIRenderer *instance, ofaIRenderable *renderable );
static gboolean    irenderer_draw_page_footer( ofaIRenderer *instance, ofaIRenderable *renderable );

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
	iface->draw_page_footer = irenderer_draw_page_footer;
}

/*
 * Draw the two dossiers lines at the top of the page
 */
static gboolean
irenderer_draw_page_header_dossier( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	ofaRenderDossierPrivate *priv;
	gdouble y, text_height;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *cstr;

	priv = ofa_render_dossier_get_instance_private( OFA_RENDER_DOSSIER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );

	ofa_irenderable_set_color( renderable, COLOR_HEADER_DOSSIER );

	/* header with lines 1 and 2 of the dossier */
	y = 0;
	ofa_irenderable_set_font( renderable, st_line1_font );
	text_height = ofa_irenderable_get_text_height( renderable );
	cstr = ofo_dossier_get_label( dossier );
	if( cstr ){
		ofa_irenderable_set_text( renderable, 0, y, cstr , PANGO_ALIGN_LEFT );
	}
	y += text_height;

	ofa_irenderable_set_font( renderable, st_line2_font );
	cstr = ofo_dossier_get_label2( dossier );
	if( cstr ){
		ofa_irenderable_set_text( renderable, 0, y, cstr , PANGO_ALIGN_LEFT );
	}

	ofa_irenderable_set_last_y( renderable, text_height );

	return( TRUE );
}

/*
 * Draw administrative properties on the page footer.
 * The line can be put before or after the separation line.
 */
static gboolean
irenderer_draw_page_footer( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	ofaRenderDossierPrivate *priv;
	gdouble y, text_height, x;
	ofaHub *hub;
	ofoDossier *dossier;
	const gchar *cstr;
	GString *gstr;
	gboolean before;

	priv = ofa_render_dossier_get_instance_private( OFA_RENDER_DOSSIER( instance ));

	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	before = FALSE;

	ofa_irenderable_set_color( renderable, COLOR_HEADER_DOSSIER );

	/* bottom line
	 * under the bottom separation line: does not take any vertical space */
	ofa_irenderable_set_font( renderable, st_bottom_font );
	text_height = ofa_irenderable_get_text_height( renderable );

	/* if drawn before, have a blank line */
	if( before ){
		y = ofa_irenderable_get_last_y( renderable );
		y += text_height;

	} else {
		y = ofa_irenderable_get_render_height( renderable ) - text_height;
	}

	gstr = g_string_new( "" );

	cstr = ofo_dossier_get_siret( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( gstr, "SIRET %s", cstr );
	}
	cstr = ofo_dossier_get_vatic( dossier );
	if( my_strlen( cstr )){
		if( gstr->len ){
			gstr = g_string_append( gstr, " - " );
		}
		g_string_append_printf( gstr, "VAT %s", cstr );
	}
	cstr = ofo_dossier_get_naf( dossier );
	if( my_strlen( cstr )){
		if( gstr->len ){
			gstr = g_string_append( gstr, " - " );
		}
		g_string_append_printf( gstr, "NAF %s", cstr );
	}

	x = ofa_irenderable_get_render_width( renderable );
	x /= 2.0;
	ofa_irenderable_set_text( renderable, x, y, gstr->str, PANGO_ALIGN_CENTER );
	g_string_free( gstr, TRUE );

	if( before ){
		y += text_height;
		ofa_irenderable_set_last_y( renderable, y );
	}

	ofa_irenderable_draw_default_page_footer( renderable );

	return( TRUE );
}
