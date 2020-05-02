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

#ifndef __OPENBOOK_API_OFA_PAGE_H__
#define __OPENBOOK_API_OFA_PAGE_H__

/**
 * SECTION: ofapage
 * @title: ofaPage
 * @short_description: #ofaPage class definition.
 * @include: openbook/ofa-page.h
 *
 * The main window is organized with an horizontal paned which
 * includes:
 * - a treeview on the left pane, as a fast way to selection the page
 *   to be displayed,
 * - a notebook on the right pane.
 *
 * Each and every tab of this 'main' notebook (the right pane above) is
 * first a #GtkGrid. This #ofaPage class is thought to manage these
 * tabs.
 *
 * In other words, the application is built to display all its datas
 * in #ofaPage -derived classes, which so are manageable through the
 * 'main' notebook tabs.
 *
 * The application manages three types of #ofaPage -derived classes:
 *
 * a/ with a column of action buttons on the right side, managed
 *    through a #ofaButtonsBox (e.g. the #ofaClassPage)
 *
 * b/ with a vertical #GtkPaned which let us display filters or
 *    parameters on one or the other pane (e.g. the #ofaRenderPage's)
 *
 * c/ without any customization at all, the entire page being directly
 *    managed by the derived class (e.g. the #ofaEntryPage).
 *
 * Properties:
 * - 'ofa-page-getter': a #ofaIGetter instance which has been set by
 *   the #ofaMain Window at instanciation time.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofa-page-def.h"
#include "api/ofa-page-prot.h"

G_BEGIN_DECLS

#define OFA_TYPE_PAGE                ( ofa_page_get_type())
#define OFA_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAGE, ofaPage ))
#define OFA_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PAGE, ofaPageClass ))
#define OFA_IS_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAGE ))
#define OFA_IS_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PAGE ))
#define OFA_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAGE, ofaPageClass ))

#if 0
typedef struct                        ofaPage;
typedef struct                        ofaPageClass;
typedef struct _ofaPageProtected      ofaPageProtected;
#endif

GType       ofa_page_get_type                ( void ) G_GNUC_CONST;

GtkWidget  *ofa_page_get_top_focusable_widget( const ofaPage *page );

ofaIGetter *ofa_page_get_getter              ( ofaPage *page );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PAGE_H__ */
