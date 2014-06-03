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

#ifndef __BASE_WINDOW_PROT_H__
#define __BASE_WINDOW_PROT_H__

/**
 * SECTION: base_window
 * @short_description: #BaseWindow class definition.
 * @include: ui/ofa-base-window.h
 *
 * Base class for application window boxes.
 *
 * This header is supposed to be included only by the child classes.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* protected instance data
 * freely available to all derived classes
 */
struct _BaseWindowProtected {
	gboolean       dispose_has_run;

	/* this may be either a GtkDialog or a GtkAssistant
	 */
	GtkWindow     *window;
};

G_END_DECLS

#endif /* __BASE_WINDOW_PROT_H__ */
