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

#ifndef __OFA_EXE_FORWARD_H__
#define __OFA_EXE_FORWARD_H__

/**
 * SECTION: ofa_exe_forward
 * @short_description: #ofaExeForward class definition.
 * @include: ui/ofa-exe-forward.h
 *
 * The configuration of carried forward entries in the dossier.
 */

#include <gtk/gtk.h>

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXE_FORWARD                ( ofa_exe_forward_get_type())
#define OFA_EXE_FORWARD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXE_FORWARD, ofaExeForward ))
#define OFA_EXE_FORWARD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXE_FORWARD, ofaExeForwardClass ))
#define OFA_IS_EXE_FORWARD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXE_FORWARD ))
#define OFA_IS_EXE_FORWARD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXE_FORWARD ))
#define OFA_EXE_FORWARD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXE_FORWARD, ofaExeForwardClass ))

typedef struct _ofaExeForwardPrivate        ofaExeForwardPrivate;

typedef struct {
	/*< public members >*/
	GObject               parent;

	/*< private members >*/
	ofaExeForwardPrivate *priv;
}
	ofaExeForward;

typedef struct {
	/*< public members >*/
	GObjectClass          parent;
}
	ofaExeForwardClass;

GType          ofa_exe_forward_get_type ( void ) G_GNUC_CONST;

ofaExeForward *ofa_exe_forward_new      ( void );

void           ofa_exe_forward_attach_to( ofaExeForward *prefs,
													GtkContainer *new_parent,
													ofaMainWindow *main_window );

gboolean       ofa_exe_forward_check    ( ofaExeForward *prefs );

void           ofa_exe_forward_apply    ( ofaExeForward *prefs );

G_END_DECLS

#endif /* __OFA_EXE_FORWARD_H__ */
