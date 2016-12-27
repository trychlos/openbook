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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"

#include "mysql/ofa-mysql-connect.h"
#include "mysql/ofa-mysql-dossier-bin.h"
#include "mysql/ofa-mysql-dossier-editor.h"
#include "mysql/ofa-mysql-root-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	guint               rule;

	/* UI
	 */
	GtkSizeGroup       *group0;
	ofaMysqlDossierBin *dossier_bin;
	ofaMysqlRootBin    *root_bin;
}
	ofaMysqlDossierEditorPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-dossier-editor.ui";

static void          idbdossier_editor_iface_init( ofaIDBDossierEditorInterface *iface );
static guint         idbdossier_editor_get_interface_version( void );
static GtkSizeGroup *idbdossier_editor_get_size_group( const ofaIDBDossierEditor *instance, guint column );
static gboolean      idbdossier_editor_is_valid( const ofaIDBDossierEditor *instance, gchar **message );
static gboolean      idbdossier_editor_apply( const ofaIDBDossierEditor *instance );
static void          idbdossier_editor_set_dossier_meta( const ofaIDBDossierEditor *instance, ofaIDBDossierMeta *dossier_meta );
static void          setup_bin( ofaMysqlDossierEditor *self );

G_DEFINE_TYPE_EXTENDED( ofaMysqlDossierEditor, ofa_mysql_dossier_editor, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlDossierEditor )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBDOSSIER_EDITOR, idbdossier_editor_iface_init ))

static void
mysql_dossier_editor_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_editor_parent_class )->finalize( instance );
}

static void
mysql_dossier_editor_dispose( GObject *instance )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ));

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_dossier_editor_parent_class )->dispose( instance );
}

static void
ofa_mysql_dossier_editor_init( ofaMysqlDossierEditor *self )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_instance_init";
	ofaMysqlDossierEditorPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_DOSSIER_EDITOR( self ));

	priv = ofa_mysql_dossier_editor_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_dossier_editor_class_init( ofaMysqlDossierEditorClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_dossier_editor_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_dossier_editor_finalize;
}

/*
 * ofaIDBDossierEditor interface management
 */
static void
idbdossier_editor_iface_init( ofaIDBDossierEditorInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_idbdossier_editor_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbdossier_editor_get_interface_version;
	iface->get_size_group = idbdossier_editor_get_size_group;
	iface->is_valid = idbdossier_editor_is_valid;
	iface->apply = idbdossier_editor_apply;
	iface->set_dossier_meta = idbdossier_editor_set_dossier_meta;
}

static guint
idbdossier_editor_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
idbdossier_editor_get_size_group( const ofaIDBDossierEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_dossier_editor_get_size_group";
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_DOSSIER_EDITOR( instance ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: column=%u", thisfn, column );
	return( NULL );
}

/*
 * all the informations are optional
 */
static gboolean
idbdossier_editor_is_valid( const ofaIDBDossierEditor *instance, gchar **message )
{
	ofaMysqlDossierEditorPrivate *priv;
	gboolean ok;

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	ok = ofa_mysql_dossier_bin_is_valid( priv->dossier_bin, message ) &&
			ofa_mysql_root_bin_is_valid( priv->root_bin, message );

	return( ok );
}

static gboolean
idbdossier_editor_apply( const ofaIDBDossierEditor *instance )
{
	ofaMysqlDossierEditorPrivate *priv;
	gboolean ok;

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	ok = ofa_mysql_dossier_bin_apply( priv->dossier_bin ) &&
			ofa_mysql_root_bin_apply( priv->root_bin );

	return( ok );
}

static void
idbdossier_editor_set_dossier_meta( const ofaIDBDossierEditor *instance, ofaIDBDossierMeta *dossier_meta )
{
	ofaMysqlDossierEditorPrivate *priv;

	priv = ofa_mysql_dossier_editor_get_instance_private( OFA_MYSQL_DOSSIER_EDITOR( instance ));

	ofa_mysql_dossier_bin_set_dossier_meta( priv->dossier_bin, dossier_meta );
	ofa_mysql_root_bin_set_dossier_meta( priv->root_bin, dossier_meta );
}

/**
 * ofa_mysql_dossier_editor_new:
 * @rule: the usage of the widget.
 *
 * Returns: a new #ofaMysqlDossierEditor widget.
 *
 * The dossier editor for MySQL embeds both:
 * - the instance informations
 * - the root credentials.
 */
ofaMysqlDossierEditor *
ofa_mysql_dossier_editor_new( guint rule )
{
	ofaMysqlDossierEditor *bin;
	ofaMysqlDossierEditorPrivate *priv;

	bin = g_object_new( OFA_TYPE_MYSQL_DOSSIER_EDITOR, NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( bin );

	priv->rule = rule;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlDossierEditor *self )
{
	ofaMysqlDossierEditorPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent;
	ofaIDBProvider *provider;

	priv = ofa_mysql_dossier_editor_get_instance_private( self );

	priv->group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mde-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	provider = ofa_idbdossier_editor_get_provider( OFA_IDBDOSSIER_EDITOR( self ));
	g_return_if_fail( provider && OFA_IS_MYSQL_DBPROVIDER( provider ));

	priv->dossier_bin = ofa_mysql_dossier_bin_new( OFA_MYSQL_DBPROVIDER( provider ), priv->rule );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mde-dossier-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dossier_bin ));
	my_utils_size_group_add_size_group( priv->group0, ofa_mysql_dossier_bin_get_size_group( priv->dossier_bin, 0 ));

	priv->root_bin = ofa_mysql_root_bin_new( OFA_MYSQL_DBPROVIDER( provider ), priv->rule );
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mde-root-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->root_bin ));
	my_utils_size_group_add_size_group( priv->group0, ofa_mysql_root_bin_get_size_group( priv->root_bin, 0 ));

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_mysql_dossier_editor_get_host:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS host.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_editor_get_host( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( ofa_mysql_dossier_bin_get_host( priv->dossier_bin ));
}

/**
 * ofa_mysql_dossier_editor_get_socket:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS listening socket.
 *
 * The returned string is owned by the @editor, and should not be
 * released by the caller.
 */
const gchar *
ofa_mysql_dossier_editor_get_socket( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), NULL );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( ofa_mysql_dossier_bin_get_socket( priv->dossier_bin ));
}

/**
 * ofa_mysql_dossier_editor_get_port:
 * @editor: this #ofaMysqlDossierEditor instance.
 *
 * Returns: the DBMS listening port.
 */
guint
ofa_mysql_dossier_editor_get_port( ofaMysqlDossierEditor *editor )
{
	ofaMysqlDossierEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_DOSSIER_EDITOR( editor ), 0 );

	priv = ofa_mysql_dossier_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( ofa_mysql_dossier_bin_get_port( priv->dossier_bin ));
}
