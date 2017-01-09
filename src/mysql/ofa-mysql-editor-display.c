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

#include "my/my-utils.h"

#include "api/ofa-idbeditor.h"
#include "api/ofa-idbprovider.h"

#include "ofa-mysql-dossier-meta.h"
#include "ofa-mysql-editor-display.h"
#include "ofa-mysql-exercice-meta.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkSizeGroup   *group0;
}
	ofaMySQLEditorDisplayPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-editor-display.ui";

static void          idbeditor_iface_init( ofaIDBEditorInterface *iface );
static guint         idbeditor_get_interface_version( void );
static void          idbeditor_set_meta( ofaIDBEditor *instance, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period );
static GtkSizeGroup *idbeditor_get_size_group( const ofaIDBEditor *instance, guint column );
static void          setup_bin( ofaMySQLEditorDisplay *bin );

G_DEFINE_TYPE_EXTENDED( ofaMySQLEditorDisplay, ofa_mysql_editor_display, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMySQLEditorDisplay )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEDITOR, idbeditor_iface_init ))

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

	priv = ofa_mysql_editor_display_get_instance_private( OFA_MYSQL_EDITOR_DISPLAY( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

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
	ofaMySQLEditorDisplayPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EDITOR_DISPLAY( self ));

	priv = ofa_mysql_editor_display_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_editor_display_class_init( ofaMySQLEditorDisplayClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_editor_display_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_editor_display_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_editor_display_finalize;
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
idbeditor_get_interface_version( void )
{
	return( 1 );
}

static void
idbeditor_set_meta( ofaIDBEditor *instance, const ofaIDBDossierMeta *dossier_meta, const ofaIDBExerciceMeta *period )
{
	GtkWidget *label;
	gchar *text;
	guint port_num;

	g_return_if_fail( instance && OFA_IS_MYSQL_EDITOR_DISPLAY( instance ));
	g_return_if_fail( !dossier_meta || OFA_IS_MYSQL_DOSSIER_META( dossier_meta ));
	g_return_if_fail( !period || OFA_IS_MYSQL_EXERCICE_META( period ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "host" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = dossier_meta ? g_strdup( ofa_mysql_dossier_meta_get_host( OFA_MYSQL_DOSSIER_META( dossier_meta ))) : NULL;
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
	text = dossier_meta ? g_strdup( ofa_mysql_dossier_meta_get_socket( OFA_MYSQL_DOSSIER_META( dossier_meta ))) : NULL;
	if( my_strlen( text )){
		gtk_label_set_text( GTK_LABEL( label ), text );
	}
	g_free( text );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "port" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	port_num = dossier_meta ? ofa_mysql_dossier_meta_get_port( OFA_MYSQL_DOSSIER_META( dossier_meta )) : 0;
	if( port_num > 0 ){
		text = g_strdup_printf( "%d", port_num );
		gtk_label_set_text( GTK_LABEL( label ), text );
		g_free( text );
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "database" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	text = period ? g_strdup( ofa_mysql_exercice_meta_get_database( OFA_MYSQL_EXERCICE_META( period ))) : NULL;
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

	priv = ofa_mysql_editor_display_get_instance_private( OFA_MYSQL_EDITOR_DISPLAY( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
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

	priv = ofa_mysql_editor_display_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

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
