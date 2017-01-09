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

#ifndef __OPENBOOK_API_OFA_IDBBACKUP_H__
#define __OPENBOOK_API_OFA_IDBBACKUP_H__

/**
 * SECTION: idbbackup
 * @title: ofaIDBBackup
 * @short_description: The DMBS Connection Interface
 * @include: openbook/ofa-idbbackup.h
 *
 * The ofaIDB<...> interfaces serie let the user choose and manage
 * different DBMS backends.
 *
 * This #ofaIDBBackup is the interface a connection object instanciated
 * by a DBMS backend should implement for backuping/restoring its datas.
 */

#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDBBACKUP                      ( ofa_idbbackup_get_type())
#define OFA_IDBBACKUP( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBBACKUP, ofaIDBBackup ))
#define OFA_IS_IDBBACKUP( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBBACKUP ))
#define OFA_IDBBACKUP_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBBACKUP, ofaIDBBackupInterface ))

typedef struct _ofaIDBBackup                    ofaIDBBackup;
typedef struct _ofaIDBBackupInterface           ofaIDBBackupInterface;

/**
 * ofaIDBBackupInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @backup_db: [should]: backup all the datas for an exercice.
 *
 * This defines the interface that an #ofaIDBBackup should implement.
 */
struct _ofaIDBBackupInterface {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * backup_db:
	 * @instance: this #ofaIDBBackup connection.
	 * @hub: the #ofaHub object of the application.
	 * @connect: an administrative connection on the exercice to be
	 *  backuped.
	 * @msgerr: [out][allow-none]: a placeholder for an error message.
	 *
	 * Backup all the datas for the exercice adressed by the
	 * #ofaIDBExerciceMeta member of @connect.
	 *
	 * Returns: %TRUE if the exercice has been successfully backuped,
	 * %FALSE else.
	 *
	 * Since: version 1
	 */
	gboolean ( *backup_db )            ( ofaIDBBackup *instance,
											ofaHub *hub,
											ofaIDBConnect *connect,
											gchar **msgerr );
};

/*
 * Interface-wide
 */
GType    ofa_idbbackup_get_type                  ( void );

guint    ofa_idbbackup_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint    ofa_idbbackup_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
gboolean ofa_idbbackup_backup_db                 ( ofaIDBBackup *instance,
														ofaHub *hub,
														ofaIDBConnect *connect,
														gchar **msgerr );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBBACKUP_H__ */
