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

#ifndef __OFA_DOSSIER_CUR_H__
#define __OFA_DOSSIER_CUR_H__

/**
 * SECTION: ofa_dossier_cur
 * @short_description: #ofaDossierCur class definition.
 * @include: ui/ofa-dossier-cur.h
 *
 * Update the DOSSIER_CUR subtable.
 */

#include <gtk/gtk.h>

#include "core/my-dialog.h"
#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_CUR                ( ofa_dossier_cur_get_type())
#define OFA_DOSSIER_CUR( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_CUR, ofaDossierCur ))
#define OFA_DOSSIER_CUR_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_CUR, ofaDossierCurClass ))
#define OFA_IS_DOSSIER_CUR( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_CUR ))
#define OFA_IS_DOSSIER_CUR_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_CUR ))
#define OFA_DOSSIER_CUR_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_CUR, ofaDossierCurClass ))

typedef struct _ofaDossierCurPrivate        ofaDossierCurPrivate;

typedef struct {
	/*< public members >*/
	myDialog              parent;

	/*< private members >*/
	ofaDossierCurPrivate *priv;
}
	ofaDossierCur;

typedef struct {
	/*< public members >*/
	myDialogClass         parent;
}
	ofaDossierCurClass;

GType    ofa_dossier_cur_get_type( void ) G_GNUC_CONST;

void     ofa_dossier_cur_run     ( ofaMainWindow *main_window, GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_DOSSIER_CUR_H__ */
