/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_RESTORE_ASSISTANT_H__
#define __OFA_RESTORE_ASSISTANT_H__

/**
 * SECTION: ofa_import
 * @short_description: #ofaRestoreAssistant class definition.
 * @include: ui/ofa-restore-assistant.h
 *
 * Guide the user through the process of restoring an exercice.
 *
 * Restoring only happens on the current exercice (though maybe on a
 * newly created dossier).
 * The operation requires:
 * - the DBMS root account and password,
 * - the dossier admin account and password.
 * All these datas must be entered by the user as the Openbook software
 * cannot guess them.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_RESTORE_ASSISTANT                ( ofa_restore_assistant_get_type())
#define OFA_RESTORE_ASSISTANT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RESTORE_ASSISTANT, ofaRestoreAssistant ))
#define OFA_RESTORE_ASSISTANT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RESTORE_ASSISTANT, ofaRestoreAssistantClass ))
#define OFA_IS_RESTORE_ASSISTANT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RESTORE_ASSISTANT ))
#define OFA_IS_RESTORE_ASSISTANT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RESTORE_ASSISTANT ))
#define OFA_RESTORE_ASSISTANT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RESTORE_ASSISTANT, ofaRestoreAssistantClass ))

typedef struct {
	/*< public members >*/
	GtkAssistant      parent;
}
	ofaRestoreAssistant;

typedef struct {
	/*< public members >*/
	GtkAssistantClass parent;
}
	ofaRestoreAssistantClass;

GType   ofa_restore_assistant_get_type( void ) G_GNUC_CONST;

void    ofa_restore_assistant_run     ( ofaIGetter *getter,
											GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_RESTORE_ASSISTANT_H__ */
