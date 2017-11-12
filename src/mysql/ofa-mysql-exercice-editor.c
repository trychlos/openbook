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

#include "my/my-date.h"
#include "my/my-ibin.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbprovider.h"

#include "mysql/ofa-mysql-connect.h"
#include "mysql/ofa-mysql-dossier-editor.h"
#include "mysql/ofa-mysql-exercice-bin.h"
#include "mysql/ofa-mysql-exercice-editor.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	gchar               *settings_prefix;
	guint                rule;

	/* UI
	 */
	GtkSizeGroup        *group0;
	ofaMysqlExerciceBin *exercice_bin;
}
	ofaMysqlExerciceEditorPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-exercice-editor.ui";

static void          setup_bin( ofaMysqlExerciceEditor *self );
static void          on_exercice_bin_changed( myIBin *bin, ofaMysqlExerciceEditor *self );
//static gboolean      does_database_exist( ofaMysqlExerciceEditor *self, const gchar *database );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );
static void          ibin_apply( myIBin *instance );
static void          idbexercice_editor_iface_init( ofaIDBExerciceEditorInterface *iface );
static guint         idbexercice_editor_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaMysqlExerciceEditor, ofa_mysql_exercice_editor, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaMysqlExerciceEditor )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDBEXERCICE_EDITOR, idbexercice_editor_iface_init ))

static void
mysql_exercice_editor_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_finalize";
	ofaMysqlExerciceEditorPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ));

	/* free data members here */
	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_editor_parent_class )->finalize( instance );
}

static void
mysql_exercice_editor_dispose( GObject *instance )
{
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ));

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_exercice_editor_parent_class )->dispose( instance );
}

static void
ofa_mysql_exercice_editor_init( ofaMysqlExerciceEditor *self )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_instance_init";
	ofaMysqlExerciceEditorPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_MYSQL_EXERCICE_EDITOR( self ));

	priv = ofa_mysql_exercice_editor_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_mysql_exercice_editor_class_init( ofaMysqlExerciceEditorClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_exercice_editor_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_exercice_editor_finalize;
}

/**
 * ofa_mysql_exercice_editor_new:
 * @provider: the #ofaMysqlProvider for which the exercice is to be created.
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of the widget.
 *
 * Returns: a new #ofaMysqlExerciceEditor widget.
 */
ofaMysqlExerciceEditor *
ofa_mysql_exercice_editor_new( ofaMysqlDBProvider *provider, const gchar *settings_prefix, guint rule )
{
	ofaMysqlExerciceEditor *bin;
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_val_if_fail( provider && OFA_IS_MYSQL_DBPROVIDER( provider ), NULL );

	bin = g_object_new( OFA_TYPE_MYSQL_EXERCICE_EDITOR, NULL );

	priv = ofa_mysql_exercice_editor_get_instance_private( bin );

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	priv->rule = rule;

	setup_bin( bin );

	return( bin );
}

static void
setup_bin( ofaMysqlExerciceEditor *self )
{
	ofaMysqlExerciceEditorPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *parent;
	GtkSizeGroup *group_bin;

	priv = ofa_mysql_exercice_editor_get_instance_private( self );

	priv->group0 = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mee-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mee-exercice-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->exercice_bin = ofa_mysql_exercice_bin_new( priv->settings_prefix, priv->rule );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->exercice_bin ));
	g_signal_connect( priv->exercice_bin, "my-ibin-changed", G_CALLBACK( on_exercice_bin_changed ), self );
	if(( group_bin = my_ibin_get_size_group( MY_IBIN( priv->exercice_bin ), 0 ))){
		my_utils_size_group_add_size_group( priv->group0, group_bin );
	}

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_exercice_bin_changed( myIBin *bin, ofaMysqlExerciceEditor *self )
{
	g_signal_emit_by_name( self, "my-ibin-changed" );
}

#if 0
static gboolean
does_database_exist( ofaMysqlExerciceEditor *self, const gchar *database )
{
	ofaIDBDossierEditor *dossier_editor;
	gboolean exists;
	ofaMysqlConnect *connect;

	exists = FALSE;

	dossier_editor = ofa_idbexercice_editor_get_dossier_editor( OFA_IDBEXERCICE_EDITOR( self ));
	g_return_val_if_fail( dossier_editor && OFA_IS_MYSQL_DOSSIER_EDITOR( dossier_editor ), FALSE );

	connect = ofa_mysql_dossier_editor_get_connect( OFA_MYSQL_DOSSIER_EDITOR( dossier_editor ));
	g_return_val_if_fail( connect && OFA_IS_MYSQL_CONNECT( connect ), FALSE );

	if( ofa_idbconnect_is_opened( OFA_IDBCONNECT( connect ))){
		exists = ofa_mysql_connect_does_database_exist( connect, database );
	}

	return( exists );
}
#endif

/**
 * ofa_mysql_exercice_editor_get_database:
 * @period: this #ofaMysqlExerciceMeta object.
 *
 * Returns: the database name.
 *
 * The returned string is owned by the @period object, and should not
 * be freed by the caller.
 */
const gchar *
ofa_mysql_exercice_editor_get_database( ofaMysqlExerciceEditor *editor )
{
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_val_if_fail( editor && OFA_IS_MYSQL_EXERCICE_EDITOR( editor ), NULL );

	priv = ofa_mysql_exercice_editor_get_instance_private( editor );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( ofa_mysql_exercice_bin_get_database( priv->exercice_bin ));
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
	iface->is_valid = ibin_is_valid;
	iface->apply = ibin_apply;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_ibin_get_size_group";
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ), NULL );

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: invalid column=%u", thisfn, column );

	return( NULL );
}

gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	ofaMysqlExerciceEditorPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ), FALSE );

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	ok = my_ibin_is_valid( MY_IBIN( priv->exercice_bin ), msgerr );

#if 0
	const gchar *database;
	gboolean exists;
	if( ok ){
		database = ofa_mysql_exercice_bin_get_database( priv->exercice_bin );
		exists = does_database_exist( OFA_MYSQL_EXERCICE_EDITOR( instance ), database );
		//g_debug( "ofa_idbexercice_editor_is_valid: database=%s, exists=%s", database, exists ? "True":"False" );

		switch( priv->rule ){
			case HUB_RULE_DOSSIER_NEW:
				if( exists ){
					ok = FALSE;
					if( msgerr ){
						*msgerr = g_strdup_printf( _( "Database '%s' already exists" ), database );
					}
				}
				break;
		}
	}
#endif

	return( ok );
}

static void
ibin_apply( myIBin *instance )
{
	ofaMysqlExerciceEditorPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_EXERCICE_EDITOR( instance ));

	priv = ofa_mysql_exercice_editor_get_instance_private( OFA_MYSQL_EXERCICE_EDITOR( instance ));

	g_return_if_fail( !priv->dispose_has_run );

	my_ibin_apply( MY_IBIN( priv->exercice_bin ));
}

/*
 * ofaIDBExerciceEditor interface management
 */
static void
idbexercice_editor_iface_init( ofaIDBExerciceEditorInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_exercice_editor_idbexercice_editor_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idbexercice_editor_get_interface_version;
}

static guint
idbexercice_editor_get_interface_version( void )
{
	return( 1 );
}
