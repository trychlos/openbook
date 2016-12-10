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

#include "my/my-utils.h"

#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbprovider.h"

/* some data attached to each IDBEditor instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIDBProvider *prov_instance;
}
	sIDBEditor;

#define IDBEDITOR_LAST_VERSION              1
#define IDBEDITOR_DATA                     "idbeditor-data"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };
static guint st_initializations         =   0;	/* interface initialization count */

static GType       register_type( void );
static void        interface_base_init( ofaIDBEditorInterface *klass );
static void        interface_base_finalize( ofaIDBEditorInterface *klass );
static sIDBEditor *get_idbeditor_data( const ofaIDBEditor *editor );
static void        on_editor_finalized( sIDBEditor *data, GObject *finalized_editor );

/**
 * ofa_idbeditor_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbeditor_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbeditor_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbeditor_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBEditorInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBEditor", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBEditorInterface *klass )
{
	static const gchar *thisfn = "ofa_idbeditor_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIDBEditor::ofa-changed:
		 *
		 * This signal is sent on the #ofaIDBEditor widget when any of
		 * the content changes.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIDBEditor *widget,
		 * 						gpointer    user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-changed",
					OFA_TYPE_IDBEDITOR,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDBEditorInterface *klass )
{
	static const gchar *thisfn = "ofa_idbeditor_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbeditor_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbeditor_get_interface_last_version( void )
{
	return( IDBEDITOR_LAST_VERSION );
}

/**
 * ofa_idbeditor_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idbeditor_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBEDITOR );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBEditorInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBEditorInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBEditor::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idbeditor_get_provider:
 * @instance: this #ofaIDBEditor instance.
 *
 * Returns: a new reference to the #ofaIDBProvider instance, which
 * manages this #ofaIDBEditor, which should be g_object_unref() by
 * the caller.
 */
ofaIDBProvider*
ofa_idbeditor_get_provider( const ofaIDBEditor *instance )
{
	sIDBEditor *data;

	g_return_val_if_fail( instance && OFA_IS_IDBEDITOR( instance ), NULL );

	data = get_idbeditor_data( instance );
	return( g_object_ref( data->prov_instance ));
}

/**
 * ofa_idbeditor_set_provider:
 * @instance: this #ofaIDBEditor instance.
 * @provider: the #ofaIDBProvider instance which manages this editor.
 *
 * Attach the editor to the @provider.
 */
void
ofa_idbeditor_set_provider( ofaIDBEditor *instance, const ofaIDBProvider *provider )
{
	sIDBEditor *data;

	g_return_if_fail( instance && OFA_IS_IDBEDITOR( instance ));
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));

	data = get_idbeditor_data( instance );
	g_clear_object( &data->prov_instance );
	data->prov_instance = g_object_ref(( gpointer ) provider );
}

/**
 * ofa_idbeditor_set_meta:
 * @instance: this #ofaIDBEditor instance.
 * @dossier_meta: [allow-none]: the #ofaIDBDossierMeta object which
 *  holds the dossier meta informations.
 * @period: [allow-none]: the #ofaIDBPeriod object which holds exercice
 *  informations; must be %NULL if @meta is %NULL.
 *
 * Initialize the widget with provided datas.
 */
void
ofa_idbeditor_set_meta( ofaIDBEditor *instance, const ofaIDBDossierMeta *dossier_meta, const ofaIDBPeriod *period )
{
	static const gchar *thisfn = "ofa_idbeditor_set_meta";

	g_debug( "%s: instance=%p, dossier_meta=%p, period=%p",
			thisfn, ( void * ) instance, ( void * ) dossier_meta, ( void * ) period );

	g_return_if_fail( instance && OFA_IS_IDBEDITOR( instance ));
	g_return_if_fail( !dossier_meta || OFA_IS_IDBDOSSIER_META( dossier_meta ));
	g_return_if_fail( !period || OFA_IS_IDBPERIOD( period ));
	g_return_if_fail( dossier_meta || !period );

	if( OFA_IDBEDITOR_GET_INTERFACE( instance )->set_meta ){
		OFA_IDBEDITOR_GET_INTERFACE( instance )->set_meta( instance, dossier_meta, period );
		return;
	}

	g_info( "%s: ofaIDBEditor's %s implementation does not provide 'set_meta()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}

/**
 * ofa_idbeditor_get_size_group:
 * @instance: this #ofaIDBEditor instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup of the specified @column.
 */
GtkSizeGroup *
ofa_idbeditor_get_size_group( const ofaIDBEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_idbeditor_get_size_group";

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && OFA_IS_IDBEDITOR( instance ), NULL );

	if( OFA_IDBEDITOR_GET_INTERFACE( instance )->get_size_group ){
		return( OFA_IDBEDITOR_GET_INTERFACE( instance )->get_size_group( instance, column ));
	}

	g_info( "%s: ofaIDBEditor's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_idbeditor_get_valid:
 * @instance: this #ofaIDBEditor instance.
 * @message: [allow-none][out]: a message to be set.
 *
 * Returns: %TRUE if the entered connection informations are valid.
 */
gboolean
ofa_idbeditor_get_valid( const ofaIDBEditor *instance, gchar **message )
{
	static const gchar *thisfn = "ofa_idbeditor_get_valid";

	g_debug( "%s: instance=%p, message=%p", thisfn, ( void * ) instance, ( void * ) message );

	g_return_val_if_fail( instance && OFA_IS_IDBEDITOR( instance ), FALSE );

	if( OFA_IDBEDITOR_GET_INTERFACE( instance )->get_valid ){
		return( OFA_IDBEDITOR_GET_INTERFACE( instance )->get_valid( instance, message ));
	}

	g_info( "%s: ofaIDBEditor's %s implementation does not provide 'get_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

static sIDBEditor *
get_idbeditor_data( const ofaIDBEditor *editor )
{
	sIDBEditor *data;

	data = ( sIDBEditor * ) g_object_get_data( G_OBJECT( editor ), IDBEDITOR_DATA );

	if( !data ){
		data = g_new0( sIDBEditor, 1 );
		g_object_set_data( G_OBJECT( editor ), IDBEDITOR_DATA, data );
		g_object_weak_ref( G_OBJECT( editor ), ( GWeakNotify ) on_editor_finalized, data );
	}

	return( data );
}

static void
on_editor_finalized( sIDBEditor *data, GObject *finalized_editor )
{
	static const gchar *thisfn = "ofa_idbeditor_on_editor_finalized";

	g_debug( "%s: data=%p, finalized_editor=%p", thisfn, ( void * ) data, ( void * ) finalized_editor );

	g_clear_object( &data->prov_instance );
	g_free( data );
}
