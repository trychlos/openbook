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
	guint ( *get_interface_version )   ( void );

	/**
	 * draw_page_header_dossier:
	 *
	 * Draw the dossier data in the page.
	 *
	 * Since: version 1.
	 */
	void  ( *draw_page_header_dossier )( ofaIRenderer *instance,
											ofaIRenderable *renderable );

	/**
	 * draw_page_footer:
	 *
	 * Draw the footer of the page.
	 *
	 * Since: version 1.
	 */
	void  ( *draw_page_footer )        ( ofaIRenderer *instance,
											ofaIRenderable *renderable );
}
	ofaIRendererInterface;

/*
 * Interface-wide
 */
GType    ofa_irenderer_get_type                  ( void );

guint    ofa_irenderer_get_interface_last_version( const ofaIRenderer *instance );

/*
 * Implementation-wide
 */
guint    ofa_irenderer_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gboolean ofa_irenderer_draw_page_header_dossier  ( ofaIRenderer *instance,
														ofaIRenderable *renderable );

gboolean ofa_irenderer_draw_page_footer          ( ofaIRenderer *instance,
														ofaIRenderable *renderable );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IRENDERER_H__ */
