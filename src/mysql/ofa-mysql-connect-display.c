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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-ibin.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-idbprovider.h"

#include "ofa-mysql-connect-display.h"
#include "ofa-mysql-dossier-meta.h"
#include "ofa-mysql-exercice-meta.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization
	 */
	ofaMysqlConnect *connect;
	gchar           *style;

	/* UI
	 */
	GtkSizeGroup    *group0;
}
	ofaMysqlConnectDisplayPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-connect-display.ui";

static void          setup_bin( ofaMysqlConnectDisplay *self );
static void          setup_data( ofaMysqlConnectDisplay *self );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );

G_DEFINE_TYPE_EXTENDED( ofaMysqlConnectDisplay, ofa_mysql_connect_display, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlConnectDisplay )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

static void
mysql_connect_display_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_finalize";
	ofaMysqlConnectDisplayPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT_DISPLAY( instance ));

	/* free data members here */
	priv = ofa_mysql_connect_display_get_instance_private( OFA_MYSQL_CONNECT_DISPLAY( instance ));

	g_free( priv->style );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_display_parent_class )->finalize( instance );
}

static void
mysql_connect_display_dispose( GObject *instance )
{
	ofaMysqlConnectDisplayPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT_DISPLAY( instance ));

	priv = ofa_mysql_connect_display_get_instance_private( OFA_MYSQL_CONNECT_DISPLAY( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_display_parent_class )->dispose( instance );
}

static void
ofa_mysql_connect_display_init( ofaMysqlConnectDisplay *self )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_instance_init";
	ofaMysqlConnectDisplayPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_CONNECT_DISPLAY( self ));

	priv = ofa_mysql_connect_display_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_connect_display_class_init( ofaMysqlConnectDisplayClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_connect_display_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_connect_display_finalize;
}

/**
 * ofa_mysql_connect_display_new:
 * connect: the #ofaMysqlConnect object to display informations from.
 * @style: [allow-none]: the display style name.
 *
 * Returns: a new #ofaMysqlConnectDisplay instance as a #GtkWidget.
 */
ofaMysqlConnectDisplay *
ofa_mysql_connect_display_new( ofaMysqlConnect *connect, const gchar *style )
{
	ofaMysqlConnectDisplay *bin;
	ofaMysqlConnectDisplayPrivate *priv;

	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_CONNECT_DISPLAY, NULL );

	priv = ofa_mysql_connect_display_get_instance_private( bin );

	priv->connect = connect;
	priv->style = g_strdup( style );

	setup_bin( bin );
	setup_data( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlConnectDisplay *self )
{
	ofaMysqlConnectDisplayPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	priv = ofa_mysql_connect_display_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mcdb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mcdb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_data( ofaMysqlConnectDisplay *self )
{
	ofaMysqlConnectDisplayPrivate *priv;
	GtkWidget *label;
	const gchar *cstr;
	guint port_num;
	gchar *text;

	priv = ofa_mysql_connect_display_get_instance_private( self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	if( my_strlen( priv->style )){
		my_style_add( label, priv->style );
	}
	cstr = ofa_mysql_connect_get_host( priv->connect );
	gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? cstr : "" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "port" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	if( my_strlen( priv->style )){
		my_style_add( label, priv->style );
	}
	port_num = ofa_mysql_connect_get_port( priv->connect );
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
	} else {
		gtk_label_set_text( GTK_LABEL( label ), "" );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "socket" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	if( my_strlen( priv->style )){
		my_style_add( label, priv->style );
	}
	cstr = ofa_mysql_connect_get_socket( priv->connect );
	gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? cstr : "" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	if( my_strlen( priv->style )){
		my_style_add( label, priv->style );
	}
	cstr = ofa_mysql_connect_get_database( priv->connect );
	gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? cstr : "" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	if( my_strlen( priv->style )){
		my_style_add( label, priv->style );
	}
	cstr = ofa_idbconnect_get_account( OFA_IDBCONNECT( priv->connect ));
	gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? cstr : "" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "password" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	if( my_strlen( priv->style )){
		my_style_add( label, priv->style );
	}
	cstr = ofa_idbconnect_get_password( OFA_IDBCONNECT( priv->connect ));
	gtk_label_set_text( GTK_LABEL( label ), my_strlen( cstr ) ? "******" : "" );
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_ibin_get_size_group";
	ofaMysqlConnectDisplayPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_CONNECT_DISPLAY( instance ), NULL );

	priv = ofa_mysql_connect_display_get_instance_private( OFA_MYSQL_CONNECT_DISPLAY( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
}
