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

#ifndef __OFA_BACKUP_DISPLAY_H__
#define __OFA_BACKUP_DISPLAY_H__

/**
 * SECTION: ofa_backup_display
 * @short_description: #ofaBackupDisplay class definition.
 * @include: ui/ofa-backup-display.h
 *
 * Display the metadatas of an archive file.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BACKUP_DISPLAY                ( ofa_backup_display_get_type())
#define OFA_BACKUP_DISPLAY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BACKUP_DISPLAY, ofaBackupDisplay ))
#define OFA_BACKUP_DISPLAY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BACKUP_DISPLAY, ofaBackupDisplayClass ))
#define OFA_IS_BACKUP_DISPLAY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BACKUP_DISPLAY ))
#define OFA_IS_BACKUP_DISPLAY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BACKUP_DISPLAY ))
#define OFA_BACKUP_DISPLAY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BACKUP_DISPLAY, ofaBackupDisplayClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaBackupDisplay;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaBackupDisplayClass;

GType ofa_backup_display_get_type( void ) G_GNUC_CONST;

void  ofa_backup_display_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										const gchar *uri );

G_END_DECLS

#endif /* __OFA_BACKUP_DISPLAY_H__ */
