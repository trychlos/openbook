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

#ifndef __OFA_EXE_CLOSING_H__
#define __OFA_EXE_CLOSING_H__

/**
 * SECTION: ofa_exe_closing
 * @short_description: #ofaExeClosing class definition.
 * @include: ui/ofa-int-closing.h
 *
 * Run an intermediate closing on the selected journals.
 */

#include "api/ofo-ledger-def.h"

#include "core/my-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXE_CLOSING                ( ofa_exe_closing_get_type())
#define OFA_EXE_CLOSING( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXE_CLOSING, ofaExeClosing ))
#define OFA_EXE_CLOSING_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXE_CLOSING, ofaExeClosingClass ))
#define OFA_IS_EXE_CLOSING( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXE_CLOSING ))
#define OFA_IS_EXE_CLOSING_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXE_CLOSING ))
#define OFA_EXE_CLOSING_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXE_CLOSING, ofaExeClosingClass ))

typedef struct _ofaExeClosingPrivate        ofaExeClosingPrivate;

typedef struct {
	/*< public members >*/
	myDialog              parent;

	/*< private members >*/
	ofaExeClosingPrivate *priv;
}
	ofaExeClosing;

typedef struct {
	/*< public members >*/
	myDialogClass parent;
}
	ofaExeClosingClass;

GType    ofa_exe_closing_get_type( void ) G_GNUC_CONST;

gboolean ofa_exe_closing_run     ( ofaMainWindow *parent );

G_END_DECLS

#endif /* __OFA_EXE_CLOSING_H__ */
