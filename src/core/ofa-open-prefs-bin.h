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

#ifndef __OFA_OPEN_PREFS_BIN_H__
#define __OFA_OPEN_PREFS_BIN_H__

/**
 * SECTION: ofa_open_prefs_bin
 * @short_description: #ofaOpenPrefsBin class definition.
 * @include: core/ofa-open-prefs-bin.h
 *
 * Let the user select its preferences when opening a dossier:
 * - whether to display the notes
 * - whether to display the properties
 * - whether to check the balances
 * - whether to check the DBMS integrity.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPEN_PREFS_BIN                ( ofa_open_prefs_bin_get_type())
#define OFA_OPEN_PREFS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPEN_PREFS_BIN, ofaOpenPrefsBin ))
#define OFA_OPEN_PREFS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPEN_PREFS_BIN, ofaOpenPrefsBinClass ))
#define OFA_IS_OPEN_PREFS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPEN_PREFS_BIN ))
#define OFA_IS_OPEN_PREFS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPEN_PREFS_BIN ))
#define OFA_OPEN_PREFS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPEN_PREFS_BIN, ofaOpenPrefsBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaOpenPrefsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaOpenPrefsBinClass;

GType            ofa_open_prefs_bin_get_type      ( void ) G_GNUC_CONST;

ofaOpenPrefsBin *ofa_open_prefs_bin_new           ( void );

void             ofa_open_prefs_bin_get_data      ( ofaOpenPrefsBin *bin,
														gboolean *display_notes,
														gboolean *when_non_empty,
														gboolean *display_properties,
														gboolean *check_balances,
														gboolean *check_integrity );

void             ofa_open_prefs_bin_set_data      ( ofaOpenPrefsBin *bin,
														gboolean display_notes,
														gboolean when_non_empty,
														gboolean display_properties,
														gboolean check_balances,
														gboolean check_integrity );

G_END_DECLS

#endif /* __OFA_OPEN_PREFS_BIN_H__ */
