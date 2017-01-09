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

#ifndef __OPENBOOK_API_OFA_PANED_PAGE_H__
#define __OPENBOOK_API_OFA_PANED_PAGE_H__

/**
 * SECTION: ofappanedage
 * @title: ofaPanedPage
 * @short_description: #ofaPanedPage class definition.
 * @include: openbook/ofa-paned-page.h
 *
 * The #ofaPanedPage is derived from #ofaPage. It is so a #GtkGrid to
 * be displayed in a tab, as a child of the 'main' notebook.
 *
 * The #ofaPanedPage is meant to be the base class for #ofaPage's which
 * want manage their datas through a vertical pane. We have so a left
 * view and a right view.
 *
 * Properties:
 *
 * - ofa-paned-page-position:
 *   the initial position of the paned page, should be set at
 *   construction time by the derived class;
 *   defaults to 150.
 *
 * - ofa-paned-page-orientation:
 *   the orientation of the paned page;
 *   defaults to GTK_ORIENTATION_HORIZONTAL.
 *
 * Dynamic of the build
 * ====================
 * ofaPage                        ofaPanedPage                   derived_class
 * -----------------------------  -----------------------------  ------------------------
 * |
 * +- instance_initialization
 *    +->                         instance_initialization
 *    +->                                                        instance_initialization
 *    |
 *    +- instance_construction
 *       +->                      instance_construction
 *       +->                                                     instance_construction
 *       |
 *       +- do_setup_page()
 *          +->                   ofaPanedPage::v_setup_page()
 *                                |
 *                                +- allocate the GtkPaned
 *                                |  attaching it to the grid
 *                                |
 *                                +- do_setup_view()
 *                                |  +->                         v_setup_view()
 *                                |                              + attach the left view
 *                                |                                attach the right view
 *                                +- do_init_view()
 *                                   +->                         v_init_view()
 */

#include <gtk/gtk.h>

#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PANED_PAGE                ( ofa_paned_page_get_type())
#define OFA_PANED_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PANED_PAGE, ofaPanedPage ))
#define OFA_PANED_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PANED_PAGE, ofaPanedPageClass ))
#define OFA_IS_PANED_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PANED_PAGE ))
#define OFA_IS_PANED_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PANED_PAGE ))
#define OFA_PANED_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PANED_PAGE, ofaPanedPageClass ))

typedef struct {
	/*< public members >*/
	ofaPage      parent;
}
	ofaPanedPage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;

	/*< protected virtual functions >*/
	/**
	 * setup_view:
	 * @page: this #ofaPanedPage object.
	 * @paned: the #GtkPaned paned.
	 *
	 * This virtual function is called at the end of instance
	 * construction (before instance initialization returns).
	 *
	 * The derived class should take advantage of this method to setup
	 * and attach (pack) the two parts of the @paned.
	 */
	void        ( *setup_view )( ofaPanedPage *page,
										GtkPaned *paned );

	/**
	 * init_view:
	 * @page: this #ofaPanedPage object.
	 *
	 * This virtual function is called after the previous setup_xxx()
	 * methods.
	 *
	 * The derived class may take advantage of this call to
	 * initialize its datas, made sure that the two views are available.
	 */
	void        ( *init_view )  ( ofaPanedPage *page );

}
	ofaPanedPageClass;

GType ofa_paned_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PANED_PAGE_H__ */
