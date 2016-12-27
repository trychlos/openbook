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

#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbprovider.h"

/* some data attached to each IDBDossierEditor instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIDBProvider *provider;
}
	sEditor;

#define IDBDOSSIER_EDITOR_DATA             "idbdossier-editor-data"
#define IDBDOSSIER_EDITOR_LAST_VERSION      1

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };
static guint st_initializations         =   0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaIDBDossierEditorInterface *klass );
static void     interface_base_finalize( ofaIDBDossierEditorInterface *klass );
static sEditor *get_editor_data( const ofaIDBDossierEditor *editor );
static void     on_editor_finalized( sEditor *data, GObject *finalized_editor );

/**
 * ofa_idbdossier_editor_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbdossier_editor_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbdossier_editor_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBDossierEditorInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBDossierEditor", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBDossierEditorInterface *klass )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIDBDossierEditor::ofa-changed:
		 *
		 * This signal is sent on the #ofaIDBDossierEditor widget when any of
		 * the content changes.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIDBDossierEditor *widget,
		 * 						gpointer    user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-changed",
					OFA_TYPE_IDBDOSSIER_EDITOR,
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
interface_base_finalize( ofaIDBDossierEditorInterface *klass )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbdossier_editor_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbdossier_editor_get_interface_last_version( void )
{
	return( IDBDOSSIER_EDITOR_LAST_VERSION );
}

/**
 * ofa_idbdossier_editor_get_interface_version:
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
ofa_idbdossier_editor_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBDOSSIER_EDITOR );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBDossierEditorInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBDossierEditorInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBDossierEditor::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

#if 0
/**
 * ofa_idbdossier_editor_get_provider:
 * @instance: this #ofaIDBDossierEditor instance.
 *
 * Returns: a new reference to the #ofaIDBProvider instance, which
 * manages this #ofaIDBDossierEditor, which should be g_object_unref() by
 * the caller.
 */
ofaIDBProvider*
ofa_idbdossier_editor_get_provider( const ofaIDBDossierEditor *instance )
{
	sEditor *data;

	g_return_val_if_fail( instance && OFA_IS_IDBDOSSIER_EDITOR( instance ), NULL );

	data = get_editor_data( instance );
	return( g_object_ref( data->provider ));
}

/**
 * ofa_idbdossier_editor_set_meta:
 * @instance: this #ofaIDBDossierEditor instance.
 * @meta: [allow-none]: the #ofaIDBDossierMeta object which holds dossier
 *  informations.
 * @period: [allow-none]: the #ofaIDBExerciceMeta object which holds exercice
 *  informations; must be %NULL if @meta is %NULL.
 *
 * Initialize the widget with provided datas.
 */
void
ofa_idbdossier_editor_set_meta( ofaIDBDossierEditor *instance, const ofaIDBDossierMeta *meta, const ofaIDBExerciceMeta *period )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_set_meta";

	g_debug( "%s: instance=%p, meta=%p, period=%p",
			thisfn, ( void * ) instance, ( void * ) meta, ( void * ) period );

	g_return_if_fail( instance && OFA_IS_IDBDOSSIER_EDITOR( instance ));
	g_return_if_fail( !meta || OFA_IS_IDBDOSSIER_META( meta ));
	g_return_if_fail( !period || OFA_IS_IDBEXERCICE_META( period ));
	g_return_if_fail( meta || !period );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->set_meta ){
		OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->set_meta( instance, meta, period );
		return;
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'set_meta()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
}
#endif

/**
 * ofa_idbdossier_editor_get_size_group:
 * @instance: this #ofaIDBDossierEditor instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup of the specified @column.
 */
GtkSizeGroup *
ofa_idbdossier_editor_get_size_group( const ofaIDBDossierEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_get_size_group";

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && OFA_IS_IDBDOSSIER_EDITOR( instance ), NULL );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->get_size_group ){
		return( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->get_size_group( instance, column ));
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_idbdossier_editor_is_valid:
 * @instance: this #ofaIDBDossierEditor instance.
 * @message: [allow-none][out]: a message to be set.
 *
 * Returns: %TRUE if the entered connection informations are valid.
 */
gboolean
ofa_idbdossier_editor_is_valid( const ofaIDBDossierEditor *instance, gchar **message )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_is_valid";

	g_debug( "%s: instance=%p, message=%p", thisfn, ( void * ) instance, ( void * ) message );

	g_return_val_if_fail( instance && OFA_IS_IDBDOSSIER_EDITOR( instance ), FALSE );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->is_valid ){
		return( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->is_valid( instance, message ));
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'is_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * ofa_idbdossier_editor_apply:
 * @instance: this #ofaIDBDossierEditor instance.
 *
 * Returns: %TRUE if the informations have been successfully registered.
 */
gboolean
ofa_idbdossier_editor_apply( const ofaIDBDossierEditor *instance )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_apply";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBDOSSIER_EDITOR( instance ), FALSE );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->apply ){
		return( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( instance )->apply( instance ));
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'apply()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * ofa_idbdossier_editor_set_provider:
 * @instance: this #ofaIDBDossierEditor instance.
 * @provider: the #ofaIDBProvider instance which manages this editor.
 *
 * Attach the editor to the @provider.
 */
void
ofa_idbdossier_editor_set_provider( ofaIDBDossierEditor *instance, ofaIDBProvider *provider )
{
	sEditor *data;

	g_return_if_fail( instance && OFA_IS_IDBDOSSIER_EDITOR( instance ));
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));

	data = get_editor_data( instance );
	data->provider = provider;
}

static sEditor *
get_editor_data( const ofaIDBDossierEditor *editor )
{
	sEditor *data;

	data = ( sEditor * ) g_object_get_data( G_OBJECT( editor ), IDBDOSSIER_EDITOR_DATA );

	if( !data ){
		data = g_new0( sEditor, 1 );
		g_object_set_data( G_OBJECT( editor ), IDBDOSSIER_EDITOR_DATA, data );
		g_object_weak_ref( G_OBJECT( editor ), ( GWeakNotify ) on_editor_finalized, data );
	}

	return( data );
}

static void
on_editor_finalized( sEditor *data, GObject *finalized_editor )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_on_editor_finalized";

	g_debug( "%s: data=%p, finalized_editor=%p", thisfn, ( void * ) data, ( void * ) finalized_editor );

	g_free( data );
}
