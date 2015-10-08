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
 */

#ifndef __OFA_OPE_TEMPLATE_BOOK_BIN_H__
#define __OFA_OPE_TEMPLATE_BOOK_BIN_H__

/**
 * SECTION: ofa_ope_template_book_bin
 * @short_description: #ofaOpeTemplateBookBin class definition.
 * @include: ui/ofa-ope-template-book.h
 *
 * This is a convenience class which manages the display of the
 * operation templates inside of a notebook, with one ledger per page.
 *
 * This convenience class also manages the update buttons (new, update,
 * duplicate and delete). So that almost all the OpeTemplatePage
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

#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_BOOK_BIN                ( ofa_ope_template_book_bin_get_type())
#define OFA_OPE_TEMPLATE_BOOK_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_BOOK_BIN, ofaOpeTemplateBookBin ))
#define OFA_OPE_TEMPLATE_BOOK_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_BOOK_BIN, ofaOpeTemplateBookBinClass ))
#define OFA_IS_OPE_TEMPLATE_BOOK_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_BOOK_BIN ))
#define OFA_IS_OPE_TEMPLATE_BOOK_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_BOOK_BIN ))
#define OFA_OPE_TEMPLATE_BOOK_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_BOOK_BIN, ofaOpeTemplateBookBinClass ))

typedef struct _ofaOpeTemplateBookBinPrivate         ofaOpeTemplateBookBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaOpeTemplateBookBinPrivate *priv;
}
	ofaOpeTemplateBookBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaOpeTemplateBookBinClass;

GType                  ofa_ope_template_book_bin_get_type            ( void ) G_GNUC_CONST;

ofaOpeTemplateBookBin *ofa_ope_template_book_bin_new                 ( const ofaMainWindow *main_window );

void                   ofa_ope_template_book_bin_expand_all          ( ofaOpeTemplateBookBin *bin );

gchar                 *ofa_ope_template_book_bin_get_selected        ( ofaOpeTemplateBookBin *bin );

void                   ofa_ope_template_book_bin_set_selected        ( ofaOpeTemplateBookBin *bin,
																				const gchar *mnemo );

void                   ofa_ope_template_book_bin_button_clicked      ( ofaOpeTemplateBookBin *bin,
																				gint button_id );

GtkWidget             *ofa_ope_template_book_bin_get_current_treeview( const ofaOpeTemplateBookBin *bin );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_BOOK_BIN_H__ */
