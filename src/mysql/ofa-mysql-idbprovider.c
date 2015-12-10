/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-settings.h"
#include "api/my-utils.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-ifile-meta.h"
#include "api/ofa-ifile-period.h"

#include "ofa-mysql.h"
#include "ofa-mysql-connect.h"
#include "ofa-mysql-editor-display.h"
#include "ofa-mysql-editor-enter.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-meta.h"
#include "ofa-mysql-period.h"

static guint           idbprovider_get_interface_version( const ofaIDBProvider *instance );
static ofaIFileMeta   *idbprovider_new_meta( void );
static ofaIDBConnect  *idbprovider_new_connect( void );
static ofaIDBEditor   *idbprovider_new_editor( gboolean editable );

/*
 * #ofaIDBProvider interface setup
 */
void
ofa_mysql_idbprovider_iface_init( ofaIDBProviderInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_idbprovider_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbprovider_get_interface_version;
	iface->get_provider_name = ofa_mysql_idbprovider_get_provider_name;
	iface->new_meta = idbprovider_new_meta;
	iface->new_connect = idbprovider_new_connect;
	iface->new_editor = idbprovider_new_editor;
}

/*
 * the version of the #ofaIDBProvider interface implemented by the module
 */
static guint
idbprovider_get_interface_version( const ofaIDBProvider *instance )
{
	return( 1 );
}

/*
 * the provider name identifier
 */
const gchar *
ofa_mysql_idbprovider_get_provider_name( const ofaIDBProvider *instance )
{
	return( "MySQL" );
}

/*
 * instanciates a new ofaIFileMeta object
 */
static ofaIFileMeta *
idbprovider_new_meta( void )
{
	ofaMySQLMeta *meta;

	meta = ofa_mysql_meta_new();

	return( OFA_IFILE_META( meta ));
}

/*
 * instanciates a new ofaIDBConnect object
 */
static ofaIDBConnect *
idbprovider_new_connect( void )
{
	ofaMySQLConnect *connect;

	connect = ofa_mysql_connect_new();

	return( OFA_IDBCONNECT( connect ));
}

static ofaIDBEditor *
idbprovider_new_editor( gboolean editable )
{
	GtkWidget *widget;

	widget = editable ? ofa_mysql_editor_enter_new() : ofa_mysql_editor_display_new();

	return( OFA_IDBEDITOR( widget ));
}
