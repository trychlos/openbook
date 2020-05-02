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

#ifndef __OFA_DBSU_H__
#define __OFA_DBSU_H__

/**
 * SECTION: ofa_dbsu
 * @short_description: #ofaDbsu class definition.
 * @include: ui/ofa-dbsu.h
 *
 * Get the superuser credentials
 *
 * Development rules:
 * - type:       modal dialog
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbsuperuser-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DBSU                ( ofa_dbsu_get_type())
#define OFA_DBSU( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DBSU, ofaDbsu ))
#define OFA_DBSU_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DBSU, ofaDbsuClass ))
#define OFA_IS_DBSU( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DBSU ))
#define OFA_IS_DBSU_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DBSU ))
#define OFA_DBSU_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DBSU, ofaDbsuClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDbsu;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDbsuClass;

GType    ofa_dbsu_get_type ( void ) G_GNUC_CONST;

gboolean ofa_dbsu_run_modal( ofaIGetter *getter,
								GtkWindow *parent,
								ofaIDBSuperuser *su_bin );

G_END_DECLS

#endif /* __OFA_DBSU_H__ */
