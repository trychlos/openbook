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

#ifndef __OFA_RECONCIL_ARGS_H__
#define __OFA_RECONCIL_ARGS_H__

/**
 * SECTION: ofa_reconcil_args
 * @short_description: #ofaReconcilArgs class definition.
 * @include: ui/ofa-reconcil-args.h
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

#define OFA_TYPE_RECONCIL_ARGS                ( ofa_reconcil_args_get_type())
#define OFA_RECONCIL_ARGS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_ARGS, ofaReconcilArgs ))
#define OFA_RECONCIL_ARGS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_ARGS, ofaReconcilArgsClass ))
#define OFA_IS_RECONCIL_ARGS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_ARGS ))
#define OFA_IS_RECONCIL_ARGS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_ARGS ))
#define OFA_RECONCIL_ARGS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_ARGS, ofaReconcilArgsClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaReconcilArgs;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaReconcilArgsClass;

GType           ofa_reconcil_args_get_type   ( void ) G_GNUC_CONST;

ofaReconcilArgs *ofa_reconcil_args_new        ( ofaIGetter *getter,
													const gchar *settings_prefix );

gboolean        ofa_reconcil_args_is_valid   ( ofaReconcilArgs *bin,
													gchar **msgerr );

const gchar    *ofa_reconcil_args_get_account( ofaReconcilArgs *bin );

void            ofa_reconcil_args_set_account( ofaReconcilArgs *bin,
													const gchar *number );

const GDate    *ofa_reconcil_args_get_date   ( ofaReconcilArgs *bin );

G_END_DECLS

#endif /* __OFA_RECONCIL_ARGS_H__ */
