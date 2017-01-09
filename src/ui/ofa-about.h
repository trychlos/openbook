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

#ifndef __OFA_ABOUT_H__
#define __OFA_ABOUT_H__

/**
 * SECTION: ofa_about
 * @short_description: #ofaAbout class definition.
 * @include: ui/ofa-about.h
 *
 * Display the About dialog.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

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

void  ofa_about_run      ( ofaIGetter *getter,
								GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_ABOUT_H__ */
