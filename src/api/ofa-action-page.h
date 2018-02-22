/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ACTION_PAGE_H__
#define __OPENBOOK_API_OFA_ACTION_PAGE_H__

/**
 * SECTION: ofaactionpage
 * @title: ofaActionPage
 * @short_description: #ofaActionPage class definition.
 * @include: openbook/ofa-action-page.h
 *
 * The #ofaActionPage is derived from #ofaPage. It is so a #GtkGrid to
 * be displayed in a tab, as a child of the 'main' notebook.
 *
 * The #ofaActionPage is meant to be the base class for #ofaPage's which
 * want manage action buttons on the right side of their view.
 *
 * Dynamic of the build
 * ====================
 * ofaPage                        ofaActionPage                  derived_class
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
 *          +->                   ofaActionPage::v_setup_page()
 *                                |
 *                                +- do_setup_view()
 *                                   +->                         v_setup_view()
 *                                   attaching the returned
 *                                   widget to the grid
 *
 *                                +  allocate the ofaButtonsBox
 *                                +- do_setup_actions()
 *                                   +->                         v_setup_actions()
 *
 *                                +- do_init_view()
 *                                   +->                         v_init_view()
 */

#include <gtk/gtk.h>

#include "api/ofa-buttons-box.h"
#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACTION_PAGE                ( ofa_action_page_get_type())
#define OFA_ACTION_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACTION_PAGE, ofaActionPage ))
#define OFA_ACTION_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACTION_PAGE, ofaActionPageClass ))
#define OFA_IS_ACTION_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACTION_PAGE ))
#define OFA_IS_ACTION_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACTION_PAGE ))
#define OFA_ACTION_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACTION_PAGE, ofaActionPageClass ))

typedef struct {
	/*< public members >*/
	ofaPage      parent;
}
	ofaActionPage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;

	/*< protected virtual functions >*/
	/**
	 * setup_view:
	 * @page: this #ofaActionPage object.
	 *
	 * This virtual function is called at the end of instance
	 * construction (before instance initialization returns).
	 *
	 * The derived class should implement this method in order to
	 * display something in the view.
	 *
	 * Returns: the #GtkWidget to be attached to the parent #GtkGrid.
	 *
	 * The returned widget (if any) will be insert in (row=0, column=0)
	 * of the top #GtkGrid.
	 */
	GtkWidget * ( *setup_view )   ( ofaActionPage *page );

	/**
	 * setup_actions:
	 * @page: this #ofaActionPage object.
	 * @buttons_box: the #ofaButtonsBox which will embed action buttons.
	 *
	 * This virtual function is called after #setup_view() above method.
	 *
	 * The derived class may take advantage of this method to add to the
	 * provided #ofaButtonsBox the action buttons it wishes implement.
	 */
	void        ( *setup_actions )( ofaActionPage *page,
											ofaButtonsBox *box );

	/**
	 * init_view:
	 * @page: this #ofaActionPage object.
	 *
	 * This virtual function is called after the previous setup_xxx()
	 * methods.
	 *
	 * The derived class may take advantage of this call to
	 * initialize its datas, made sure that both the view and the
	 * actions have been previously defined, and are available.
	 */
	void        ( *init_view )    ( ofaActionPage *page );

}
	ofaActionPageClass;

GType ofa_action_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ACTION_PAGE_H__ */
