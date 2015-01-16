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

#ifndef __OFA_CLOSING_PARMS_BIN_H__
#define __OFA_CLOSING_PARMS_BIN_H__

/**
 * SECTION: ofa_closing_parms_bin
 * @short_description: #ofaClosingParmsBin class definition.
 * @include: ui/ofa-closing-parms-bin.h
 *
 * The configuration of balancing account carried forward entries when
 * closing the exercice.
 */

#include <gtk/gtk.h>

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CLOSING_PARMS_BIN                ( ofa_closing_parms_bin_get_type())
#define OFA_CLOSING_PARMS_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CLOSING_PARMS_BIN, ofaClosingParmsBin ))
#define OFA_CLOSING_PARMS_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CLOSING_PARMS_BIN, ofaClosingParmsBinClass ))
#define OFA_IS_CLOSING_PARMS_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CLOSING_PARMS_BIN ))
#define OFA_IS_CLOSING_PARMS_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CLOSING_PARMS_BIN ))
#define OFA_CLOSING_PARMS_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CLOSING_PARMS_BIN, ofaClosingParmsBinClass ))

typedef struct _ofaClosingParmsBinPrivate         ofaClosingParmsBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                     parent;

	/*< private members >*/
	ofaClosingParmsBinPrivate *priv;
}
	ofaClosingParmsBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                parent;
}
	ofaClosingParmsBinClass;

GType               ofa_closing_parms_bin_get_type       ( void ) G_GNUC_CONST;

ofaClosingParmsBin *ofa_closing_parms_bin_new            ( void );

void                ofa_closing_parms_bin_attach_to      ( ofaClosingParmsBin *prefs,
																	GtkContainer *new_parent );

void                ofa_closing_parms_bin_set_main_window( ofaClosingParmsBin *prefs,
																	ofaMainWindow *main_window );

gboolean            ofa_closing_parms_bin_is_valid       ( ofaClosingParmsBin *prefs,
																	gchar **msg );

void                ofa_closing_parms_bin_apply          ( ofaClosingParmsBin *prefs );

G_END_DECLS

#endif /* __OFA_CLOSING_PARMS_BIN_H__ */
