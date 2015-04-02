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

#ifndef __OPENBOOK_API_MY_WINDOW_PROT_H__
#define __OPENBOOK_API_MY_WINDOW_PROT_H__

/**
 * SECTION: my_window
 * @short_description: #myWindow class definition.
 * @include: openbook/my-window.h
 *
 * This is a base class for application window toplevels. These may be
 * either GtkDialog-derived or GtkAssistant-derived classes.
 *
 * This header is supposed to be included only by the child classes.
 */

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

/* protected instance data
 * freely available to all derived classes
 */
struct _myWindowProtected {
	gboolean dispose_has_run;
};

G_END_DECLS

#endif /* __OPENBOOK_API_MY_WINDOW_PROT_H__ */
