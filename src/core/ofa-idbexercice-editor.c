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
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbprovider.h"

/* some data attached to each IDBExerciceEditor instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIDBProvider      *provider;
	ofaIDBDossierEditor *dossier_editor;
}
	sEditor;

#define IDBEXERCICE_EDITOR_LAST_VERSION     1
#define IDBEXERCICE_EDITOR_DATA            "idbexercice-editor-data"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };
static guint st_initializations         =   0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaIDBExerciceEditorInterface *klass );
static void     interface_base_finalize( ofaIDBExerciceEditorInterface *klass );
static sEditor *get_instance_data( const ofaIDBExerciceEditor *self );
static void     on_instance_finalized( sEditor *sdata, GObject *finalized_instance );

/**
 * ofa_idbexercice_editor_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idbexercice_editor_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idbexercice_editor_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDBExerciceEditorInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDBExerciceEditor", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIDBExerciceEditorInterface *klass )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIDBExerciceEditor::ofa-changed:
		 *
		 * This signal is sent on the #ofaIDBExerciceEditor widget when any of
		 * the content changes.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIDBExerciceEditor *widget,
		 * 						gpointer    user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-changed",
					OFA_TYPE_IDBEXERCICE_EDITOR,
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
interface_base_finalize( ofaIDBExerciceEditorInterface *klass )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idbexercice_editor_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idbexercice_editor_get_interface_last_version( void )
{
	return( IDBEXERCICE_EDITOR_LAST_VERSION );
}

/**
 * ofa_idbexercice_editor_get_interface_version:
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
ofa_idbexercice_editor_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDBEXERCICE_EDITOR );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDBExerciceEditorInterface * ) iface )->get_interface_version ){
		version = (( ofaIDBExerciceEditorInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDBExerciceEditor::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

#if 0
/**
 * ofa_idbexercice_editor_get_provider:
 * @instance: this #ofaIDBExerciceEditor instance.
 *
 * Returns: a new reference to the #ofaIDBProvider instance, which
 * manages this #ofaIDBExerciceEditor, which should be g_object_unref() by
 * the caller.
 */
ofaIDBProvider*
ofa_idbexercice_editor_get_provider( const ofaIDBExerciceEditor *instance )
{
	sEditor *data;

	g_return_val_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ), NULL );

	data = get_instance_data( instance );
	return( g_object_ref( data->provider ));
}
#endif

/**
 * ofa_idbexercice_editor_set_provider:
 * @instance: this #ofaIDBExerciceEditor instance.
 * @provider: the #ofaIDBProvider instance which manages this editor.
 *
 * Attach the @provider to the @instance.
 */
void
ofa_idbexercice_editor_set_provider( ofaIDBExerciceEditor *instance, ofaIDBProvider *provider )
{
	sEditor *sdata;

	g_return_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ));
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));

	sdata = get_instance_data( instance );

	sdata->provider = provider;
}

/**
 * ofa_idbexercice_editor_get_dossier_editor:
 * @instance: this #ofaIDBExerciceEditor instance.
 *
 * Returns: the attached #ofaIDBDossierEditor.
 */
ofaIDBDossierEditor *
ofa_idbexercice_editor_get_dossier_editor( ofaIDBExerciceEditor *instance )
{
	sEditor *sdata;

	g_return_val_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ), NULL );

	sdata = get_instance_data( instance );

	return( sdata->dossier_editor );
}

/**
 * ofa_idbexercice_editor_set_dossier_editor:
 * @instance: this #ofaIDBExerciceEditor instance.
 * @editor: the corresponding #ofaIDBDossierEditor.
 *
 * Attach the @editor to the @instance.
 */
void
ofa_idbexercice_editor_set_dossier_editor( ofaIDBExerciceEditor *instance, ofaIDBDossierEditor *editor )
{
	sEditor *sdata;

	g_return_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ));
	g_return_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ));

	sdata = get_instance_data( instance );

	sdata->dossier_editor = editor;
}

/**
 * ofa_idbexercice_editor_get_size_group:
 * @instance: this #ofaIDBExerciceEditor instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup of the specified @column.
 */
GtkSizeGroup *
ofa_idbexercice_editor_get_size_group( const ofaIDBExerciceEditor *instance, guint column )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_get_size_group";

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ), NULL );

	if( OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance )->get_size_group ){
		return( OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance )->get_size_group( instance, column ));
	}

	g_info( "%s: ofaIDBExerciceEditor's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( NULL );
}

/**
 * ofa_idbexercice_editor_is_valid:
 * @instance: this #ofaIDBExerciceEditor instance.
 * @message: [allow-none][out]: a message to be set.
 *
 * Returns: %TRUE if the entered connection informations are valid.
 */
gboolean
ofa_idbexercice_editor_is_valid( const ofaIDBExerciceEditor *instance, gchar **message )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_is_valid";

	g_return_val_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ), FALSE );

	if( OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance )->is_valid ){
		return( OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance )->is_valid( instance, message ));
	}

	g_info( "%s: ofaIDBExerciceEditor's %s implementation does not provide 'is_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

/**
 * ofa_idbexercice_editor_apply:
 * @instance: this #ofaIDBExerciceEditor instance.
 *
 * Returns: %TRUE if the informations have been successfully registered.
 */
gboolean
ofa_idbexercice_editor_apply( const ofaIDBExerciceEditor *instance )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_apply";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IDBEXERCICE_EDITOR( instance ), FALSE );

	if( OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance )->apply ){
		return( OFA_IDBEXERCICE_EDITOR_GET_INTERFACE( instance )->apply( instance ));
	}

	g_info( "%s: ofaIDBExerciceEditor's %s implementation does not provide 'apply()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( FALSE );
}

static sEditor *
get_instance_data( const ofaIDBExerciceEditor *self )
{
	sEditor *data;

	data = ( sEditor * ) g_object_get_data( G_OBJECT( self ), IDBEXERCICE_EDITOR_DATA );

	if( !data ){
		data = g_new0( sEditor, 1 );
		g_object_set_data( G_OBJECT( self ), IDBEXERCICE_EDITOR_DATA, data );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, data );
	}

	return( data );
}

static void
on_instance_finalized( sEditor *data, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_idbexercice_editor_on_instance_finalized";

	g_debug( "%s: data=%p, finalized_instance=%p", thisfn, ( void * ) data, ( void * ) finalized_instance );

	g_free( data );
}
