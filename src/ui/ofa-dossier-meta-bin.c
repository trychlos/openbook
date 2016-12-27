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

#include "api/ofa-dossier-collection.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbprovider.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-application.h"
#include "ui/ofa-dossier-edit-bin.h"
#include "ui/ofa-dossier-meta-bin.h"
#include "ui/ofa-dossier-store.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaHub         *hub;
	gchar          *settings_prefix;
	guint           rule;

	/* UI
	 */
	GtkSizeGroup   *group0;
	GtkWidget      *name_entry;
	GtkWidget      *dbms_combo;

	/* runtime data
	 */
	gboolean        initializing;
	gchar          *dossier_name;
	gchar          *provider_name;
	ofaIDBProvider *provider;
}
	ofaDossierMetaBinPrivate;

/* columns in DBMS provider combo box
 */
enum {
	DBMS_COL_NAME = 0,
	DBMS_COL_CANON,
	DBMS_COL_PROVIDER,
	DBMS_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-dossier-meta-bin.ui";

static void setup_bin( ofaDossierMetaBin *self );
static void setup_dbms_provider( ofaDossierMetaBin *self );
static void on_dossier_name_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaDossierMetaBin *self );
static void on_dossier_name_changed( GtkEditable *editable, ofaDossierMetaBin *self );
static void on_dbms_provider_changed( GtkComboBox *combo, ofaDossierMetaBin *self );
static void changed_composite( ofaDossierMetaBin *self );
static void read_settings( ofaDossierMetaBin *self );
static void write_settings( ofaDossierMetaBin *self );

G_DEFINE_TYPE_EXTENDED( ofaDossierMetaBin, ofa_dossier_meta_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDossierMetaBin ))

static void
dossier_meta_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_meta_bin_finalize";
	ofaDossierMetaBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_META_BIN( instance ));

	/* free data members here */
	priv = ofa_dossier_meta_bin_get_instance_private( OFA_DOSSIER_META_BIN( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->dossier_name );
	g_free( priv->provider_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_meta_bin_parent_class )->finalize( instance );
}

static void
dossier_meta_bin_dispose( GObject *instance )
{
	ofaDossierMetaBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_META_BIN( instance ));

	priv = ofa_dossier_meta_bin_get_instance_private( OFA_DOSSIER_META_BIN( instance ));

	if( !priv->dispose_has_run ){

		write_settings( OFA_DOSSIER_META_BIN( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_meta_bin_parent_class )->dispose( instance );
}

static void
ofa_dossier_meta_bin_init( ofaDossierMetaBin *self )
{
	static const gchar *thisfn = "ofa_dossier_meta_bin_instance_init";
	ofaDossierMetaBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_META_BIN( self ));

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
	priv->initializing = FALSE;
	priv->dossier_name = NULL;
	priv->provider_name = NULL;
}

static void
ofa_dossier_meta_bin_class_init( ofaDossierMetaBinClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_meta_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_meta_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_meta_bin_finalize;

	/**
	 * ofaDossierMetaBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaDossierMetaBin when any of the
	 * underlying information is changed. This includes the dossier
	 * name and the DBMS provider.
	 *
	 * There is no argument.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierMetaBin *bin,
	 * 						gpointer         user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DOSSIER_META_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_dossier_meta_bin_new:
 * @hub: the #ofaHub object of the application.
 * @settings_prefix: the prefix of the key in user settings.
 * @rule: the usage of this widget.
 *
 * Returns: a newly defined composite widget which aggregates dossier
 * meta datas: name and DBMS provider.
 */
ofaDossierMetaBin *
ofa_dossier_meta_bin_new( ofaHub *hub, const gchar *settings_prefix, guint rule )
{
	static const gchar *thisfn = "ofa_dossier_meta_bin_new";
	ofaDossierMetaBin *self;
	ofaDossierMetaBinPrivate *priv;

	g_debug( "%s: hub=%p, settings_prefix=%s, guint=%u",
			thisfn, ( void * ) hub, settings_prefix, rule );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	self = g_object_new( OFA_TYPE_DOSSIER_META_BIN, NULL );

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	priv->hub = hub;
	priv->rule = rule;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( self );
	read_settings( self );

	return( self );
}

/*
 * At initialization time, only setup the providers combo box
 */
static void
setup_bin( ofaDossierMetaBin *self )
{
	ofaDossierMetaBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "dmb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "dmb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* dossier name */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dmb-name-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "insert-text", G_CALLBACK( on_dossier_name_insert_text ), self );
	g_signal_connect( entry, "changed", G_CALLBACK( on_dossier_name_changed ), self );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dmb-name-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );
	priv->name_entry = entry;

	setup_dbms_provider( self );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_dbms_provider( ofaDossierMetaBin *self )
{
	ofaDossierMetaBinPrivate *priv;
	ofaExtenderCollection *extenders;
	GList *modules, *it;
	gchar *it_name, *canon_name;
	GtkWidget *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	priv->initializing = TRUE;

	combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dmb-provider-combo" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	priv->dbms_combo = combo;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			DBMS_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_OBJECT ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", DBMS_COL_NAME );

	extenders = ofa_hub_get_extender_collection( priv->hub );
	modules = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IDBPROVIDER );

	for( it=modules ; it ; it=it->next ){
		it_name = ofa_idbprovider_get_display_name( OFA_IDBPROVIDER( it->data ));
		canon_name = ofa_idbprovider_get_canon_name( OFA_IDBPROVIDER( it->data ));
		if( my_strlen( it_name ) && my_strlen( canon_name )){
			gtk_list_store_insert_with_values(
					GTK_LIST_STORE( tmodel ),
					&iter,
					-1,
					DBMS_COL_NAME,     it_name,
					DBMS_COL_CANON,    canon_name,
					DBMS_COL_PROVIDER, it->data,
					-1 );
		}
		g_free( it_name );
		g_free( canon_name );
	}

	ofa_extender_collection_free_types( modules );

	gtk_combo_box_set_id_column( GTK_COMBO_BOX( combo ), DBMS_COL_CANON );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_dbms_provider_changed ), self );

	/* setup the mnemonic widget on the label */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "dmb-provider-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	priv->initializing = FALSE;
}

/**
 * ofa_dossier_meta_bin_get_size_group:
 * @bin: this #ofaDossierMetaBin instance.
 * @column: the desire column.
 *
 * Returns: the #GtkSizeGroup which handles the desired @column.
 */
GtkSizeGroup *
ofa_dossier_meta_bin_get_size_group( ofaDossierMetaBin *bin, guint column )
{
	static const gchar *thisfn = "ofa_dossier_meta_bin_get_size_group";
	ofaDossierMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_META_BIN( bin ), NULL );

	priv = ofa_dossier_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: unmanaged column=%u", thisfn, column );
	return( NULL );
}

/*
 * just refuse any new text which would contain square brackets
 * as this is refused by underlying GKeyFile
 */
static void
on_dossier_name_insert_text( GtkEditable *editable, gchar *new_text, gint new_text_length, gint *position, ofaDossierMetaBin *bin )
{
	if( g_strstr_len( new_text, -1, "[" ) || g_strstr_len( new_text, -1, "]" )){
		g_signal_stop_emission_by_name( editable, "insert-text" );
	}
}

/*
 * Underlying GKeyFile does not allow modification of the group name.
 * More, once the dossier is created, it is too late to change the
 * DBMS provider.
 */
static void
on_dossier_name_changed( GtkEditable *editable, ofaDossierMetaBin *self )
{
#if 0
	ofaDossierMetaBinPrivate *priv;
	ofaDossierCollection *collection;
	const gchar *cstr;

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( editable ));

	g_free( priv->dossier_name );
	priv->dossier_name = g_strdup( cstr );

	collection = ofa_hub_get_dossier_collection( priv->hub );
#endif

	changed_composite( self );
}

static void
on_dbms_provider_changed( GtkComboBox *combo, ofaDossierMetaBin *self )
{
	static const gchar *thisfn = "ofa_dossier_meta_bin_on_dbms_provider_changed";
	ofaDossierMetaBinPrivate *priv;
	const gchar *canon_name;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) self );

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	if( !priv->initializing ){
		canon_name = gtk_combo_box_get_active_id( combo );
		g_free( priv->provider_name );
		priv->provider_name = g_strdup( canon_name );
		priv->provider = ofa_idbprovider_get_by_name( priv->hub, canon_name );
		changed_composite( self );
	}
}

static void
changed_composite( ofaDossierMetaBin *self )
{
#if 0
	ofaDossierMetaBinPrivate *priv;

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	gtk_widget_set_sensitive( priv->dbms_combo, ( priv->dossier_meta == NULL ));
#endif

	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_dossier_meta_bin_is_valid:
 * @bin: this #ofaDossierMetaBin instance.
 * @error_message: [allow-none]: the error message to be displayed.
 *
 * The bin of dialog is valid if :
 * - the dossier name is set
 * - a DBMS provider is set
 */
gboolean
ofa_dossier_meta_bin_is_valid( ofaDossierMetaBin *bin, gchar **error_message )
{
	ofaDossierMetaBinPrivate *priv;
	gboolean ok;
	gchar *str;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_META_BIN( bin ), FALSE );

	priv = ofa_dossier_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	ok = TRUE;
	str = NULL;

	/* check for dossier name */
	if( ok ) {
		if( !my_strlen( priv->dossier_name )){
			str = g_strdup( _( "Dossier name is not set" ));
			ok = FALSE;
		}
	}

	/* check for DBMS provider */
	if( ok ){
		if( !my_strlen( priv->provider_name )){
			str = g_strdup( _( "DBMS provider is not selected" ));
			ok = FALSE;
		}
	}

	if( error_message ){
		*error_message = str;
	} else {
		g_free( str );
	}

	return( ok );
}

/**
 * ofa_dossier_meta_bin_apply:
 * @bin: this #ofaDossierMetaBin instance.
 *
 * Define the dossier in user settings, which triggers a #ofaDossierStore
 * update.
 *
 * The #ofaDossierMeta object passed at initialization time is
 * instanciated if not already done, and updated with the meta datas.
 *
 * Returns: %TRUE if the dossier has been successfully defined in the
 * settings.
 */
gboolean
ofa_dossier_meta_bin_apply( ofaDossierMetaBin *bin )
{
	ofaDossierMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_META_BIN( bin ), FALSE );
	g_return_val_if_fail( ofa_dossier_meta_bin_is_valid( bin, NULL ), FALSE );

	priv = ofa_dossier_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( TRUE );
}

#if 0
/**
 * ofa_dossier_meta_bin_get_dossier_meta:
 * @bin: this #ofaDossierMetaBin instance.
 *
 * Returns: the #ofaDossierMeta object.
 */
ofaDossierMeta *
ofa_dossier_meta_bin_get_dossier_meta( ofaDossierMetaBin *bin )
{
	ofaDossierMetaBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_META_BIN( bin ), NULL );

	priv = ofa_dossier_meta_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->dossier_meta );
}

/**
 * ofa_dossier_meta_bin_set_dossier_meta:
 * @bin: this #ofaDossierMetaBin instance.
 * @dossier_meta: [allow-none]: the #ofaDossierMeta object.
 *
 * Set the #ofaDossierMeta object.
 */
void
ofa_dossier_meta_bin_set_dossier_meta( ofaDossierMetaBin *bin, ofaDossierMeta *dossier_meta )
{
	ofaDossierMetaBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_META_BIN( bin ));

	priv = ofa_dossier_meta_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_clear_object( &priv->dossier_meta );
	priv->dossier_meta = dossier_meta ? g_object_ref( dossier_meta ) : NULL;

	setup_dossier_meta( bin );
}
#endif

/*
 * settings are: "last_chosen_provider_name(s);"
 */
static void
read_settings( ofaDossierMetaBin *self )
{
	ofaDossierMetaBinPrivate *priv;
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;
	gchar *key;

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-dossier-meta", priv->settings_prefix );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->dbms_combo ), cstr );
	}

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaDossierMetaBin *self )
{
	ofaDossierMetaBinPrivate *priv;
	myISettings *settings;
	gchar *key, *str;

	priv = ofa_dossier_meta_bin_get_instance_private( self );

	str = g_strdup_printf( "%s;",
				priv->provider_name );

	settings = ofa_hub_get_user_settings( priv->hub );
	key = g_strdup_printf( "%s-dossier-meta", priv->settings_prefix );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( str );
}
