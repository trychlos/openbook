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

#ifndef __OFA_ABOUT_H__
#define __OFA_ABOUT_H__

/**
 * SECTION: ofa_about
 * @short_description: #ofaAbout class definition.
 * @include: ui/ofa-about.h
 *
 * A convenience class to manage a balance grid.
 * It defines one "update" action signal to let the user update a
 * balance row in the grid.
 */

#include "api/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ABOUT                ( ofa_about_get_type())
#define OFA_ABOUT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ABOUT, ofaAbout ))
#define OFA_ABOUT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ABOUT, ofaAboutClass ))
#define OFA_IS_ABOUT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ABOUT ))
#define OFA_IS_ABOUT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ABOUT ))
#define OFA_ABOUT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ABOUT, ofaAboutClass ))

typedef struct {
	/*< public members >*/
	GtkAboutDialog      parent;
}
	ofaAbout;

typedef struct {
	/*< public members >*/
	GtkAboutDialogClass parent;
}
	ofaAboutClass;

GType ofa_about_get_type ( void ) G_GNUC_CONST;

void  ofa_about_run      ( const ofaMainWindow *main_window );

G_END_DECLS

#endif /* __OFA_ABOUT_H__ */
