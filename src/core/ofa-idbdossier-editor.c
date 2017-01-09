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

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbdossier-editor.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-editor.h"
#include "api/ofa-idbprovider.h"

/* some data attached to each IDBDossierEditor instance
 * we store here the data provided by the application
 * which do not depend of a specific implementation
 */
typedef struct {
	ofaIDBProvider *provider;
}
	sEditor;

#define IDBDOSSIER_EDITOR_LAST_VERSION      1
#define IDBDOSSIER_EDITOR_DATA             "idbdossier-editor-data"

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
static sEditor *get_instance_data( const ofaIDBDossierEditor *self );
static void     on_instance_finalized( sEditor *sdata, GObject *finalized_editor );

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

/**
 * ofa_idbdossier_editor_get_provider:
 * @editor: this #ofaIDBDossierEditor instance.
 *
 * Returns: the #ofaIDBProvider instance which was attached at
 * instanciation time.
 *
 * The returned reference is owned by the @editor, and should not be
 * released by the caller.
 */
ofaIDBProvider *
ofa_idbdossier_editor_get_provider( ofaIDBDossierEditor *editor )
{
	sEditor *sdata;

	g_return_val_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ), NULL );

	sdata = get_instance_data( editor );

	return( sdata->provider );
}

/**
 * ofa_idbdossier_editor_set_provider:
 * @editor: this #ofaIDBDossierEditor instance.
 * @provider: the #ofaIDBProvider instance which manages this @editor.
 *
 * Attach the @provider to the @editor.
 */
void
ofa_idbdossier_editor_set_provider( ofaIDBDossierEditor *editor, ofaIDBProvider *provider )
{
	sEditor *sdata;

	g_return_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ));
	g_return_if_fail( provider && OFA_IS_IDBPROVIDER( provider ));

	sdata = get_instance_data( editor );

	sdata->provider = provider;
}

/**
 * ofa_idbdossier_editor_get_size_group:
 * @editor: this #ofaIDBDossierEditor instance.
 * @column: the desired column.
 *
 * Returns: the #GtkSizeGroup of the specified @column.
 */
GtkSizeGroup *
ofa_idbdossier_editor_get_size_group( const ofaIDBDossierEditor *editor, guint column )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_get_size_group";

	g_debug( "%s: editor=%p, column=%u", thisfn, ( void * ) editor, column );

	g_return_val_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ), NULL );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->get_size_group ){
		return( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->get_size_group( editor, column ));
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'get_size_group()' method",
			thisfn, G_OBJECT_TYPE_NAME( editor ));
	return( NULL );
}

/**
 * ofa_idbdossier_editor_is_valid:
 * @editor: this #ofaIDBDossierEditor instance.
 * @message: [allow-none][out]: a message to be set.
 *
 * Returns: %TRUE if the entered connection informations are valid.
 */
gboolean
ofa_idbdossier_editor_is_valid( const ofaIDBDossierEditor *editor, gchar **message )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_is_valid";

	g_return_val_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ), FALSE );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->is_valid ){
		return( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->is_valid( editor, message ));
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'is_valid()' method",
			thisfn, G_OBJECT_TYPE_NAME( editor ));
	return( FALSE );
}

/**
 * ofa_idbdossier_editor_get_valid_connect:
 * @editor: this #ofaIDBDossierEditor instance.
 * @dossier_meta: the #ofaIDBDossierMeta to associate with the @instance.
 *
 * Returns: the #ofaIDBConnect which was used to validate the @editor.
 *
 * This #ofaIDBConnect is expected to handles a superuser connection to
 * the DBMS at server level, and thus dossier_meta, account and password
 * are set.
 *
 * It is not expected that it knows anything about an #ofaIDBExerciceMeta.
 *
 * If not %NULL, the returned reference is owned by @editor, and should
 * not be released by the caller.
 */
ofaIDBConnect *
ofa_idbdossier_editor_get_valid_connect( const ofaIDBDossierEditor *editor, ofaIDBDossierMeta *dossier_meta )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_get_valid_connect";

	g_debug( "%s: editor=%p", thisfn, ( void * ) editor );

	g_return_val_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ), NULL );
	g_return_val_if_fail( dossier_meta && OFA_IS_IDBDOSSIER_META( dossier_meta ), NULL );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->get_valid_connect ){
		return( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->get_valid_connect( editor, dossier_meta ));
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'get_valid_connect()' method",
			thisfn, G_OBJECT_TYPE_NAME( editor ));
	return( NULL );
}

/**
 * ofa_idbdossier_editor_new_exercice_editor:
 * @editor: this #ofaIDBDossierEditor provider.
 * @settings_prefix: the prefix of a user preference key.
 * @rule: the usage of the editor.
 *
 * Returns: a composite GTK container widget intended to hold the
 * informations needed to fully identify the DBMS server which manages
 * a exercice.
 *
 * The returned container will be added to a GtkWindow and must be
 * destroyable with this same window. In other words, the DBMS provider
 * should not keep any reference on this container.
 */
ofaIDBExerciceEditor *
ofa_idbdossier_editor_new_exercice_editor( ofaIDBDossierEditor *editor, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_new_exercice_editor";
	ofaIDBExerciceEditor *exercice_editor;

	g_debug( "%s: editor=%p, settings_prefix=%s, rule=%u",
			thisfn,( void * ) editor, settings_prefix, rule );

	g_return_val_if_fail( editor && OFA_IS_IDBDOSSIER_EDITOR( editor ), NULL );

	if( OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->new_exercice_editor ){
		exercice_editor = OFA_IDBDOSSIER_EDITOR_GET_INTERFACE( editor )->new_exercice_editor( editor, settings_prefix, rule );
		ofa_idbexercice_editor_set_dossier_editor( exercice_editor, editor );
		return( exercice_editor );
	}

	g_info( "%s: ofaIDBDossierEditor's %s implementation does not provide 'new_exercice_editor()' method",
			thisfn, G_OBJECT_TYPE_NAME( editor ));
	return( NULL );
}

static sEditor *
get_instance_data( const ofaIDBDossierEditor *self )
{
	sEditor *sdata;

	sdata = ( sEditor * ) g_object_get_data( G_OBJECT( self ), IDBDOSSIER_EDITOR_DATA );

	if( !sdata ){
		sdata = g_new0( sEditor, 1 );
		g_object_set_data( G_OBJECT( self ), IDBDOSSIER_EDITOR_DATA, sdata );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sEditor *sdata, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_idbdossier_editor_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_instance=%p", thisfn, ( void * ) sdata, ( void * ) finalized_instance );

	g_free( sdata );
}
