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

#include "api/my-utils.h"
#include "api/ofa-dossier-misc.h"
#include "api/ofa-settings.h"

#include "ofa-mysql.h"
#include "ofa-mysql-idbms.h"
#include "ofa-mysql-connect-display-bin.h"

/* private instance data
 */
struct _ofaMySQLConnectDisplayBinPrivate {
	gboolean        dispose_has_run;

	/* runtime data
	 */
	const ofaIDbms *instance;
	gchar          *dname;

	/* UI
	 */
	GtkSizeGroup   *group0;
};

static const gchar *st_bin_xml          = PROVIDER_DATADIR "/ofa-mysql-connect-display-bin.ui";

G_DEFINE_TYPE( ofaMySQLConnectDisplayBin, ofa_mysql_connect_display_bin, GTK_TYPE_BIN )

static void setup_composite( ofaMySQLConnectDisplayBin *bin );

static void
mysql_connect_display_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_bin_finalize";
	ofaMySQLConnectDisplayBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT_DISPLAY_BIN( instance ));

	/* free data members here */
	priv = OFA_MYSQL_CONNECT_DISPLAY_BIN( instance )->priv;

	g_free( priv->dname );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_display_bin_parent_class )->finalize( instance );
}

static void
mysql_connect_display_bin_dispose( GObject *instance )
{
	ofaMySQLConnectDisplayBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_CONNECT_DISPLAY_BIN( instance ));

	priv = OFA_MYSQL_CONNECT_DISPLAY_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_connect_display_bin_parent_class )->dispose( instance );
}

static void
ofa_mysql_connect_display_bin_init( ofaMySQLConnectDisplayBin *self )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_bin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_CONNECT_DISPLAY_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN, ofaMySQLConnectDisplayBinPrivate );
}

static void
ofa_mysql_connect_display_bin_class_init( ofaMySQLConnectDisplayBinClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_connect_display_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_connect_display_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_connect_display_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLConnectDisplayBinPrivate ));
}

/**
 * ofa_mysql_connect_display_bin_new:
 * @instance: the provider instance
 * @dname: the dossier name
 *
 * Returns: a new #ofaMySQLConnectDisplayBin instance as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_connect_display_bin_new( const ofaIDbms *instance, const gchar *dname )
{
	ofaMySQLConnectDisplayBin *bin;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( my_strlen( dname ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_CONNECT_DISPLAY_BIN, NULL );

	bin->priv->instance = instance;
	bin->priv->dname = g_strdup( dname );

	setup_composite( bin );

	return( GTK_WIDGET( bin ));
}

static void
setup_composite( ofaMySQLConnectDisplayBin *bin )
{
	ofaMySQLConnectDisplayBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;
	GtkWidget *label;
	gchar *text;
	gint port_num;

	priv = bin->priv;
	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "mcdb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mcdb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "provider" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), ofa_mysql_idbms_get_provider_name( priv->instance ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = ofa_settings_dossier_get_string( priv->dname, SETTINGS_HOST );
	if( !my_strlen( text )){
		g_free( text );
		text = g_strdup( "localhost" );
	}
	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "socket" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = ofa_settings_dossier_get_string( priv->dname, SETTINGS_SOCKET );
	if( my_strlen( text )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = ofa_dossier_misc_get_current_dbname( priv->dname );
	gtk_label_set_text( GTK_LABEL( label ), text );
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "port" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	port_num = ofa_settings_dossier_get_int( priv->dname, SETTINGS_PORT );
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
	}

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_mysql_connect_display_bin_get_size_group:
 * @instance: the provider instance
 * @bin: this #ofaMySQLConnectDisplayBin composite widget
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup for the specified @column.
 */
GtkSizeGroup *
ofa_mysql_connect_display_bin_get_size_group( const ofaIDbms *instance, GtkWidget *bin, guint column )
{
	ofaMySQLConnectDisplayBinPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( bin && OFA_IS_MYSQL_CONNECT_DISPLAY_BIN( bin ), NULL );

	priv = OFA_MYSQL_CONNECT_DISPLAY_BIN( bin )->priv;

	if( !priv->dispose_has_run ){
		if( column == 0 ){
			return( priv->group0 );
		}
	}

	g_return_val_if_reached( NULL );
}
