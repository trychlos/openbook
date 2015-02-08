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
 *
 * $Id$
 */

#ifndef __OFA_PAGE_DEF_H__
#define __OFA_PAGE_DEF_H__

/**
 * SECTION: ofa_page
 * @short_description: #ofaPage class definition.
 * @include: ui/ofa-page.h
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_PAGE                ( ofa_page_get_type())
#define OFA_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAGE, ofaPage ))
#define OFA_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PAGE, ofaPageClass ))
#define OFA_IS_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAGE ))
#define OFA_IS_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PAGE ))
#define OFA_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAGE, ofaPageClass ))

typedef struct _ofaPageProtected      ofaPageProtected;
typedef struct _ofaPagePrivate        ofaPagePrivate;

typedef struct {
	/*< public members >*/
	GObject           parent;

	/*< protected members >*/
	ofaPageProtected *prot;

	/*< private members >*/
	ofaPagePrivate   *priv;
}
	ofaPage;

typedef struct {
	/*< public members >*/
	GObjectClass parent;

	/*< protected virtual functions >*/
	/**
	 * setup_page:
	 * @page: this #ofaPage object.
	 *
	 * This virtual function is called at the end of instance
	 * construction (before instance initialization returns).
	 *
	 * The base class default implementation successively calls
	 * #setup_view() and #setup_buttons_box() virtual methods,
	 * attaching the two returned widgets respectively on columns 0
	 * and 1 of the main grid of the page.
	 * It ends up by calling gtk_widget_show_all() on the page.
	 */
	void        ( *setup_page )              ( ofaPage *page );

	/**
	 * setup_view:
	 * @page: this #ofaPage object.
	 *
	 * This virtual function is called by #setup_page() default
	 * implementation of the base class virtual method.
	 *
	 * This is a pure virtual function, that the child class should
	 * implement.
	 */
	GtkWidget * ( *setup_view )              ( ofaPage *page );

	/**
	 * setup_buttons:
	 * @page: this #ofaPage object.
	 *
	 * This virtual function is called by #setup_page() default
	 * implementation of the base class virtual method.
	 *
	 * The base class default implementation creates 'New',
	 * 'Properties' and 'Delete' buttons. On click messages are sent to
	 * the corresponding virtual functions.
	 */
	GtkWidget * ( *setup_buttons )           ( ofaPage *page );

	/**
	 * init_view:
	 * @page: this #ofaPage object.
	 *
	 * This virtual function is called after the page has been set up.
	 *
	 * This is a pure virtual function, that the child class should
	 * implement.
	 */
	void        ( *init_view )               ( ofaPage *page );

	/**
	 * on_button_clicked:
	 * @page: this #ofaPage object.
	 * @button_id: the standard identifier of the clicked button.
	 *
	 * This virtual function is triggered when a button is clicked.
	 *
	 * This is a pure virtual function, that the child class should
	 * implement.
	 */
	void        ( *on_button_clicked )       ( ofaPage *page, guint button_id );

	/**
	 * get_top_focusable_widget:
	 * @page: this #ofaPage object.
	 *
	 * This virtual function should return the top focusable widget of
	 * the page. The default implementation just returns NULL. The main
	 * window typically call this virtual when activating a page in
	 * order the focus to be correctly set.
	 */
	GtkWidget * ( *get_top_focusable_widget )( const ofaPage *page );
}
	ofaPageClass;

GType ofa_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_PAGE_DEF_H__ */
