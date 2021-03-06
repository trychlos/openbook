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

#ifndef __OPENBOOK_API_OFA_RENDER_PAGE_H__
#define __OPENBOOK_API_OFA_RENDER_PAGE_H__

/**
 * SECTION: ofa_render_page
 * @short_description: #ofaRenderPage class definition.
 * @include: api/ofa-render-page.h
 *
 * This is an abstract base class which handled the preview of printings.
 * These printings, after having been displayed, may be printed (or
 * exported as PDF files).
 *
 * This base classe along with its companion interface #ofaIRenderable,
 * which is expected to be implemented by the derived class, both make
 * use of the #GtkPrintOperation operations via the ofaIPrintable
 * interface.
 *
 * It is expected in a future version that these same displayed printings
 * may be saved in the DBMS.
 */

#include "api/ofa-icontext.h"
#include "api/ofa-paned-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_RENDER_PAGE                ( ofa_render_page_get_type())
#define OFA_RENDER_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RENDER_PAGE, ofaRenderPage ))
#define OFA_RENDER_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RENDER_PAGE, ofaRenderPageClass ))
#define OFA_IS_RENDER_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RENDER_PAGE ))
#define OFA_IS_RENDER_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RENDER_PAGE ))
#define OFA_RENDER_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RENDER_PAGE, ofaRenderPageClass ))

typedef struct {
	/*< public members >*/
	ofaPanedPage      parent;
}
	ofaRenderPage;

typedef struct {
	/*< public members >*/
	ofaPanedPageClass parent;

	/*< virtual protected methods >*/
	/**
	 * get_args_widget:
	 * @page: this #ofaRenderPage instance.
	 *
	 * Returns the widget to be attached to the arguments zone.
	 */
	GtkWidget *        ( *get_args_widget )     ( ofaRenderPage *page );

	/**
	 * get_paper_name:
	 * @page: this #ofaRenderPage instance.
	 *
	 * Returns the paper format name.
	 *
	 * This is used to compute the drawn page width and height.
	 *
	 * This base class does not provide any default.
	 * The derived class MUST implement this method and return a
	 * suitable value.
	 */
	const gchar *      ( *get_paper_name )      ( ofaRenderPage *page );

	/**
	 * get_page_orientation:
	 * @page: this #ofaRenderPage instance.
	 *
	 * Returns the page orientation.
	 *
	 * This is used to compute the drawn page width and height.
	 *
	 * This base class does not provide any default.
	 * The derived class MUST implement this method and return a
	 * suitable value.
	 */
	GtkPageOrientation ( *get_page_orientation )( ofaRenderPage *page );

	/**
	 * get_print_settings:
	 * @page: this #ofaRenderPage instance.
	 *
	 * Returns the name of the keyfile's group which holds the
	 * current print settings.
	 */
	void               ( *get_print_settings )  ( ofaRenderPage *page,
														GKeyFile **keyfile,
														gchar **group_name );

	/**
	 * get_dataset:
	 * @page: this #ofaRenderPage instance.
	 *
	 * Returns the dataset for the current arguments.
	 */
	GList *            ( *get_dataset )         ( ofaRenderPage *page );

	/**
	 * free_dataset:
	 * @page: this #ofaRenderPage instance.
	 *
	 * Free the current dataset after an argument has changed.
	 */
	void               ( *free_dataset )        ( ofaRenderPage *page,
														GList *dataset );
}
	ofaRenderPageClass;

GType        ofa_render_page_get_type        ( void ) G_GNUC_CONST;

void         ofa_render_page_set_args_changed( ofaRenderPage *page,
													gboolean is_valid,
													const gchar *message );

ofaIContext *ofa_render_page_get_icontext    ( ofaRenderPage *page );

GtkWidget   *ofa_render_page_get_top_paned   ( ofaRenderPage *page );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_RENDER_PAGE_H__ */
