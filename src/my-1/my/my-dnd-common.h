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

#ifndef __MY_API_MY_DND_COMMON_H__
#define __MY_API_MY_DND_COMMON_H__

/**
 * SECTION: idnd_common
 * @title: myIDndCommon
 * @short_description: The IDndCommon Interface
 * @include: my/my-idnd-common.h
 *
 * Header common to myIDndAttach and myIDndDetach interfaces.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MY_DND_TARGET                   "XdndOpenbookDrag"

#define MY_DND_SHIFT                     20

#define MY_DND_POPUP_SCALE             0.75

/**
 * myDndData:
 * @page_w: the #GtkWidget page.
 * @title:  the title of the page.
 * @parent: the parent toplevel.
 *
 * A data structure to communicate between sender and receiver.
 */
typedef struct {
	GtkWidget *page;
	gchar     *title;
	GtkWindow *parent;
}
	myDndData;

G_END_DECLS

#endif /* __MY_API_MY_DND_COMMON_H__ */
