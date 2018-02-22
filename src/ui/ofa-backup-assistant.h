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

#ifndef __OFA_BACKUP_ASSISTANT_H__
#define __OFA_BACKUP_ASSISTANT_H__

/**
 * SECTION: ofa_import
 * @short_description: #ofaBackupAssistant class definition.
 * @include: ui/ofa-backup-assistant.h
 *
 * Guide the user through the process of saving an exercice.
 *
 * The process is very simple: just select a destination file, and
 * optionally enter a comment. That's all.
 *
 * Just an assistant has been built to have to distinct enter pages,
 * and not to mix the comment on the same window than the chooser.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BACKUP_ASSISTANT                ( ofa_backup_assistant_get_type())
#define OFA_BACKUP_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BACKUP_ASSISTANT, ofaBackupAssistant ))
#define OFA_BACKUP_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BACKUP_ASSISTANT, ofaBackupAssistantClass ))
#define OFA_IS_BACKUP_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BACKUP_ASSISTANT ))
#define OFA_IS_BACKUP_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BACKUP_ASSISTANT ))
#define OFA_BACKUP_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BACKUP_ASSISTANT, ofaBackupAssistantClass ))

typedef struct {
	/*< public members >*/
	GtkAssistant      parent;
}
	ofaBackupAssistant;

typedef struct {
	/*< public members >*/
	GtkAssistantClass parent;
}
	ofaBackupAssistantClass;

GType ofa_backup_assistant_get_type( void ) G_GNUC_CONST;

void  ofa_backup_assistant_run     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_BACKUP_ASSISTANT_H__ */
