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

#ifndef __OFA_CHECK_INTEGRITY_H__
#define __OFA_CHECK_INTEGRITY_H__

/**
 * SECTION: ofa_check_integrity
 * @short_description: #ofaCheckIntegrity class definition.
 * @include: ui/ofa-check-integrity.h
 *
 * User interface for the DBMS integrity check.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CHECK_INTEGRITY                ( ofa_check_integrity_get_type())
#define OFA_CHECK_INTEGRITY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CHECK_INTEGRITY, ofaCheckIntegrity ))
#define OFA_CHECK_INTEGRITY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CHECK_INTEGRITY, ofaCheckIntegrityClass ))
#define OFA_IS_CHECK_INTEGRITY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CHECK_INTEGRITY ))
#define OFA_IS_CHECK_INTEGRITY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CHECK_INTEGRITY ))
#define OFA_CHECK_INTEGRITY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CHECK_INTEGRITY, ofaCheckIntegrityClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaCheckIntegrity;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaCheckIntegrityClass;

GType    ofa_check_integrity_get_type( void ) G_GNUC_CONST;

void     ofa_check_integrity_run     ( ofaIGetter *getter,
											GtkWindow *parent );

gboolean ofa_check_integrity_check   ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_CHECK_INTEGRITY_H__ */
