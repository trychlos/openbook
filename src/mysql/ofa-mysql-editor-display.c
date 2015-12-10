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
#include "api/my-settings.h"
#include "api/ofa-idbeditor.h"

#include "ofa-mysql.h"
#include "ofa-mysql-editor-display.h"
#include "ofa-mysql-idbprovider.h"
#include "ofa-mysql-meta.h"
#include "ofa-mysql-period.h"

/* private instance data
 */
struct _ofaMySQLEditorDisplayPrivate {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkSizeGroup   *group0;
};

static const gchar *st_bin_xml          = PROVIDER_DATADIR "/ofa-mysql-editor-display.ui";

static void          idbeditor_iface_init( ofaIDBEditorInterface *iface );
static guint         idbeditor_get_interface_version( const ofaIDBEditor *instance );
static void          idbeditor_set_meta( ofaIDBEditor *instance, const ofaIFileMeta *meta, const ofaIFilePeriod *period );
static GtkSizeGroup *idbeditor_get_size_group( const ofaIDBEditor *instance, guint column );
static void          setup_bin( ofaMySQLEditorDisplay *bin );

G_DEFINE_TYPE_EXTENDED( ofaMySQLEditorDisplay, ofa_mysql_editor_display, GTK_TYPE_BIN, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEDITOR, idbeditor_iface_init ));

static void
mysql_editor_display_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_editor_display_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_DISPLAY( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_editor_display_parent_class )->finalize( instance );
}

static void
mysql_editor_display_dispose( GObject *instance )
{
	ofaMySQLEditorDisplayPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_DISPLAY( instance ));

	priv = OFA_MYSQL_EDITOR_DISPLAY( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_editor_display_parent_class )->dispose( instance );
}

static void
ofa_mysql_editor_display_init( ofaMySQLEditorDisplay *self )
{
	static const gchar *thisfn = "ofa_mysql_editor_display_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EDITOR_DISPLAY( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_MYSQL_EDITOR_DISPLAY, ofaMySQLEditorDisplayPrivate );
}

static void
ofa_mysql_editor_display_class_init( ofaMySQLEditorDisplayClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_editor_display_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_editor_display_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_editor_display_finalize;

	g_type_class_add_private( klass, sizeof( ofaMySQLEditorDisplayPrivate ));
}

/*
 * ofaIDBEditor interface management
 */
static void
idbeditor_iface_init( ofaIDBEditorInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_editor_display_idbeditor_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbeditor_get_interface_version;
	iface->set_meta = idbeditor_set_meta;
	iface->get_size_group = idbeditor_get_size_group;
}

static guint
idbeditor_get_interface_version( const ofaIDBEditor *instance )
{
	return( 1 );
}

static void
idbeditor_set_meta( ofaIDBEditor *instance, const ofaIFileMeta *meta, const ofaIFilePeriod *period )
{
	GtkWidget *label;
	gchar *text;
	guint port_num;

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_DISPLAY( instance ));
	g_return_if_fail( !meta || OFA_IS_MYSQL_META( meta ));
	g_return_if_fail( !period || OFA_IS_MYSQL_PERIOD( period ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = meta ? g_strdup( ofa_mysql_meta_get_host( OFA_MYSQL_META( meta ))) : NULL;
	if( !my_strlen( text )){
		g_free( text );
		text = g_strdup( "localhost" );
	}
	if( my_strlen( text )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "socket" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = meta ? g_strdup( ofa_mysql_meta_get_socket( OFA_MYSQL_META( meta ))) : NULL;
	if( my_strlen( text )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "port" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	port_num = meta ? ofa_mysql_meta_get_port( OFA_MYSQL_META( meta )) : 0;
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = period ? g_strdup( ofa_mysql_period_get_database( OFA_MYSQL_PERIOD( period ))) : NULL;
	if( my_strlen( text )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );
}


static GtkSizeGroup *
idbeditor_get_size_group( const ofaIDBEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_editor_display_get_size_group";
	ofaMySQLEditorDisplayPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EDITOR_DISPLAY( instance ), NULL );

	priv = OFA_MYSQL_EDITOR_DISPLAY( instance )->priv;

	if( !priv->dispose_has_run ){

		if( column == 0 ){
			return( priv->group0 );
		}
		g_warning( "%s: column=%u", thisfn, column );
		return( NULL );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_mysql_editor_display_new:
 *
 * Returns: a new #ofaMySQLEditorDisplay instance as a #GtkWidget.
 */
GtkWidget *
ofa_mysql_editor_display_new( void )
{
	ofaMySQLEditorDisplay *bin;

	bin = g_object_new( OFA_TYPE_MYSQL_EDITOR_DISPLAY, NULL );

	setup_bin( bin );

	return( GTK_WIDGET( bin ));
}

static void
setup_bin( ofaMySQLEditorDisplay *bin )
{
	ofaMySQLEditorDisplayPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	priv = bin->priv;
	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "mcdb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mcdb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}