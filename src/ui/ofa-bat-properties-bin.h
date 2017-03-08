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

#ifndef __OFA_BAT_PROPERTIES_BIN_H__
#define __OFA_BAT_PROPERTIES_BIN_H__

/**
 * SECTION: ofa_bat_properties_bin
 * @short_description: #ofaBatPropertiesBin class definition.
 * @include: ui/ofa-bat-properties-bin.h
 *
 * A convenience class which manages BAT properties. It is used both
 * for displaying the properties both from BatProperties and from
 * BatSelect.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no
 * - settings:   no
 * - current:    yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-bat-def.h"

#include "ui/ofa-batline-treeview.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_PROPERTIES_BIN                ( ofa_bat_properties_bin_get_type())
#define OFA_BAT_PROPERTIES_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_PROPERTIES_BIN, ofaBatPropertiesBin ))
#define OFA_BAT_PROPERTIES_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_PROPERTIES_BIN, ofaBatPropertiesBinClass ))
#define OFA_IS_BAT_PROPERTIES_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_PROPERTIES_BIN ))
#define OFA_IS_BAT_PROPERTIES_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_PROPERTIES_BIN ))
#define OFA_BAT_PROPERTIES_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_PROPERTIES_BIN, ofaBatPropertiesBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaBatPropertiesBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaBatPropertiesBinClass;

GType                ofa_bat_properties_bin_get_type            ( void ) G_GNUC_CONST;

ofaBatPropertiesBin *ofa_bat_properties_bin_new                 ( ofaIGetter *getter,
																		const gchar *settings_prefix );

void                 ofa_bat_properties_bin_set_bat             ( ofaBatPropertiesBin *bin,
																		ofoBat *bat );

ofaBatlineTreeview  *ofa_bat_properties_bin_get_batline_treeview( ofaBatPropertiesBin *bin );

G_END_DECLS

#endif /* __OFA_BAT_PROPERTIES_BIN_H__ */
