/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_ACCOUNT_SELECT_H__
#define __OFA_ACCOUNT_SELECT_H__

/**
 * SECTION: ofa_account_select
 * @short_description: #ofaAccountSelect class definition.
 * @include: core/ofa-account-select.h
 *
 * Display the chart of accounts, letting the user edit it.
 *
 * Development rules:
 * - type:         modal dialog
 * - settings:     yes
 * - current:      no
 * - on terminate: hide
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-account-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ACCOUNT_SELECT                ( ofa_account_select_get_type())
#define OFA_ACCOUNT_SELECT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_ACCOUNT_SELECT, ofaAccountSelect ))
#define OFA_ACCOUNT_SELECT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_ACCOUNT_SELECT, ofaAccountSelectClass ))
#define OFA_IS_ACCOUNT_SELECT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_ACCOUNT_SELECT ))
#define OFA_IS_ACCOUNT_SELECT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_ACCOUNT_SELECT ))
#define OFA_ACCOUNT_SELECT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_ACCOUNT_SELECT, ofaAccountSelectClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaAccountSelect;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaAccountSelectClass;

GType  ofa_account_select_get_type ( void ) G_GNUC_CONST;

gchar *ofa_account_select_run_modal( ofaIGetter *getter,
											GtkWindow *parent,
											const gchar *asked_number,
											ofeAccountAllowed allowed );

G_END_DECLS

#endif /* __OFA_ACCOUNT_SELECT_H__ */
