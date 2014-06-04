/*
 * Open Freelance Accounting
 * A double-entry bating application for freelances.
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

#ifndef __OFA_BAT_SELECT_H__
#define __OFA_BAT_SELECT_H__

/**
 * SECTION: ofa_bat_select
 * @short_description: #ofaBatSelect class definition.
 * @include: ui/ofa-bat-select.h
 *
 * Display the chart of bats, letting the user edit it.
 */

#include "ui/ofa-base-dialog.h"
#include "ui/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_SELECT                ( ofa_bat_select_get_type())
#define OFA_BAT_SELECT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_SELECT, ofaBatSelect ))
#define OFA_BAT_SELECT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_SELECT, ofaBatSelectClass ))
#define OFA_IS_BAT_SELECT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_SELECT ))
#define OFA_IS_BAT_SELECT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_SELECT ))
#define OFA_BAT_SELECT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_SELECT, ofaBatSelectClass ))

typedef struct _ofaBatSelectPrivate        ofaBatSelectPrivate;

typedef struct {
	/*< private >*/
	ofaBaseDialog            parent;
	ofaBatSelectPrivate *private;
}
	ofaBatSelect;

typedef struct {
	/*< private >*/
	ofaBaseDialogClass parent;
}
	ofaBatSelectClass;

GType  ofa_bat_select_get_type( void ) G_GNUC_CONST;

gint   ofa_bat_select_run     ( ofaMainWindow *main_window );

G_END_DECLS

#endif /* __OFA_BAT_SELECT_H__ */
