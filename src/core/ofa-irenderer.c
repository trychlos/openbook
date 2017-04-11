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
 * ofa_irenderer_begin_render:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * This method is called by the #ofaIRenderable interface when about to
 * begin the pagination, just after having called its own
 * ofaIRenderable::begin_render() method.
 *
 * The #ofaIRenderer implementation may take advantage of this method to
 * do its own initialization.
 *
 * Please note that all known #ofaIRenderer implementations are called
 * by the #ofaIRenderable interface.
 */
void
ofa_irenderer_begin_render( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERER( instance ));
	g_return_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ));

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->begin_render ){
		OFA_IRENDERER_GET_INTERFACE( instance )->begin_render( instance, renderable );
		return;
	}

	sdata->empty = NULL;
}

/**
 * ofa_irenderer_render_page:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * This method is called by the #ofaIRenderable interface to draw a page.
 *
 * Returns: %TRUE if the @instance has drawn the page, %FALSE to let the
 * #ofaIRenderable interface calls other implementations.
 *
 * If no #ofaIRenderer implementation returns %TRUE, then the
 * ofaIRenderable::render_page() method is called. It this later is not
 * implemented, then #ofaIRenderable interface defaults to draw the page
 * on the provided cairo context.
 */
gboolean
ofa_irenderer_render_page( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->render_page ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->render_page( instance, renderable ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_end_render:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * This method is called by the #ofaIRenderable interface after having
 * rendered all pages, just before calling its own
 * ofaIRenderable::end_render() method.
 *
 * The #ofaIRenderer implementation may take advantage of this method to
 * free its allocated resources.
 *
 * Please note that all known #ofaIRenderer implementations are called
 * by the #ofaIRenderable interface.
 */
void
ofa_irenderer_end_render( ofaIRenderer *instance, ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_if_fail( instance && OFA_IS_IRENDERER( instance ));
	g_return_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ));

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->end_render ){
		OFA_IRENDERER_GET_INTERFACE( instance )->end_render( instance, renderable );
		return;
	}

	sdata->empty = NULL;
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
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_dossier_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @page_num: the number of the page to be rendered, counted from zero.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_dossier_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_dossier_font( ofaIRenderer *instance, const ofaIRenderable *renderable, guint page_num )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_dossier_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_dossier_font( instance, renderable, page_num ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_dossier_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_dossier_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_dossier_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_dossier_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_dossier_color( instance, renderable, r, g, b ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_title_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @page_num: the number of the page to be rendered, counted from zero.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_title_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_title_font( ofaIRenderer *instance, const ofaIRenderable *renderable, guint page_num )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_title_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_title_font( instance, renderable, page_num ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_title_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_title_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_title_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_title_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_title_color( instance, renderable, r, g, b ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_columns_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @page_num: the number of the page to be rendered, counted from zero.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_columns_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_columns_font( ofaIRenderer *instance, const ofaIRenderable *renderable, guint page_num )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_columns_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_columns_font( instance, renderable, page_num ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_columns_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_columns_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_columns_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_columns_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_columns_color( instance, renderable, r, g, b ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_summary_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @page_num: the number of the page to be rendered, counted from zero.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_summary_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_summary_font( ofaIRenderer *instance, const ofaIRenderable *renderable, guint page_num )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_summary_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_summary_font( instance, renderable, page_num ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_summary_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_summary_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_summary_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_summary_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_summary_color( instance, renderable, r, g, b ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_group_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @page_num: the number of the page to be rendered, counted from zero.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_group_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_group_font( ofaIRenderer *instance, const ofaIRenderable *renderable, guint page_num )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_group_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_group_font( instance, renderable, page_num ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_group_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_group_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_group_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_group_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_group_color( instance, renderable, r, g, b ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_report_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @page_num: the number of the page to be rendered, counted from zero.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_report_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_report_font( ofaIRenderer *instance, const ofaIRenderable *renderable, guint page_num )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_report_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_report_font( instance, renderable, page_num ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_report_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_report_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_report_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_report_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_report_color( instance, renderable, r, g, b ));
	}

	sdata->empty = NULL;

	return( FALSE );
}

/**
 * ofa_irenderer_get_body_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_body_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_body_font( ofaIRenderer *instance, const ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_body_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_body_font( instance, renderable ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_body_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_body_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_body_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_body_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_body_color( instance, renderable, r, g, b ));
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

/**
 * ofa_irenderer_get_footer_font:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 *
 * Returns: the name of the font to be used to draw the dosssier header.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer implementation
 * until the first which returns a non-empty font name.
 *
 * It will try its own ofaIRenderable::get_footer_font() method only if no
 * #ofaIRenderer implementation has returned something.
 */
const gchar *
ofa_irenderer_get_footer_font( ofaIRenderer *instance, const ofaIRenderable *renderable )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), NULL );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), NULL );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_footer_font ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_footer_font( instance, renderable ));
	}

	sdata->empty = NULL;

	return( NULL );
}

/**
 * ofa_irenderer_get_footer_color:
 * @instance: this #ofaIRenderer instance.
 * @renderable: the #ofaIRenderable target.
 * @r: [out]: a placeholder for red component.
 * @g: [out]: a placeholder for green component.
 * @b: [out]: a placeholder for blue component.
 *
 * Returns: %TRUE if the interface implements the method and has executed
 * it.
 *
 * The #ofaIRenderable interface will call each #ofaIRenderer
 * implementation until the first which returns %TRUE.
 *
 * It will try its own ofaIRenderable::get_footer_color() method only
 * if no #ofaIRenderer implementation has returned something.
 */
gboolean
ofa_irenderer_get_footer_color( ofaIRenderer *instance, const ofaIRenderable *renderable, gdouble *r, gdouble *g, gdouble *b )
{
	sIRenderer *sdata;

	g_return_val_if_fail( instance && OFA_IS_IRENDERER( instance ), FALSE );
	g_return_val_if_fail( renderable && OFA_IS_IRENDERABLE( renderable ), FALSE );

	sdata = get_instance_data( instance );

	if( OFA_IRENDERER_GET_INTERFACE( instance )->get_footer_color ){
		return( OFA_IRENDERER_GET_INTERFACE( instance )->get_footer_color( instance, renderable, r, g, b ));
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
