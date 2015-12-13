/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbprovider.h"

#include "core/ofa-file-dir.h"

#include "ui/ofa-application.h"
#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-dossier-store.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewBinPrivate {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkSizeGroup   *group0;
	GtkWidget      *dbms_combo;
	GtkWidget      *connect_infos_parent;
	ofaIDBEditor   *connect_infos;
	GtkWidget      *msg_label;

	/* initialization
	 */
	ofaMainWindow  *main_window;
	ofaFileDir     *dir;

	/* runtime data
	 */
	gchar          *dossier_name;
	gulong          prov_handler;
};

/* columns in DBMS provider combo box
 */
enum {
	DBMS_COL_PROVIDER = 0,
	DBMS_N_COLUMNS
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-dossier-new-bin.ui";

G_DEFINE_TYPE( ofaDossierNewBin, ofa_dossier_new_bin, GTK_TYPE_BIN )

static void     setup_bin( ofaDossierNewBin *bin );
static void     setup_dbms_provider( ofaDossierNewBin *bin );
static void     on_dossier_name_changed( GtkEditable *editable, ofaDossierNewBin *bin );
static void     on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewBin *self );
static void     on_connect_infos_changed( ofaIDBEditor *widget, ofaDossierNewBin *self );
static void     changed_composite( ofaDossierNewBin *bin );

static void
dossier_new_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_bin_finalize";
	ofaDossierNewBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_BIN( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_NEW_BIN( instance )->priv;

	g_free( priv->dossier_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_bin_parent_class )->finalize( instance );
}

static void
dossier_new_bin_dispose( GObject *instance )
{
	ofaDossierNewBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_BIN( instance ));

	priv = OFA_DOSSIER_NEW_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_bin_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_bin_init( ofaDossierNewBin *self )
{
	static const gchar *thisfn = "ofa_dossier_new_bin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBinPrivate );
}

static void
ofa_dossier_new_bin_class_init( ofaDossierNewBinClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierNewBinPrivate ));

	/**
	 * ofaDossierNewBin::ofa-changed:
	 *
	 * This signal is sent on the #ofaDossierNewBin when any of the
	 * underlying information is changed. This includes the dossier
	 * name, the DBMS provider, the connection informations and the
	 * DBMS root credentials
	 *
	 * Arguments are dossier name and connection informations.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierNewBin *bin,
	 * 						const gchar    *dname,
	 * 						ofaIDBEditor   *editor,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DOSSIER_NEW_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_POINTER );
}

/**
 * ofa_dossier_new_bin_new:
 * @main_window: the #ofaMainWindow main window.
 *
 * Returns: a newly defined composite widget which aggregates dossier
 * name, DBMS provider, connection informations and root credentials.
 */
ofaDossierNewBin *
ofa_dossier_new_bin_new( ofaMainWindow *main_window )
{
	ofaDossierNewBin *bin;

	bin = g_object_new( OFA_TYPE_DOSSIER_NEW_BIN, NULL );

	bin->priv->main_window = main_window;

	setup_bin( bin );

	return( bin );
}

/*
 * At initialization time, only setup the providers combo box
 * because the other parts of this windows depend of the selected
 * provider
 */
static void
setup_bin( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;
	GtkApplication *application;

	priv = bin->priv;
	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "dnb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "dnb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	/* dossier name */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-dossier-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_dossier_name_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-dossier-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	setup_dbms_provider( bin );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );

	/* setup file directory */
	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_APPLICATION( application ));

	priv->dir = ofa_application_get_file_dir( OFA_APPLICATION( application ));
	g_return_if_fail( priv->dir && OFA_IS_FILE_DIR( priv->dir ));

}

static void
setup_dbms_provider( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	GtkWidget *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	GList *prov_list, *ip;

	priv = bin->priv;

	combo = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-provider-combo" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));
	priv->dbms_combo = combo;

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			DBMS_N_COLUMNS,
			G_TYPE_STRING ));
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), tmodel );
	g_object_unref( tmodel );

	gtk_combo_box_set_id_column( GTK_COMBO_BOX( combo ), DBMS_COL_PROVIDER );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", DBMS_COL_PROVIDER );

	prov_list = ofa_idbprovider_get_list();

	for( ip=prov_list ; ip ; ip=ip->next ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DBMS_COL_PROVIDER, ip->data,
				-1 );
	}

	ofa_idbprovider_free_list( prov_list );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_dbms_provider_changed ), bin );

	/* setup the mnemonic widget on the label */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-provider-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), combo );

	/* take a pointer on the parent container of the DBMS widget before
	 *  selecting the default */
	priv->connect_infos_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-connect-infos" );
	g_return_if_fail( priv->connect_infos_parent && GTK_IS_CONTAINER( priv->connect_infos_parent ));

	gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), 0 );
}

/**
 * ofa_dossier_new_bin_get_size_group:
 * @bin: this #ofaDossierNewBin instance.
 * @column: the desire column.
 *
 * Returns: the #GtkSizeGroup which handles the desired @column.
 */
GtkSizeGroup *
ofa_dossier_new_bin_get_size_group( const ofaDossierNewBin *bin, guint column )
{
	ofaDossierNewBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), NULL );

	priv = bin->priv;

	if( !priv->dispose_has_run ){
		if( column == 0 ){
			return( priv->group0 );
		}
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofa_dossier_new_bin_set_provider:
 *
 * This must be called after having attached the widget to its parent.
 */
void
ofa_dossier_new_bin_set_provider( ofaDossierNewBin *bin, const gchar *provider )
{
	ofaDossierNewBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));
	g_return_if_fail( my_strlen( provider ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->dbms_combo ), provider );
	}
}

static void
on_dossier_name_changed( GtkEditable *editable, ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;

	priv = bin->priv;

	g_free( priv->dossier_name );
	priv->dossier_name = g_strdup( gtk_entry_get_text( GTK_ENTRY( editable )));

	changed_composite( bin );
}

static void
on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewBin *self )
{
	static const gchar *thisfn = "ofa_dossier_new_bin_on_dbms_provider_changed";
	ofaDossierNewBinPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	GtkWidget *child, *label;
	gchar *str, *prov_name;
	ofaIDBProvider *prov_instance;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) self );

	priv = self->priv;

	/* do we have finished with the initialization ? */
	if( priv->connect_infos_parent ){

		/* do we had a previous selection ? */
		if( priv->prov_handler ){
			g_return_if_fail( priv->connect_infos && OFA_IS_IDBEDITOR( priv->connect_infos ));
			/* last, remove the widget */
			child = gtk_bin_get_child( GTK_BIN( priv->connect_infos_parent ));
			if( child ){
				g_signal_handler_disconnect( child, priv->prov_handler );
				gtk_container_remove( GTK_CONTAINER( priv->connect_infos_parent ), child );
				priv->connect_infos = NULL;
			}
		}

		priv->prov_handler = 0;

		/* setup current provider */
		if( gtk_combo_box_get_active_iter( combo, &iter )){
			tmodel = gtk_combo_box_get_model( combo );
			gtk_tree_model_get( tmodel, &iter,
					DBMS_COL_PROVIDER, &prov_name,
					-1 );

			prov_instance = ofa_idbprovider_get_instance_by_name( prov_name );

			if( prov_instance ){
				/* let the DBMS initialize its own part */
				priv->connect_infos = ofa_idbprovider_new_editor( prov_instance, TRUE );
				gtk_container_add( GTK_CONTAINER( priv->connect_infos_parent ), GTK_WIDGET( priv->connect_infos ));
				my_utils_size_group_add_size_group(
						priv->group0, ofa_idbeditor_get_size_group( priv->connect_infos, 0 ));
				priv->prov_handler =
						g_signal_connect( priv->connect_infos,
								"ofa-changed" , G_CALLBACK( on_connect_infos_changed ), self );

			} else {
				str = g_strdup_printf( _( "Unable to handle %s DBMS provider" ), prov_name );
				label = gtk_label_new( str );
				g_free( str );
				gtk_label_set_line_wrap( GTK_LABEL( label ), TRUE );
				gtk_container_add( GTK_CONTAINER( priv->connect_infos_parent ), label );
			}

			g_clear_object( &prov_instance );
			g_free( prov_name );
		}

		changed_composite( self );
	}
}

/*
 * a callback on the "changed" signal sent by the ofaIDBEditor object
 *
 * the connection itself is validated from these connection informations
 * and the DBMS root credentials
 */
static void
on_connect_infos_changed( ofaIDBEditor *widget, ofaDossierNewBin *self )
{
	g_debug( "ofa_dossier_new_bin_on_connect_infos_changed" );

	changed_composite( self );
}

static void
changed_composite( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;

	priv = bin->priv;

	g_signal_emit_by_name( bin, "ofa-changed", priv->dossier_name, priv->connect_infos );
}

/**
 * ofa_dossier_new_bin_get_valid:
 * @bin: this #ofaDossierNewBin instance.
 * @error_message: [allow-none]: the error message to be displayed.
 *
 * The bin of dialog is valid if :
 * - the dossier name is set and doesn't exist yet
 * - the connection informations are valid
 */
gboolean
ofa_dossier_new_bin_get_valid( const ofaDossierNewBin *bin, gchar **error_message )
{
	ofaDossierNewBinPrivate *priv;
	gboolean ok;
	gchar *str;
	ofaIDBMeta *meta;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), FALSE );

	priv = bin->priv;
	ok = FALSE;
	str = NULL;

	if( !priv->dispose_has_run ){

		/* check for dossier name */
		if( !my_strlen( priv->dossier_name )){
			str = g_strdup( _( "Dossier name is not set" ));

		} else {
			meta = ofa_file_dir_get_meta( priv->dir, priv->dossier_name );
			if( meta ){
				str = g_strdup_printf( _( "%s: dossier is already defined" ), priv->dossier_name );
				g_clear_object( &meta );

			} else {
				ok = TRUE;
			}
		}

		/* check for connection informations */
		if( ok ){
			ok = ofa_idbeditor_get_valid( priv->connect_infos, &str );
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
 * ofa_dossier_new_bin_apply:
 *
 * Define the dossier in user settings, updating the ofaDossierStore
 * simultaneously.
 *
 * Returns: a newly created #ofaIDBMeta object.
 */
ofaIDBMeta *
ofa_dossier_new_bin_apply( const ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	ofaIDBProvider *provider;
	ofaIDBMeta *meta;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), NULL );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		provider = ofa_idbeditor_get_provider( priv->connect_infos );
		meta = ofa_idbprovider_new_meta( provider );
		ofa_idbmeta_set_dossier_name( meta, priv->dossier_name );
		ofa_file_dir_set_meta_from_editor( priv->dir, meta, priv->connect_infos );
		g_object_unref( provider );

		return( meta );
	}

	return( NULL );
}

/**
 * ofa_dossier_new_bin_get_editor:
 * @bin: this #ofaDossierNewBin instance.
 *
 * Returns: the #ofaIDBEditor widget.
 */
ofaIDBEditor *
ofa_dossier_new_bin_get_editor( const ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), NULL );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		return( priv->connect_infos );
	}

	g_return_val_if_reached( NULL );
}
