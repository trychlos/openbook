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
#include "api/ofa-extension.h"
#include "api/ofa-iabout.h"

#include "mysql/ofa-mysql-dbmodel.h"
#include "mysql/ofa-mysql-dbprovider.h"
#include "mysql/ofa-mysql-properties.h"

/*
 * The part below defines and implements the GTypeModule-derived class
 *  for this library.
 * See infra for the software extension API implementation.
 */
#define OFA_TYPE_MYSQL_MAIN                ( ofa_mysql_main_get_type())
#define OFA_MYSQL_MAIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_MAIN, ofaMysqlMain ))
#define OFA_MYSQL_MAIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_MAIN, ofaMysqlMainClass ))
#define OFA_IS_MYSQL_MAIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_MAIN ))
#define OFA_IS_MYSQL_MAIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_MAIN ))
#define OFA_MYSQL_MAIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_MAIN, ofaMysqlMainClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlMain;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlMainClass;

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaMysqlMainPrivate;

GType ofa_mysql_main_get_type( void );

static const gchar *st_resource_ui      = "/org/trychlos/openbook/mysql/ofa-mysql-about.ui";

static void       iident_iface_init( myIIdentInterface *iface );
static gchar     *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar     *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar     *iident_get_version( const myIIdent *instance, void *user_data );
static void       iabout_iface_init( ofaIAboutInterface *iface );
static GtkWidget *iabout_do_init( const ofaIAbout *instance );

G_DEFINE_DYNAMIC_TYPE_EXTENDED( ofaMysqlMain, ofa_mysql_main, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE_DYNAMIC( ofaMysqlMain )
		G_IMPLEMENT_INTERFACE_DYNAMIC( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE_DYNAMIC( OFA_TYPE_IABOUT, iabout_iface_init ))

static void
ofa_mysql_main_class_finalize( ofaMysqlMainClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_main_class_finalize";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
}

static void
mysql_main_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_mysql_main_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_MYSQL_MAIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_main_parent_class )->finalize( instance );
}

static void
mysql_main_dispose( GObject *instance )
{
	ofaMysqlMainPrivate *priv;

	g_return_if_fail( instance && OFA_IS_MYSQL_MAIN( instance ));

	priv = ofa_mysql_main_get_instance_private( OFA_MYSQL_MAIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_mysql_main_parent_class )->dispose( instance );
}

static void
ofa_mysql_main_init( ofaMysqlMain *self )
{
	static const gchar *thisfn = "ofa_mysql_main_init";
	ofaMysqlMainPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_mysql_main_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_mysql_main_class_init( ofaMysqlMainClass *klass )
{
	static const gchar *thisfn = "ofa_mysql_main_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = mysql_main_dispose;
	G_OBJECT_CLASS( klass )->finalize = mysql_main_finalize;
}

/*
 * #myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_mysql_main_iident_iface_init";

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
	static const gchar *thisfn = "ofa_mysql_main_iabout_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->do_init = iabout_do_init;
}

static GtkWidget *
iabout_do_init( const ofaIAbout *instance )
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

/*
 * The part below implements the software extension API
 */

/*
 * The count of GType types provided by this extension.
 * Each of these GType types must be addressed in #ofa_extension_list_types().
 * Only the GTypeModule has to be registered from #ofa_extension_startup().
 */
#define TYPES_COUNT	 4

/*
 * ofa_extension_startup:
 *
 * mandatory starting with API v. 1.
 */
gboolean
ofa_extension_startup( GTypeModule *module, ofaIGetter *getter )
{
	static const gchar *thisfn = "mysql/ofa_extension_startup";

	g_debug( "%s: module=%p, getter=%p", thisfn, ( void * ) module, ( void * ) getter  );

	ofa_mysql_main_register_type( module );

	return( TRUE );
}

/*
 * ofa_extension_list_types:
 *
 * mandatory starting with API v. 1.
 */
guint
ofa_extension_list_types( const GType **types )
{
	static const gchar *thisfn = "mysql/ofa_extension_list_types";
	static GType types_list [1+TYPES_COUNT];
	gint i = 0;

	g_debug( "%s: types=%p, count=%u", thisfn, ( void * ) types, TYPES_COUNT );

	types_list[i++] = OFA_TYPE_MYSQL_MAIN;
	types_list[i++] = OFA_TYPE_MYSQL_DBMODEL;
	types_list[i++] = OFA_TYPE_MYSQL_DBPROVIDER;
	types_list[i++] = OFA_TYPE_MYSQL_PROPERTIES;

	g_return_val_if_fail( i == TYPES_COUNT, 0 );
	types_list[i] = 0;
	*types = types_list;

	return( TYPES_COUNT );
}

/*
 * ofa_extension_shutdown:
 *
 * optional as of API v. 1.
 */
void
ofa_extension_shutdown( void )
{
	static const gchar *thisfn = "mysql/ofa_extension_shutdown";

	g_debug( "%s", thisfn );
}
