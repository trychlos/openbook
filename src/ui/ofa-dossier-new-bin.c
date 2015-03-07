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

/* private instance data
 */
struct _ofaDossierNewBinPrivate {
	gboolean        dispose_has_run;

	/* UI
	 */
	GtkSizeGroup   *group;
	GtkWidget      *dbms_combo;
	GtkWidget      *connect_infos;
	ofaDBMSRootBin *dbms_credentials;
	GtkWidget      *msg_label;
	GdkRGBA         color;

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
static const gchar *st_bin_id           = "DossierNewBin";

G_DEFINE_TYPE( ofaDossierNewBin, ofa_dossier_new_bin, GTK_TYPE_BIN )

static void     setup_dbms_provider( ofaDossierNewBin *bin );
static void     setup_dialog( ofaDossierNewBin *bin );
static void     on_dname_changed( GtkEditable *editable, ofaDossierNewBin *bin );
static void     on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewBin *self );
static void     on_connect_infos_changed( ofaIDbms *instance, void *infos, ofaDossierNewBin *self );
static void     on_dbms_credentials_changed( ofaDBMSRootBin *bin, const gchar *account, const gchar *password, ofaDossierNewBin *self );
static void     check_for_enable_dlg( ofaDossierNewBin *bin );
static void     set_message( const ofaDossierNewBin *bin, const gchar *msg );

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
				"changed",
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

	return( bin );
}

/**
 * ofa_dossier_new_bin_attach_to:
 */
void
ofa_dossier_new_bin_attach_to( ofaDossierNewBin *bin, GtkContainer *parent, GtkSizeGroup *group )
{
	ofaDossierNewBinPrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	g_return_if_fail( !group || GTK_IS_SIZE_GROUP( group ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		window = my_utils_builder_load_from_path( st_bin_xml, st_bin_id );
		g_return_if_fail( window && GTK_IS_CONTAINER( window ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-alignment" );
		g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

		gtk_widget_reparent( widget, GTK_WIDGET( bin ));
		gtk_container_add( parent, GTK_WIDGET( bin ));

		priv->group = group;

		setup_dbms_provider( bin );
		setup_dialog( bin );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
setup_dbms_provider( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	GSList *prov_list, *ip;

	priv = bin->priv;

	combo = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-provider" );
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

	/* take a pointer on the parent container of the DBMS widget before
	 *  selecting the default */
	priv->connect_infos = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-connect-infos" );
	g_return_if_fail( priv->connect_infos && GTK_IS_CONTAINER( priv->connect_infos ));

	gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), 0 );
}

static void
setup_dialog( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	GtkWidget *label, *entry, *parent;

	priv =bin->priv;

	if( priv->group ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-label1" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-label2" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );
	}

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-dname" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_dname_changed ), bin );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-dbms-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dbms_credentials = ofa_dbms_root_bin_new();
	ofa_dbms_root_bin_attach_to( priv->dbms_credentials, GTK_CONTAINER( parent ), priv->group );

	g_signal_connect(
			priv->dbms_credentials, "changed", G_CALLBACK( on_dbms_credentials_changed ), bin );

	priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dn-message" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	gdk_rgba_parse( &priv->color, "#ff0000" );
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
		gtk_label_set_markup( GTK_LABEL( label ), visible ? _( "<b> Dossier properties </b>" ) : "" );

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

	priv = bin->priv;

	if( !priv->dispose_has_run && provider && g_utf8_strlen( provider, -1 )){

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

	check_for_enable_dlg( bin );
}

static void
on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewBin *self )
{
	static const gchar *thisfn = "ofa_dossier_new_bin_on_dbms_provider_changed";
	ofaDossierNewBinPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	GtkWidget *child;
	gchar *str;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) self );

	priv = self->priv;
	set_message( self, "" );

	/* do we have finished with the initialization ? */
	if( priv->connect_infos ){

		/* do we had a previous selection ? */
		if( priv->prov_handler ){
			g_free( priv->prov_name );
			g_signal_handler_disconnect( priv->prov_module, priv->prov_handler );
			g_clear_object( &priv->prov_module );
			/* last, remove the widget */
			child = gtk_bin_get_child( GTK_BIN( priv->connect_infos ));
			if( child ){
				gtk_container_remove( GTK_CONTAINER( priv->connect_infos ), child );
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
				ofa_idbms_connect_enter_attach_to(
						priv->prov_module, GTK_CONTAINER( priv->connect_infos ), priv->group );

				priv->prov_handler = g_signal_connect(
												priv->prov_module,
												"changed" , G_CALLBACK( on_connect_infos_changed ), self );

			} else {
				str = g_strdup_printf( _( "Unable to handle %s DBMS provider" ), priv->prov_name );
				set_message( self, str );
				g_free( str );
			}
		}

		check_for_enable_dlg( self );
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

	check_for_enable_dlg( self );
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

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;

	priv = bin->priv;

	g_signal_emit_by_name( bin, "changed", priv->dname, priv->infos, priv->account, priv->password );
}

/**
 * ofa_dossier_new_bin_is_valid:
 *
 * The bin of dialog is valid if :
 * - the dossier name is set and doesn't exist yet
 * - the connection informations and the DBMS root credentials are valid
 */
gboolean
ofa_dossier_new_bin_is_valid( const ofaDossierNewBin *bin )
{
	ofaDossierNewBinPrivate *priv;
	gboolean ok, oka, okb, okc;
	gchar *str;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ), FALSE );

	priv = bin->priv;
	ok = FALSE;
	set_message( bin, "" );

	if( !priv->dispose_has_run ){

		oka = FALSE;

		/* check for dossier name */
		if( !priv->dname || !g_utf8_strlen( priv->dname, -1 )){
			set_message( bin, _( "Dossier name is not set" ));

		} else if( ofa_settings_has_dossier( priv->dname )){
			str = g_strdup_printf( _( "%s: dossier is already defined" ), priv->dname );
			set_message( bin, str );
			g_free( str );

		} else {
			oka = TRUE;
		}

		/* check for connection informations */
		okb = ofa_idbms_connect_enter_is_valid( priv->prov_module, GTK_CONTAINER( priv->connect_infos ));

		/* check for credentials */
		okc = ofa_idbms_connect_ex( priv->prov_module, priv->infos, priv->account, priv->password );
		ofa_dbms_root_bin_set_valid( priv->dbms_credentials, okc );

		ok = oka && okb && okc;
	}

	return( ok );
}

/**
 * ofa_dossier_new_bin_apply:
 *
 * Define the dossier in user settings
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
 * Return the dossier name
 */
void
ofa_dossier_new_bin_get_database( const ofaDossierNewBin *bin, gchar **database )
{
	ofaDossierNewBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_NEW_BIN( bin ));
	g_return_if_fail( database );

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		*database = ofa_idbms_connect_enter_get_database(
							priv->prov_module, GTK_CONTAINER( priv->connect_infos ));
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

static void
set_message( const ofaDossierNewBin *bin, const gchar *msg )
{
	ofaDossierNewBinPrivate *priv;

	priv = bin->priv;

	if( priv->msg_label ){

		gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg );
		gtk_widget_override_color( priv->msg_label, GTK_STATE_FLAG_NORMAL, &priv->color );
	}
}
