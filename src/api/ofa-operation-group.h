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

#ifndef __OPENBOOK_API_OFA_OPERATION_GROUP_H__
#define __OPENBOOK_API_OFA_OPERATION_GROUP_H__

/**
 * SECTION: ofa_operation_group
 * @short_description: #ofaOperationGroup class definition.
 * @include: openbook/ofa-operation-group.h
 *
 * Display entries of an operation.
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

#define OFA_TYPE_OPERATION_GROUP                ( ofa_operation_group_get_type())
#define OFA_OPERATION_GROUP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPERATION_GROUP, ofaOperationGroup ))
#define OFA_OPERATION_GROUP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPERATION_GROUP, ofaOperationGroupClass ))
#define OFA_IS_OPERATION_GROUP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPERATION_GROUP ))
#define OFA_IS_OPERATION_GROUP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPERATION_GROUP ))
#define OFA_OPERATION_GROUP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPERATION_GROUP, ofaOperationGroupClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaOperationGroup;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaOperationGroupClass;

GType ofa_operation_group_get_type( void ) G_GNUC_CONST;

void  ofa_operation_group_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										GList *opes );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_OPERATION_GROUP_H__ */
