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

#ifndef __OFA_RECONCIL_GROUP_H__
#define __OFA_RECONCIL_GROUP_H__

/**
 * SECTION: ofa_reconcil_group
 * @short_description: #ofaReconcilGroup class definition.
 * @include: core/ofa-reconcil-group.h
 *
 * Display BAT lines and/or entries of a reconciliation group.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: no
 * - settings:           no
 * - current:            no
 */

#include <gtk/gtk.h>

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECONCIL_GROUP                ( ofa_reconcil_group_get_type())
#define OFA_RECONCIL_GROUP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECONCIL_GROUP, ofaReconcilGroup ))
#define OFA_RECONCIL_GROUP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECONCIL_GROUP, ofaReconcilGroupClass ))
#define OFA_IS_RECONCIL_GROUP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECONCIL_GROUP ))
#define OFA_IS_RECONCIL_GROUP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECONCIL_GROUP ))
#define OFA_RECONCIL_GROUP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECONCIL_GROUP, ofaReconcilGroupClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaReconcilGroup;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaReconcilGroupClass;

GType ofa_reconcil_group_get_type( void ) G_GNUC_CONST;

void  ofa_reconcil_group_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										ofxCounter concil_id );

G_END_DECLS

#endif /* __OFA_RECONCIL_GROUP_H__ */
