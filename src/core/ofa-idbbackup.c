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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbbackup.h"
#include "api/ofa-idbconnect.h"

#define IDBBACKUP_LAST_VERSION           1

static guint st_initializations         = 0;	/* interface initialization count */

static GType register_type( void );
static void  interface_base_init( ofaIDBBackupInterface *klass );
static void  interface_base_finalize( ofaIDBBackupInterface *klass );

/**
 * ofa_idbbackup_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbbackup_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbbackup_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbbackup_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBBackupInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBBackup", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBBackupInterface *klass )
{
	static const gchar *thisfn = "ofa_idbbackup_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBBackupInterface *klass )
{
	static const gchar *thisfn = "ofa_idbbackup_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbbackup_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbbackup_get_interface_last_version( void )
{
	return( IDBBACKUP_LAST_VERSION );
}

/**
 * ofa_idbbackup_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idbbackup_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBBACKUP );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBBackupInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBBackupInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBBackup::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbbackup_backup_db:
 * @backup: this #ofaIDBBackup instance.
 * @hub: the #ofaHub object of the application.
 * @connect: an administrative connection on the exercice to be
 *  backuped.
 * @msgerr: [out][allow-none]: a placeholder for an error message.
 *
 * Backup all the datas for the exercice adressed by the
 * #ofaIDBExerciceMeta member of @connect.
 *
 * Returns: %TRUE if the exercice has been successfully backuped.
 */
gboolean
ofa_idbbackup_backup_db( ofaIDBBackup *instance, ofaHub *hub, ofaIDBConnect *connect, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_idbbackup_backup_db";
	gboolean ok;

	g_debug( "%s: instance=%p, hub=%p, connect=%p, msgerr=%p",
			thisfn, ( void * ) instance, ( void * ) hub, ( void * ) connect, ( void * ) msgerr );

	g_return_val_if_fail( instance && OFA_IS_IDBBACKUP( instance ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), FALSE );

	if( OFA_IDBBACKUP_GET_INTERFACE( connect )->backup_db ){
		ok = OFA_IDBBACKUP_GET_INTERFACE( connect )->backup_db( instance, hub, connect, msgerr );
		return( ok );
	}

	g_info( "%s: ofaIDBBackup's %s implementation does not provide 'backup_db()' method",
			thisfn, G_OBJECT_TYPE_NAME( connect ));
	return( FALSE );
}
