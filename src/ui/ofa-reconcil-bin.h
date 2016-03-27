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

#ifndef __OFA_RECONCIL_BIN_H__
#define __OFA_RECONCIL_BIN_H__

/**
 * SECTION: ofa_reconcil_bin
 * @short_description: #ofaReconcilBin class definition.
 * @include: ui/ofa-reconcil-bin.h
 *
 * Display a frame with let the user select the parameters needed to
 * print the reconciliation summary.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   yes
 * - current:    no
 */

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCIL_BIN                ( ofa_reconcil_bin_get_type())
#define OFA_RECONCIL_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_BIN, ofaReconcilBin ))
#define OFA_RECONCIL_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_BIN, ofaReconcilBinClass ))
#define OFA_IS_RECONCIL_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_BIN ))
#define OFA_IS_RECONCIL_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_BIN ))
#define OFA_RECONCIL_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_BIN, ofaReconcilBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaReconcilBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaReconcilBinClass;

GType           ofa_reconcil_bin_get_type   ( void ) G_GNUC_CONST;

ofaReconcilBin *ofa_reconcil_bin_new        ( ofaIGetter *getter );

gboolean        ofa_reconcil_bin_is_valid   ( ofaReconcilBin *bin,
													gchar **msgerr );

const gchar    *ofa_reconcil_bin_get_account( const ofaReconcilBin *bin );

void            ofa_reconcil_bin_set_account( ofaReconcilBin *bin,
													const gchar *number );

const GDate    *ofa_reconcil_bin_get_date   ( const ofaReconcilBin *bin );

G_END_DECLS

#endif /* __OFA_RECONCIL_BIN_H__ */
