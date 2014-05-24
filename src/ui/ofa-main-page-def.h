/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MAIN_PAGE_DEF_H__
#define __OFA_MAIN_PAGE_DEF_H__

/**
 * SECTION: ofa_main_page
 * @short_description: #ofaMainPage class definition.
 * @include: ui/ofa-main-page.h
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_MAIN_PAGE                ( ofa_main_page_get_type())
#define OFA_MAIN_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MAIN_PAGE, ofaMainPage ))
#define OFA_MAIN_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MAIN_PAGE, ofaMainPageClass ))
#define OFA_IS_MAIN_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MAIN_PAGE ))
#define OFA_IS_MAIN_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MAIN_PAGE ))
#define OFA_MAIN_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MAIN_PAGE, ofaMainPageClass ))

typedef struct _ofaMainPagePrivate        ofaMainPagePrivate;

typedef struct {
	/*< private members >*/
	GObject             parent;
	ofaMainPagePrivate *private;
}
	ofaMainPage;

typedef struct {
	/*< private members >*/
	GObjectClass             parent;

	/*< protected virtual functions >*/
	/**
	 * setup_page:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is called during instance construction.
	 * The base class implementation successively calls #setup_view()
	 * and #setup_buttons_box() virtual methods, attaching the two
	 * returned widgets respectively on columns 0 and 1 of the main
	 * grid of the page.
	 * The base class implementation ends up by calling #init_view()
	 * virtual method.
	 */
	void        ( *setup_page )( ofaMainPage *page );

	/**
	 * setup_view:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is called by #setup_page() implementation
	 * of the base class virtual method.
	 * This is a pure virtual function, that the child class should
	 * implement.
	 */
	GtkWidget * ( *setup_view )( ofaMainPage *page );

	/**
	 * setup_buttons:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is called by #setup_page() implementation
	 * of the base class virtual method.
	 * The base class implementation defines three 'New',
	 * 'Update' and 'Delete' buttons, attaching respectively to
	 * 'on_new_clicked', 'on_update_clicked' and 'on_delete_clicked'
	 * virtual methods.
	 */
	GtkWidget * ( *setup_buttons )    ( ofaMainPage *page );

	/**
	 * init_view:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is called by #setup_page() implementation
	 * of the base class virtual method.
	 * This is a pure virtual function, that the child class should
	 * implement.
	 */
	void        ( *init_view )        ( ofaMainPage *page );

	/**
	 * on_new_clicked:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is triggered when the 'New' button
	 * installed by the above #setup_buttons() virtual function is
	 * clicked.
	 * The base class implementation does nothing.
	 */
	void        ( *on_new_clicked )   ( GtkButton *button, ofaMainPage *page );

	/**
	 * on_update_clicked:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is triggered when the 'Update' button
	 * installed by the above #setup_buttons() virtual function is
	 * clicked.
	 * The base class implementation does nothing.
	 */
	void        ( *on_update_clicked )( GtkButton *button, ofaMainPage *page );

	/**
	 * on_delete_clicked:
	 * @page: this #ofaMainPage object.
	 *
	 * This virtual function is triggered when the 'Delete' button
	 * installed by the above #setup_buttons() virtual function is
	 * clicked.
	 * The base class implementation does nothing.
	 */
	void        ( *on_delete_clicked )( GtkButton *button, ofaMainPage *page );
}
	ofaMainPageClass;

G_END_DECLS

#endif /* __OFA_MAIN_PAGE_DEF_H__ */
