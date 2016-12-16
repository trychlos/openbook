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

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-core.h"
#include "api/ofa-iabout.h"
#include "api/ofa-igetter.h"

#include "ofa-mysql-ident.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaMysqlIdentPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-about.ui";

static void       iident_iface_init( myIIdentInterface *iface );
static gchar     *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar     *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar     *iident_get_version( const myIIdent *instance, void *user_data );
static void       iabout_iface_init( ofaIAboutInterface *iface );
static GtkWidget *iabout_do_init( ofaIAbout *instance, ofaIGetter *getter );

G_DEFINE_TYPE_EXTENDED( ofaMysqlIdent, ofa_mysql_ident, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaMysqlIdent )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IABOUT, iabout_iface_init ))

static void
mysql_ident_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_ident_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_IDENT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_ident_parent_class )->finalize( instance );
}

static void
mysql_ident_dispose( GObject *instance )
{
	ofaMysqlIdentPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_IDENT( instance ));

	priv = ofa_mysql_ident_get_instance_private( OFA_MYSQL_IDENT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_ident_parent_class )->dispose( instance );
}

static void
ofa_mysql_ident_init( ofaMysqlIdent *self )
{
	static const gchar *thisfn = "ofa_mysql_ident_init";
	ofaMysqlIdentPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_ident_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_ident_class_init( ofaMysqlIdentClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_ident_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_ident_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_ident_finalize;
}

/*
 * #myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_ident_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "MySQL" ));
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( "MySQL Library" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( PACKAGE_VERSION ));
}

/*
 * #ofaIAbout interface management
 */
static void
iabout_iface_init( ofaIAboutInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_ident_iabout_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->do_init = iabout_do_init;
}

static GtkWidget *
iabout_do_init( ofaIAbout *instance, ofaIGetter *getter )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *box, *toplevel, *label, *grid;
	gchar *str, *tmp;
	const gchar **authors, **it;
	gint row;

	box = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	my_utils_widget_set_margins( box, 4, 4, 4, 4 );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "top-window" );
	g_return_val_if_fail( object && GTK_IS_WINDOW( object ), NULL );
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( box ), GTK_WINDOW( toplevel ), "top" );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "name" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	str = iident_get_canon_name( MY_IIDENT( instance ), NULL );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "version" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	tmp = iident_get_version( MY_IIDENT( instance ), NULL );
	str = g_strdup_printf( _( "Version %s" ), tmp );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );
	g_free( tmp );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "comment" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	str = iident_get_display_name( MY_IIDENT( instance ), NULL );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "authors-grid" );
	g_return_val_if_fail( grid && GTK_IS_GRID( grid ), NULL );
	authors = ofa_core_get_authors();
	it = authors;
	row = 0;
	while( *it ){
		label = gtk_label_new( *it );
		gtk_widget_set_hexpand( label, TRUE );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, row, 1, 1 );
		it++;
		row++;
	}

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "copyright" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	gtk_label_set_text( GTK_LABEL( label ), ofa_core_get_copyright());

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );

	return( box );
}
