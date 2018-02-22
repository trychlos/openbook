/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_BAT_PROPERTIES_H__
#define __OFA_BAT_PROPERTIES_H__

/**
 * SECTION: ofa_bat_properties
 * @short_description: #ofaBatProperties class definition.
 * @include: core/ofa-bat-properties.h
 *
 * Display the BAT properties. Only the notes can be updated when the
 * opened dossier is not an archive.
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, maybe after having displayed an error message box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: no
 * - settings:           yes
 * - current:            yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-bat-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_PROPERTIES                ( ofa_bat_properties_get_type())
#define OFA_BAT_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_PROPERTIES, ofaBatProperties ))
#define OFA_BAT_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_PROPERTIES, ofaBatPropertiesClass ))
#define OFA_IS_BAT_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_PROPERTIES ))
#define OFA_IS_BAT_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_PROPERTIES ))
#define OFA_BAT_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_PROPERTIES, ofaBatPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaBatProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaBatPropertiesClass;

GType ofa_bat_properties_get_type( void ) G_GNUC_CONST;

void  ofa_bat_properties_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										ofoBat *bat );

G_END_DECLS

#endif /* __OFA_BAT_PROPERTIES_H__ */
