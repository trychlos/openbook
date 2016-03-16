/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RECURRENT_NEW_H__
#define __OFA_RECURRENT_NEW_H__

/**
 * SECTION: ofa_recurrent_new
 * @short_description: #ofaRecurrentNew class definition.
 * @include: recurrent/ofa-recurrent-new.h
 *
 * Let the user validate the generated operations before recording.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   yes
 * - current:    no
 */

#include "api/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_NEW                ( ofa_recurrent_new_get_type())
#define OFA_RECURRENT_NEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_NEW, ofaRecurrentNew ))
#define OFA_RECURRENT_NEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_NEW, ofaRecurrentNewClass ))
#define OFA_IS_RECURRENT_NEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_NEW ))
#define OFA_IS_RECURRENT_NEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_NEW ))
#define OFA_RECURRENT_NEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_NEW, ofaRecurrentNewClass ))

typedef struct _ofaRecurrentNewPrivate        ofaRecurrentNewPrivate;

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaRecurrentNew;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaRecurrentNewClass;

GType ofa_recurrent_new_get_type( void ) G_GNUC_CONST;

void  ofa_recurrent_new_run     ( ofaMainWindow *main_window,
										GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_RECURRENT_NEW_H__ */
