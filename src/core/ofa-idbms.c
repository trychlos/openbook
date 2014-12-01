/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "core/ofa-dblogin.h"
#include "core/ofa-plugin.h"

static guint st_initializations = 0;		/* interface initialization count */

static GType     register_type( void );
static void      interface_base_init( ofaIDbmsInterface *klass );
static void      interface_base_finalize( ofaIDbmsInterface *klass );
static guint     idbms_get_interface_version( const ofaIDbms *instance );
static ofaIDbms *get_provider_by_name( GList *modules, const gchar *name );

static GSList   *get_providers_list( GList *modules );
static gboolean  confirm_for_deletion( const ofaIDbms *instance, const gchar *label, gboolean drop_db, gboolean drop_accounts );

/**
 * ofa_idbms_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbms_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbms_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbms_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDbmsInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDbms", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDbmsInterface *klass )
{
	static const gchar *thisfn = "ofa_idbms_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->get_interface_version = idbms_get_interface_version;
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDbmsInterface *klass )
{
	static const gchar *thisfn = "ofa_idbms_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

static guint
idbms_get_interface_version( const ofaIDbms *instance )
{
	return( 1 );
}

/**
 * ofa_idbms_connect:
 * @instance: this
 */
void *
ofa_idbms_connect( const ofaIDbms *instance,
						const gchar *dname, const gchar *dbname,
						const gchar *account, const gchar *password )
{
	static const gchar *thisfn = "ofa_idbms_connect";
	void *handle;

	g_debug( "%s: instance=%p, dname=%s, dbname=%s, account=%s, password=%s",
			thisfn, ( void * ) instance, dname, dbname, account, password );

	handle = NULL;

	if( OFA_IDBMS_GET_INTERFACE( instance )->connect ){
		handle = OFA_IDBMS_GET_INTERFACE( instance )->connect( instance, dname, dbname, account, password );
	}

	return( handle );
}

/**
 * ofa_idbms_close:
 */
void
ofa_idbms_close( const ofaIDbms *instance, void *handle )
{
	if( OFA_IDBMS_GET_INTERFACE( instance )->close ){
		OFA_IDBMS_GET_INTERFACE( instance )->close( instance, handle );
	}
}

/**
 * ofa_idbms_get_provider_by_name:
 * @pname: the name of the provider as published in the settings.
 *
 * Returns a new reference to the #ofaIDbms module instance which
 * publishes this name.
 * This new reference should be g_object_unref() by the caller after
 * usage.
 */
ofaIDbms *
ofa_idbms_get_provider_by_name( const gchar *pname )
{
	static const gchar *thisfn = "ofa_idbms_get_provider_by_name";
	GList *modules;
	ofaIDbms *module;

	g_debug( "%s: name=%s", thisfn, pname );

	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBMS );
	module = get_provider_by_name( modules, pname );
	ofa_plugin_free_extensions( modules );

	return( module );
}

static ofaIDbms *
get_provider_by_name( GList *modules, const gchar *name )
{
	GList *im;
	ofaIDbms *instance;
	const gchar *provider_name;

	instance = NULL;

	for( im=modules ; im ; im=im->next ){

		provider_name = ofa_idbms_get_provider_name( OFA_IDBMS( im->data ));
		if( !g_utf8_collate( provider_name, name )){
			instance = g_object_ref( OFA_IDBMS( im->data ));
			break;
		}
	}

	return( instance );
}

/**
 * ofa_idbms_get_provider_name:
 */
const gchar *
ofa_idbms_get_provider_name( const ofaIDbms *instance )
{
	const gchar *name;

	name = NULL;

	if( OFA_IDBMS_GET_INTERFACE( instance )->get_provider_name ){
		name = OFA_IDBMS_GET_INTERFACE( instance )->get_provider_name( instance );
	}

	return( name );
}

/**
 * ofa_idbms_get_exercices:
 * @instance: this #ofaIDbms instance.
 * @dname: the dossier name read from settings.
 *
 * Returns: the list of known exercices for the dossier as a sem-colon
 * separated list of strings:
 * - a displayable label
 * - the corresponding database name.
 *
 * The returned list should be ofa_idbms_free_exercices() by the caller.
 */
GSList *
ofa_idbms_get_exercices( const ofaIDbms *instance, const gchar *dname )
{
	g_return_val_if_fail( OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( dname && g_utf8_strlen( dname, -1 ), NULL );

	if( OFA_IDBMS_GET_INTERFACE( instance )->get_exercices ){
		return( OFA_IDBMS_GET_INTERFACE( instance )->get_exercices( instance, dname ));
	}

	return( NULL );
}

/**
 * ofa_idbms_query:
 */
gboolean
ofa_idbms_query( const ofaIDbms *instance, void *handle, const gchar *query )
{
	g_return_val_if_fail( OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( handle, FALSE );
	g_return_val_if_fail( query && g_utf8_strlen( query, -1 ), FALSE );

	if( OFA_IDBMS_GET_INTERFACE( instance )->query ){
		return( OFA_IDBMS_GET_INTERFACE( instance )->query( instance, handle, query ));
	}

	return( FALSE );
}

/**
 * ofa_idbms_query_ex:
 */
gboolean
ofa_idbms_query_ex( const ofaIDbms *instance, void *handle, const gchar *query, GSList **result )
{
	g_return_val_if_fail( OFA_IS_IDBMS( instance ), FALSE );
	g_return_val_if_fail( handle, FALSE );
	g_return_val_if_fail( query && g_utf8_strlen( query, -1 ), FALSE );
	g_return_val_if_fail( result, FALSE );

	if( OFA_IDBMS_GET_INTERFACE( instance )->query_ex ){
		return( OFA_IDBMS_GET_INTERFACE( instance )->query_ex( instance, handle, query, result ));
	}

	return( FALSE );
}

/**
 * ofa_idbms_last_error:
 */
gchar *
ofa_idbms_last_error( const ofaIDbms *instance, void *handle )
{
	g_return_val_if_fail( OFA_IS_IDBMS( instance ), NULL );
	g_return_val_if_fail( handle, NULL );

	if( OFA_IDBMS_GET_INTERFACE( instance )->last_error ){
		return( OFA_IDBMS_GET_INTERFACE( instance )->last_error( instance, handle ));
	}

	return( NULL );
}

/**
 * ofa_idbms_get_providers_list:
 *
 * Returns the list of ISgbd provider names, as a newly allocated list
 * of newly allocated strings. The returned list should be free via
 * ofa_idbms_free_providers_list() method.
 */
GSList *
ofa_idbms_get_providers_list( void )
{
	static const gchar *thisfn = "ofa_idbms_get_providers_list";
	GList *modules;
	GSList *names;

	g_debug( "%s", thisfn );

	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBMS );

	names = get_providers_list( modules );

	ofa_plugin_free_extensions( modules );

	return( names );
}

static GSList *
get_providers_list( GList *modules )
{
	GList *im;
	GSList *names;
	const gchar *provider_name;

	names = NULL;

	for( im=modules ; im ; im=im->next ){

		provider_name = ofa_idbms_get_provider_name( OFA_IDBMS( im->data ));
		names = g_slist_prepend( names, g_strdup( provider_name ));
	}

	return( names );
}

/**
 * ofa_idbms_free_providers_list:
 *
 * Free the list returned by ofa_idbms_get_providers_list() method.
 */
void
ofa_idbms_free_providers_list( GSList *list )
{
	static const gchar *thisfn = "ofa_idbms_free_providers_list";

	g_debug( "%s: list=%p (count=%u)", thisfn, ( void * ) list, g_slist_length( list ));

	g_slist_free_full( list, g_free );
}

/**
 * ofa_idbms_properties_new_init:
 *
 * Initialize the GtkDialog part which let the user enter properties
 * for a new connection definition
 */
void
ofa_idbms_properties_new_init( const ofaIDbms *instance, GtkContainer *parent,
									GtkSizeGroup *group )
{
	if( OFA_IDBMS_GET_INTERFACE( instance )->properties_new_init ){
		OFA_IDBMS_GET_INTERFACE( instance )->properties_new_init( instance, parent, group );
	}
}

/**
 * ofa_idbms_properties_new_check:
 *
 * Check that the definition is enough to be validated.
 */
gboolean
ofa_idbms_properties_new_check( const ofaIDbms *instance, GtkContainer *parent )
{
	gboolean ok;

	ok = FALSE;

	if( OFA_IDBMS_GET_INTERFACE( instance )->properties_new_check ){
		ok = OFA_IDBMS_GET_INTERFACE( instance )->properties_new_check( instance, parent );
	}

	return( ok );
}

/**
 * ofa_idbms_properties_new_apply:
 *
 * Try to apply the new definition
 */
gboolean
ofa_idbms_properties_new_apply( const ofaIDbms *instance, GtkContainer *parent,
									const gchar *label, const gchar *account, const gchar *password )
{
	gboolean ok;

	ok = FALSE;

	if( OFA_IDBMS_GET_INTERFACE( instance )->properties_new_apply ){
		ok = OFA_IDBMS_GET_INTERFACE( instance )->properties_new_apply( instance, parent, label, account, password );
	}

	return( ok );
}

/**
 * ofa_idbms_get_dossier_host:
 */
gchar *
ofa_idbms_get_dossier_host( const ofaIDbms *instance, const gchar *label )
{
	gchar *host;

	host = NULL;

	if( OFA_IDBMS_GET_INTERFACE( instance )->get_dossier_host ){
		host = OFA_IDBMS_GET_INTERFACE( instance )->get_dossier_host( instance, label );
	}

	return( host );
}

/**
 * ofa_idbms_get_dossier_dbname:
 */
gchar *
ofa_idbms_get_dossier_dbname( const ofaIDbms *instance, const gchar *label )
{
	gchar *dbname;

	dbname = NULL;

	if( OFA_IDBMS_GET_INTERFACE( instance )->get_dossier_dbname ){
		dbname = OFA_IDBMS_GET_INTERFACE( instance )->get_dossier_dbname( instance, label );
	}

	return( dbname );
}

/**
 * ofa_idbms_delete_dossier:
 *
 * Returns %TRUE while at least the DB server connection is successful
 */
gboolean
ofa_idbms_delete_dossier( const ofaIDbms *instance,
							const gchar *label, const gchar *account, const gchar *password,
							gboolean drop_db, gboolean drop_accounts, gboolean with_confirm )
{
	gboolean confirmed;
	gboolean ok;

	ok = FALSE;
	confirmed = TRUE;

	if( with_confirm ){
		confirmed = confirm_for_deletion( instance, label, drop_db, drop_accounts );
	}

	if( confirmed && OFA_IDBMS_GET_INTERFACE( instance )->delete_dossier ){
		ok = OFA_IDBMS_GET_INTERFACE( instance )->delete_dossier( instance,
																	label, account, password,
																	drop_db, drop_accounts );

		ofa_settings_remove_dossier( label );
	}

	return( ok );
}

static gboolean
confirm_for_deletion( const ofaIDbms *instance, const gchar *label, gboolean drop_db, gboolean drop_accounts )
{
	GtkWidget *dialog;
	gchar *msg;
	gint response;

	msg = g_strdup_printf( _( "You are about to delete the '%s' dossier.\n"
			"This operation will cannot be recoverable.\n"
			"Are you sure ?" ), label );

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", msg );

	g_free( msg );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "_OK" ), GTK_RESPONSE_CANCEL,
			_( "_Cancel" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

/**
 * ofa_idbms_get_def_backup_cmd:
 */
const gchar *
ofa_idbms_get_def_backup_cmd( const ofaIDbms *instance )
{
	if( OFA_IDBMS_GET_INTERFACE( instance )->get_def_backup_cmd ){
		return( OFA_IDBMS_GET_INTERFACE( instance )->get_def_backup_cmd( instance ));
	}

	return( NULL );
}

/**
 * ofa_idbms_backup:
 */
gboolean
ofa_idbms_backup( const ofaIDbms *instance, void *handle, const gchar *fname )
{
	gboolean ok;

	ok = FALSE;

	if( OFA_IDBMS_GET_INTERFACE( instance )->backup ){
		ok = OFA_IDBMS_GET_INTERFACE( instance )->backup( instance, handle, fname );
	}

	return( ok );
}

/**
 * ofa_idbms_get_def_restore_cmd:
 */
const gchar *
ofa_idbms_get_def_restore_cmd( const ofaIDbms *instance )
{
	if( OFA_IDBMS_GET_INTERFACE( instance )->get_def_restore_cmd ){
		return( OFA_IDBMS_GET_INTERFACE( instance )->get_def_restore_cmd( instance ));
	}

	return( NULL );
}

/**
 * ofa_idbms_restore:
 *
 * Takes care of asking the DBMS administrator account and password
 * before calling the DBMS provider.
 */
gboolean
ofa_idbms_restore( const ofaIDbms *instance, const gchar *label, const gchar *fname )
{
	gboolean ok;
	gchar *account, *password;

	ok = FALSE;

	if( OFA_IDBMS_GET_INTERFACE( instance )->restore &&
		ofa_dblogin_run( label, &account, &password )){

		ok = OFA_IDBMS_GET_INTERFACE( instance )->restore( instance, label, fname, account, password );
	}

	return( ok );
}

/**
 * ofa_idbms_display_connect_infos:
 * @container: the widget into which display the connection informations
 * @label: the label of the dossier.
 *
 * Ask the DBMS provider associated to the named dossier to display
 * its connect informations
 */
void
ofa_idbms_display_connect_infos( GtkWidget *container, const gchar *label )
{
	gchar *provider;
	GList *modules;
	ofaIDbms *instance;

	instance = NULL;
	provider = ofa_settings_get_dossier_provider( label );

	if( provider && g_utf8_strlen( provider, -1 )){
		modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IDBMS );
		instance = get_provider_by_name( modules, provider );
		ofa_plugin_free_extensions( modules );
	}

	if( instance && OFA_IS_IDBMS( instance ) &&
		OFA_IDBMS_GET_INTERFACE( instance )->display_connect_infos ){

		OFA_IDBMS_GET_INTERFACE( instance )->display_connect_infos( instance, container, label );
	}
}
