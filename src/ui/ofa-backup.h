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

#ifndef __OFA_BACKUP_H__
#define __OFA_BACKUP_H__

/**
 * SECTION: ofa_backup
 * @short_description: #ofaBackup class definition.
 * @include: ui/ofa-backup.h
 *
 * Backup the database behind the dossier.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BACKUP                ( ofa_backup_get_type())
#define OFA_BACKUP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BACKUP, ofaBackup ))
#define OFA_BACKUP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BACKUP, ofaBackupClass ))
#define OFA_IS_BACKUP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BACKUP ))
#define OFA_IS_BACKUP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BACKUP ))
#define OFA_BACKUP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BACKUP, ofaBackupClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaBackup;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaBackupClass;

GType ofa_backup_get_type( void ) G_GNUC_CONST;

void  ofa_backup_run     ( ofaIGetter *getter,
								GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_BACKUP_H__ */
