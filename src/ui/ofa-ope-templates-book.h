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

#ifndef __OFA_OPE_TEMPLATES_BOOK_H__
#define __OFA_OPE_TEMPLATES_BOOK_H__

/**
 * SECTION: ofa_ope_templates_book
 * @short_description: #ofaOpeTemplatesBook class definition.
 * @include: ui/ofa-ope-template-book.h
 *
 * This is a convenience class which manages the display of the
 * operation templates inside of a notebook, with one ledger per page.
 *
 * This convenience class also manages the update buttons (new, update,
 * duplicate and delete). So that almost all the OpeTemplatesPage
 * features are also available in OpeTemplateSelect dialog.
 *
 * The #GtkNotebook is created when attaching to the parent widget.
 * The underlying list store is created (if not already done) when
 * setting the main window. The dataset is so loaded, and inserted
 * in the store.
 * The #GtkTreeViews are created when a row is inserted for a new
 * ledger.
 * So attaching to the parent widget should be called before setting
 * the main window, so that the treeviews are rightly created.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATES_BOOK                ( ofa_ope_templates_book_get_type())
#define OFA_OPE_TEMPLATES_BOOK( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATES_BOOK, ofaOpeTemplatesBook ))
#define OFA_OPE_TEMPLATES_BOOK_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATES_BOOK, ofaOpeTemplatesBookClass ))
#define OFA_IS_OPE_TEMPLATES_BOOK( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATES_BOOK ))
#define OFA_IS_OPE_TEMPLATES_BOOK_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATES_BOOK ))
#define OFA_OPE_TEMPLATES_BOOK_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATES_BOOK, ofaOpeTemplatesBookClass ))

typedef struct _ofaOpeTemplatesBookPrivate         ofaOpeTemplatesBookPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaOpeTemplatesBookPrivate *priv;
}
	ofaOpeTemplatesBook;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaOpeTemplatesBookClass;

GType               ofa_ope_templates_book_get_type            ( void ) G_GNUC_CONST;

ofaOpeTemplatesBook *ofa_ope_templates_book_new                ( void );

void                ofa_ope_templates_book_set_main_window     ( ofaOpeTemplatesBook *book,
																		ofaMainWindow *main_window );

void                ofa_ope_templates_book_expand_all          ( ofaOpeTemplatesBook *book );

gchar              *ofa_ope_templates_book_get_selected        ( ofaOpeTemplatesBook *book );

void                ofa_ope_templates_book_set_selected        ( ofaOpeTemplatesBook *book,
																		const gchar *mnemo );

void                ofa_ope_templates_book_button_clicked      ( ofaOpeTemplatesBook *book,
																		gint button_id );

GtkWidget          *ofa_ope_templates_book_get_current_treeview( const ofaOpeTemplatesBook *book );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATES_BOOK_H__ */
