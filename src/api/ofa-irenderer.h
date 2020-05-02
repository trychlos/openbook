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

#ifndef __OPENBOOK_API_OFA_IRENDERER_H__
#define __OPENBOOK_API_OFA_IRENDERER_H__

/**
 * SECTION: irenderer
 * @title: ofaIRenderer
 * @short_description: The IRenderer Interface
 * @include: api/ofa-irenderer.h
 *
 * The #ofaIRenderer interface lets plugins interact with the #ofaIRenderable
 * implementations.
 */

#include <glib-object.h>

#include "api/ofa-irenderable.h"

G_BEGIN_DECLS

#define OFA_TYPE_IRENDERER                      ( ofa_irenderer_get_type())
#define OFA_IRENDERER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IRENDERER, ofaIRenderer ))
#define OFA_IS_IRENDERER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IRENDERER ))
#define OFA_IRENDERER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IRENDERER, ofaIRendererInterface ))

typedef struct _ofaIRenderer                    ofaIRenderer;

/**
 * ofaIRendererInterface:
 *
 * This defines the interface that an #ofaIRenderer should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint         ( *get_interface_version )   ( void );

	/**
	 * begin_render:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 *
	 * This method is called by the #ofaIRenderable interface when about
	 * to begin the pagination, just after having called its own
	 * ofaIRenderable::begin_render() method.
	 *
	 * The #ofaIRenderer implementation may take advantage of this method
	 * to do its own initialization.
	 *
	 * Please note that all known #ofaIRenderer implementations are called
	 * by the #ofaIRenderable interface.
	 */
	void          ( *begin_render )            ( ofaIRenderer *instance,
													ofaIRenderable *renderable );

	/**
	 * render_page:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
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
	gboolean      ( *render_page )             ( ofaIRenderer *instance,
													ofaIRenderable *renderable );

	/**
	 * end_render:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
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
	void          ( *end_render )              ( ofaIRenderer *instance,
													ofaIRenderable *renderable );

	/**
	 * draw_page_header_dossier:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 *
	 * Draw the dossier data in the page.
	 *
	 * Returns: %TRUE if the drawing has been done, %FALSE to let the
	 * interface calls other implementations.
	 *
	 * Since: version 1.
	 */
	gboolean      ( *draw_page_header_dossier )( ofaIRenderer *instance,
													ofaIRenderable *renderable );

	/**
	 * get_dossier_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_dossier_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_dossier_font )        ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													guint page_num );

	/**
	 * get_dossier_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_dossier_color )       ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * get_title_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_title_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_title_font )          ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													guint page_num );

	/**
	 * get_title_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_title_color )         ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * get_columns_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_columns_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_columns_font )        ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													guint page_num );

	/**
	 * get_columns_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_columns_color )       ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * get_summary_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_summary_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_summary_font )        ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													guint page_num );

	/**
	 * get_summary_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_summary_color )       ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * get_group_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_group_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_group_font )          ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													guint page_num );

	/**
	 * get_group_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_group_color )         ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * get_report_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @page_num: the page number, counted from zero.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_report_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_report_font )         ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													guint page_num );

	/**
	 * get_report_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_report_color )        ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * get_body_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_body_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_body_font )           ( ofaIRenderer *instance,
													const ofaIRenderable *renderable );

	/**
	 * get_body_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_body_color )          ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );

	/**
	 * draw_page_footer:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 *
	 * Draw the footer of the page.
	 *
	 * Returns: %TRUE if the drawing has been done, %FALSE to let the
	 * interface calls other implementations.
	 *
	 * Since: version 1.
	 */
	gboolean ( *draw_page_footer )        ( ofaIRenderer *instance,
												ofaIRenderable *renderable );

	/**
	 * get_footer_font:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 *
	 * Returns: the name of the font to be used to draw the dosssier header.
	 *
	 * The #ofaIRenderable interface will call each #ofaIRenderer
	 * implementation until the first which returns a non-empty font name.
	 *
	 * It will try its own ofaIRenderable::get_footer_font() method only
	 * if no #ofaIRenderer implementation has returned something.
	 */
	const gchar * ( *get_footer_font )         ( ofaIRenderer *instance,
													const ofaIRenderable *renderable );

	/**
	 * get_footer_color:
	 * @instance: the #ofaIRenderer instance.
	 * @renderable: the #ofaIRenderable caller.
	 * @r: [out]: a placeholder for the red composante.
	 * @g: [out]: a placeholder for the green composante.
	 * @b: [out]: a placeholder for the blue composante.
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
	gboolean      ( *get_footer_color )        ( ofaIRenderer *instance,
													const ofaIRenderable *renderable,
													gdouble *r,
													gdouble *g,
													gdouble *b );
}
	ofaIRendererInterface;

/*
 * Interface-wide
 */
GType        ofa_irenderer_get_type                  ( void );

guint        ofa_irenderer_get_interface_last_version( const ofaIRenderer *instance );

/*
 * Implementation-wide
 */
guint        ofa_irenderer_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void         ofa_irenderer_begin_render              ( ofaIRenderer *instance,
															ofaIRenderable *renderable );

gboolean     ofa_irenderer_render_page               ( ofaIRenderer *instance,
															ofaIRenderable *renderable );

void         ofa_irenderer_end_render                ( ofaIRenderer *instance,
															ofaIRenderable *renderable );

gboolean     ofa_irenderer_draw_page_header_dossier  ( ofaIRenderer *instance,
															ofaIRenderable *renderable );

const gchar *ofa_irenderer_get_dossier_font          ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															guint page_num );

gboolean     ofa_irenderer_get_dossier_color         ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

const gchar *ofa_irenderer_get_title_font            ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															guint page_num );

gboolean     ofa_irenderer_get_title_color           ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

const gchar *ofa_irenderer_get_columns_font          ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															guint page_num );

gboolean     ofa_irenderer_get_columns_color         ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

const gchar *ofa_irenderer_get_summary_font          ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															guint page_num );

gboolean     ofa_irenderer_get_summary_color         ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

const gchar *ofa_irenderer_get_group_font            ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															guint page_num );

gboolean     ofa_irenderer_get_group_color           ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

const gchar *ofa_irenderer_get_report_font           ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															guint page_num );

gboolean     ofa_irenderer_get_report_color          ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

const gchar *ofa_irenderer_get_body_font             ( ofaIRenderer *instance,
															const ofaIRenderable *renderable );

gboolean     ofa_irenderer_get_body_color            ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

gboolean     ofa_irenderer_draw_page_footer          ( ofaIRenderer *instance,
															ofaIRenderable *renderable );

const gchar *ofa_irenderer_get_footer_font           ( ofaIRenderer *instance,
															const ofaIRenderable *renderable );

gboolean     ofa_irenderer_get_footer_color          ( ofaIRenderer *instance,
															const ofaIRenderable *renderable,
															gdouble *r,
															gdouble *g,
															gdouble *b );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IRENDERER_H__ */
