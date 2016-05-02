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

#ifndef __OFA_MISC_AUDIT_UI_H__
#define __OFA_MISC_AUDIT_UI_H__

/**
 * SECTION: ofa-miscaudit_ui
 * @include: ui/ofa-misc-audit-ui.h
 *
 * Display the DBMS audit trace.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_MISC_AUDIT_UI                ( ofa_misc_audit_ui_get_type())
#define OFA_MISC_AUDIT_UI( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MISC_AUDIT_UI, ofaMiscAuditUI ))
#define OFA_MISC_AUDIT_UI_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MISC_AUDIT_UI, ofaMiscAuditUIClass ))
#define OFA_IS_MISC_AUDIT_UI( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MISC_AUDIT_UI ))
#define OFA_IS_MISC_AUDIT_UI_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MISC_AUDIT_UI ))
#define OFA_MISC_AUDIT_UI_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MISC_AUDIT_UI, ofaMiscAuditUIClass ))

typedef struct {
	/*< public members >*/
	GtkDialog         parent;
}
	ofaMiscAuditUI;

typedef struct {
	/*< public members >*/
	GtkDialogClass    parent;
}
	ofaMiscAuditUIClass;

GType ofa_misc_audit_ui_get_type( void ) G_GNUC_CONST;

void  ofa_misc_audit_ui_run     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_MISC_AUDIT_UI_H__ */
