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
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "core/ofa-dbms-root-bin.h"

#include "ui/ofa-dossier-new-bin.h"
#include "ui/ofa-dossier-store.h"

/* private instance data
 */
struct _ofaDossierNewBinPrivate {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkSizeGroup   *group0;
	GtkWidget      *dbms_combo;
	GtkWidget      *connect_infos_parent;
	GtkWidget      *connect_infos;
	ofaDBMSRootBin *dbms_credentials;
	GtkWidget      *msg_label;

	/* runtime data
	 */
	gchar          *dname;
	gchar          *prov_name;
	gulong          prov_handler;
	ofaIDbms       *prov_module;
	void           *infos;				/* connection informations */
	gchar          *account;
	gchar          *password;
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

static void     setup_composite( ofaDossierNewBin *bin );
static void     setup_dbms_provider( ofaDossierNewBin *bin );
static void     on_dname_changed( GtkEditable *editable, ofaDossierNewBin *bin );
static void     on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewBin *self );
static void     on_connect_infos_changed( ofaIDbms *instance, void *infos, ofaDossierNewBin *self );
static void     on_dbms_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNewBin *self );
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

	g_free( priv->dname );
	g_free( priv->prov_name );
	g_free( priv->account );
	g_free( priv->password );

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
		if( priv->group0 ){
			g_clear_object( &priv->group0 );
		}
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
	 * ofaDossierNewBin::changed:
	 *
	 * This signal is sent on the #ofaDossierNewBin when any of the
	 * underlying information is changed. This includes the dossier
	 * name, the DBMS provider, the connection informations and the
	 * DBMS root credentials
	 *
	 * Arguments is dossier name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierNewBin *bin,
	 * 						const gchar      *dname,
	 * 						void             *infos,
	 * 						const gchar      *account,
	 * 						const gchar      *password,
	 * 						gpointer          user_data );
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
				4,
				G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING );
}

/**
 * ofa_dossier_new_bin_new:
 */
ofaDossierNewBin *
ofa_dossier_new_bin_new( void )
{
	ofaDossierNewBin *bin;

	bin = g_object_new( OFA_TYPE_DOSSIER_NEW_BIN, NULL );

	setup_composite( bin );

	return( bin );
}

/*
 * setup_composite:
 * At initialization time, only setup the providers combo box
 * because the other parts of this windows depend of the selected
 * provider
 */
static void
setup_composite( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *parent, *label;

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
	g_signal_connect( entry, "changed", G_CALLBACK( on_dname_changed ), bin );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-dossier-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	setup_dbms_provider( bin );

	/* administrative credentials */
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dnb-dbms-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dbms_credentials = ofa_dbms_root_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->dbms_credentials ));
	my_utils_size_group_add_size_group(
			priv->group0, ofa_dbms_root_bin_get_size_group( priv->dbms_credentials, 0 ));
	g_signal_connect(
			priv->dbms_credentials, "ofa-changed", G_CALLBACK( on_dbms_credentials_changed ), bin );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_dbms_provider( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	GtkWidget *combo, *label;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	GSList *prov_list, *ip;

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

	prov_list = ofa_idbms_get_providers_list();

	for( ip=prov_list ; ip ; ip=ip->next ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				DBMS_COL_PROVIDER, ip->data,
				-1 );
	}

	ofa_idbms_free_providers_list( prov_list );

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
 * ofa_dossier_new_bin_set_frame:
 *
 * This must be called after having attached the widget to its parent.
 */
void
ofa_dossier_new_bin_set_frame( ofaDossierNewBin *bin, gboolean visible )
{
	ofaDossierNewBinPrivate *priv;
	GtkWidget *frame, *label;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		frame = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-frame" );
		g_return_if_fail( frame && GTK_IS_FRAME( frame ));
		gtk_frame_set_shadow_type( GTK_FRAME( frame ), visible ? GTK_SHADOW_IN : GTK_SHADOW_NONE );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-frame-label" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_label_set_markup( GTK_LABEL( label ), visible ? _( " Dossier properties " ) : "" );

		gtk_widget_show_all( GTK_WIDGET( bin ));
	}
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
on_dname_changed( GtkEditable *editable, ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;

	priv = bin->priv;

	g_free( priv->dname );
	priv->dname = g_strdup( gtk_entry_get_text( GTK_ENTRY( editable )));

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
	gchar *str;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) self );

	priv = self->priv;

	/* do we have finished with the initialization ? */
	if( priv->connect_infos_parent ){

		/* do we had a previous selection ? */
		if( priv->prov_handler ){
			g_free( priv->prov_name );
			g_signal_handler_disconnect( priv->prov_module, priv->prov_handler );
			g_clear_object( &priv->prov_module );
			/* last, remove the widget */
			child = gtk_bin_get_child( GTK_BIN( priv->connect_infos_parent ));
			if( child ){
				gtk_container_remove( GTK_CONTAINER( priv->connect_infos_parent ), child );
				priv->connect_infos = NULL;
			}
		}

		priv->prov_name = NULL;
		priv->prov_handler = 0;
		priv->infos = NULL;

		/* setup current provider */
		if( gtk_combo_box_get_active_iter( combo, &iter )){
			tmodel = gtk_combo_box_get_model( combo );
			gtk_tree_model_get( tmodel, &iter,
					DBMS_COL_PROVIDER, &priv->prov_name,
					-1 );

			priv->prov_module = ofa_idbms_get_provider_by_name( priv->prov_name );

			if( priv->prov_module ){
				/* let the DBMS initialize its own part */
				priv->prov_handler =
						g_signal_connect( priv->prov_module,
								"dbms-changed" , G_CALLBACK( on_connect_infos_changed ), self );
				priv->connect_infos = ofa_idbms_connect_enter_new( priv->prov_module, priv->group0 );
				gtk_container_add( GTK_CONTAINER( priv->connect_infos_parent ), priv->connect_infos );

			} else {
				str = g_strdup_printf( _( "Unable to handle %s DBMS provider" ), priv->prov_name );
				label = gtk_label_new( str );
				g_free( str );
				gtk_label_set_line_wrap( GTK_LABEL( label ), TRUE );
				gtk_container_add( GTK_CONTAINER( priv->connect_infos_parent ), label );
			}
		}

		changed_composite( self );
	}
}

/*
 * a callback on the "changed" signal sent by the ofaIDbms module
 * the 'infos' data is a handler on connection informations
 *
 * the connection itself is validated from these connection informations
 * and the DBMS root credentials
 */
static void
on_connect_infos_changed( ofaIDbms *instance, void *infos, ofaDossierNewBin *self )
{
	ofaDossierNewBinPrivate *priv;

	g_debug( "ofa_dossier_new_bin_on_connect_infos_changed" );

	priv = self->priv;
	priv->infos = infos;

	changed_composite( self );
}

static void
on_dbms_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNewBin *self )
{
	ofaDossierNewBinPrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( account );

	g_free( priv->password );
	priv->password = g_strdup( password );

	changed_composite( self );
}

static void
changed_composite( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;

	priv = bin->priv;

	g_signal_emit_by_name( bin, "ofa-changed", priv->dname, priv->infos, priv->account, priv->password );
}

/**
 * ofa_dossier_new_bin_is_valid:
 * @bin: this #ofaDossierNewBin instance.
 * @error_message: [allow-none]: the error message to be displayed.
 *
 * The bin of dialog is valid if :
 * - the dossier name is set and doesn't exist yet
 * - the connection informations and the DBMS root credentials are valid
 */
gboolean
ofa_dossier_new_bin_is_valid( const ofaDossierNewBin *bin, gchar **error_message )
{
	ofaDossierNewBinPrivate *priv;
	gboolean ok, root_ok;
	gchar *str;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), FALSE );

	priv = bin->priv;
	ok = FALSE;
	str = NULL;

	if( !priv->dispose_has_run ){

		/* check for dossier name */
		if( !my_strlen( priv->dname )){
			str = g_strdup( _( "Dossier name is not set" ));

		} else if( ofa_settings_has_dossier( priv->dname )){
			str = g_strdup_printf( _( "%s: dossier is already defined" ), priv->dname );

		/* check for connection informations */
		} else {
			ok = ofa_idbms_connect_enter_is_valid( priv->prov_module, priv->connect_infos, &str );
		}

		/* check for DBMS root credentials in all cases s that the DBMS
		 * status message is erased when credentials are no longer valid
		 * (and even another error message is displayed)
		 * ofa_dbms_root_credential_bin_is_valid() can only be called
		 * when the dossier has already been registered */
		root_ok = ofa_idbms_connect_ex( priv->prov_module, priv->infos, priv->account, priv->password );
		ofa_dbms_root_bin_set_valid( priv->dbms_credentials, root_ok );
		if( ok && !root_ok ){
			str = g_strdup( _( "DBMS root credentials are not valid" ));
		}
		ok &= root_ok;
	}

	if( error_message ){
		*error_message = str;
	}

	return( ok );
}

/**
 * ofa_dossier_new_bin_apply:
 *
 * Define the dossier in user settings, updating the ofaDossierStore
 * simultaneously.
 */
gboolean
ofa_dossier_new_bin_apply( const ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), FALSE );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		ok = ofa_idbms_connect_enter_apply( priv->prov_module, priv->dname, priv->infos );

		return( ok );
	}

	return( FALSE );
}

/**
 * ofa_dossier_new_bin_get_dname:
 *
 * Return the dossier name
 */
void
ofa_dossier_new_bin_get_dname( const ofaDossierNewBin *bin, gchar **dname )
{
	ofaDossierNewBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));
	g_return_if_fail( dname );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		*dname = g_strdup( priv->dname );
	}
}

/**
 * ofa_dossier_new_bin_get_database:
 *
 * Return the database name
 */
void
ofa_dossier_new_bin_get_database( const ofaDossierNewBin *bin, gchar **database )
{
	ofaDossierNewBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));
	g_return_if_fail( database );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		*database = ofa_idbms_connect_enter_get_database( priv->prov_module, priv->connect_infos );
	}
}

/**
 * ofa_dossier_new_bin_get_credentials:
 *
 * Return the content of DBMS root credentials
 */
void
ofa_dossier_new_bin_get_credentials( const ofaDossierNewBin *bin, gchar **account, gchar **password )
{
	ofaDossierNewBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		*account = g_strdup( priv->account );
		*password = g_strdup( priv->password );
	}
}
