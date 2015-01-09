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

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "core/ofa-dbms-root-piece.h"

#include "ui/ofa-dossier-new-piece.h"

/* private instance data
 */
struct _ofaDossierNewPiecePrivate {
	gboolean          dispose_has_run;
	gboolean          from_widget_finalized;

	/* UI
	 */
	GtkContainer     *parent;
	GtkContainer     *container;
	GtkSizeGroup     *group;
	GtkWidget        *dbms_combo;
	GtkWidget        *connect_infos;
	ofaDBMSRootPiece *dbms_credentials;
	GtkWidget        *msg_label;
	GdkRGBA           color;

	/* runtime data
	 */
	gchar            *dname;
	gchar            *prov_name;
	gulong            prov_handler;
	ofaIDbms         *prov_module;
	void             *infos;			/* connection informations */
	gchar            *account;
	gchar            *password;
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

static const gchar *st_piece_xml        = PKGUIDIR "/ofa-dossier-new-piece.ui";
static const gchar *st_piece_id         = "DossierNewPiece";

G_DEFINE_TYPE( ofaDossierNewPiece, ofa_dossier_new_piece, G_TYPE_OBJECT )

static void     on_widget_finalized( ofaDossierNewPiece *piece, GObject *finalized_parent );
static void     setup_dbms_provider( ofaDossierNewPiece *piece );
static void     setup_dialog( ofaDossierNewPiece *piece );
static void     on_dname_changed( GtkEditable *editable, ofaDossierNewPiece *piece );
static void     on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewPiece *self );
static void     on_connect_infos_changed( ofaIDbms *instance, void *infos, ofaDossierNewPiece *self );
static void     on_dbms_credentials_changed( ofaDBMSRootPiece *piece, const gchar *account, const gchar *password, ofaDossierNewPiece *self );
static void     check_for_enable_dlg( ofaDossierNewPiece *piece );
static void     set_message( const ofaDossierNewPiece *piece, const gchar *msg );

static void
dossier_new_piece_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_piece_finalize";
	ofaDossierNewPiecePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_PIECE( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_NEW_PIECE( instance )->priv;

	g_free( priv->dname );
	g_free( priv->prov_name );
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_piece_parent_class )->finalize( instance );
}

static void
dossier_new_piece_dispose( GObject *instance )
{
	ofaDossierNewPiecePrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW_PIECE( instance ));

	priv = OFA_DOSSIER_NEW_PIECE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( !priv->from_widget_finalized ){
			g_object_weak_unref(
					G_OBJECT( priv->container ), ( GWeakNotify ) on_widget_finalized, instance );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_piece_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_piece_init( ofaDossierNewPiece *self )
{
	static const gchar *thisfn = "ofa_dossier_new_piece_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW_PIECE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_DOSSIER_NEW_PIECE, ofaDossierNewPiecePrivate );
}

static void
ofa_dossier_new_piece_class_init( ofaDossierNewPieceClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_piece_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_piece_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_piece_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierNewPiecePrivate ));

	/**
	 * ofaDossierNewPiece::changed:
	 *
	 * This signal is sent on the #ofaDossierNewPiece when any of the
	 * underlying information is changed. This includes the dossier
	 * name, the DBMS provider, the connection informations and the
	 * DBMS root credentials
	 *
	 * Arguments is dossier name.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierNewPiece *piece,
	 * 						const gchar      *dname,
	 * 						void             *infos,
	 * 						const gchar      *account,
	 * 						const gchar      *password,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_DOSSIER_NEW_PIECE,
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
 * ofa_dossier_new_piece_new:
 */
ofaDossierNewPiece *
ofa_dossier_new_piece_new( void )
{
	ofaDossierNewPiece *piece;

	piece = g_object_new( OFA_TYPE_DOSSIER_NEW_PIECE, NULL );

	return( piece );
}

/**
 * ofa_dossier_new_piece_attach_to:
 */
void
ofa_dossier_new_piece_attach_to( ofaDossierNewPiece *piece, GtkContainer *parent, GtkSizeGroup *group )
{
	ofaDossierNewPiecePrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	g_return_if_fail( !group || GTK_IS_SIZE_GROUP( group ));

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		window = my_utils_builder_load_from_path( st_piece_xml, st_piece_id );
		g_return_if_fail( window && GTK_IS_CONTAINER( window ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-alignment" );
		g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

		gtk_widget_reparent( widget, GTK_WIDGET( parent ));
		priv->parent = parent;
		priv->container = GTK_CONTAINER( widget );
		priv->group = group;

		g_object_weak_ref( G_OBJECT( widget ), ( GWeakNotify ) on_widget_finalized, piece );

		setup_dbms_provider( piece );
		setup_dialog( piece );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
on_widget_finalized( ofaDossierNewPiece *piece, GObject *finalized_widget )
{
	static const gchar *thisfn = "ofa_dossier_new_piece_on_widget_finalized";
	ofaDossierNewPiecePrivate *priv;

	g_debug( "%s: piece=%p, finalized_widget=%p (%s)",
			thisfn, ( void * ) piece, ( void * ) finalized_widget, G_OBJECT_TYPE_NAME( finalized_widget ));

	priv = piece->priv;
	priv->from_widget_finalized = TRUE;

	g_object_unref( piece );
}

static void
setup_dbms_provider( ofaDossierNewPiece *piece )
{
	ofaDossierNewPiecePrivate *priv;
	GtkWidget *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	GSList *prov_list, *ip;

	priv = piece->priv;

	combo = my_utils_container_get_child_by_name( priv->container, "dn-provider" );
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

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_dbms_provider_changed ), piece );

	/* take a pointer on the parent container of the DBMS widget before
	 *  selecting the default */
	priv->connect_infos = my_utils_container_get_child_by_name( priv->container, "dn-connect-infos" );
	g_return_if_fail( priv->connect_infos && GTK_IS_CONTAINER( priv->connect_infos ));

	gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), 0 );
}

static void
setup_dialog( ofaDossierNewPiece *piece )
{
	ofaDossierNewPiecePrivate *priv;
	GtkWidget *label, *entry, *parent;

	priv =piece->priv;

	if( priv->group ){
		label = my_utils_container_get_child_by_name( priv->container, "dn-label1" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );

		label = my_utils_container_get_child_by_name( priv->container, "dn-label2" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );
	}

	entry = my_utils_container_get_child_by_name( priv->container, "dn-dname" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_dname_changed ), piece );

	parent = my_utils_container_get_child_by_name( priv->container, "dn-dbms-credentials" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->dbms_credentials = ofa_dbms_root_piece_new();
	ofa_dbms_root_piece_attach_to( priv->dbms_credentials, GTK_CONTAINER( parent ), priv->group );

	g_signal_connect(
			priv->dbms_credentials, "changed", G_CALLBACK( on_dbms_credentials_changed ), piece );

	priv->msg_label = my_utils_container_get_child_by_name( priv->container, "dn-message" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
	gdk_rgba_parse( &priv->color, "#ff0000" );
}

/**
 * ofa_dossier_new_piece_set_frame:
 *
 * This must be called after having attached the widget to its parent.
 */
void
ofa_dossier_new_piece_set_frame( ofaDossierNewPiece *piece, gboolean visible )
{
	ofaDossierNewPiecePrivate *priv;
	GtkWidget *frame, *label;

	g_return_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ));

	priv = piece->priv;
	g_return_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ));

	if( !priv->dispose_has_run ){

		frame = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->container ), "dn-frame" );
		g_return_if_fail( frame && GTK_IS_FRAME( frame ));
		gtk_frame_set_shadow_type( GTK_FRAME( frame ), visible ? GTK_SHADOW_IN : GTK_SHADOW_NONE );

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( priv->container ), "dn-frame-label" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_label_set_markup( GTK_LABEL( label ), visible ? _( "<b> Dossier properties </b>" ) : "" );

		gtk_widget_show_all( GTK_WIDGET( priv->container ));
	}
}

/**
 * ofa_dossier_new_piece_set_provider:
 *
 * This must be called after having attached the widget to its parent.
 */
void
ofa_dossier_new_piece_set_provider( ofaDossierNewPiece *piece, const gchar *provider )
{
	ofaDossierNewPiecePrivate *priv;

	g_return_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ));

	priv = piece->priv;
	g_return_if_fail( priv->container && GTK_IS_CONTAINER( priv->container ));

	if( !priv->dispose_has_run && provider && g_utf8_strlen( provider, -1 )){

		gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->dbms_combo ), provider );
	}
}

static void
on_dname_changed( GtkEditable *editable, ofaDossierNewPiece *piece )
{
	ofaDossierNewPiecePrivate *priv;

	priv = piece->priv;

	g_free( priv->dname );
	priv->dname = g_strdup( gtk_entry_get_text( GTK_ENTRY( editable )));

	check_for_enable_dlg( piece );
}

static void
on_dbms_provider_changed( GtkComboBox *combo, ofaDossierNewPiece *self )
{
	static const gchar *thisfn = "ofa_dossier_new_piece_on_dbms_provider_changed";
	ofaDossierNewPiecePrivate *priv;
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
on_connect_infos_changed( ofaIDbms *instance, void *infos, ofaDossierNewPiece *self )
{
	ofaDossierNewPiecePrivate *priv;

	g_debug( "ofa_dossier_new_piece_on_connect_infos_changed" );

	priv = self->priv;
	priv->infos = infos;

	check_for_enable_dlg( self );
}

static void
on_dbms_credentials_changed( ofaDBMSRootPiece *piece, const gchar *account, const gchar *password, ofaDossierNewPiece *self )
{
	ofaDossierNewPiecePrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( account );

	g_free( priv->password );
	priv->password = g_strdup( password );

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierNewPiece *piece )
{
	ofaDossierNewPiecePrivate *priv;

	priv = piece->priv;

	g_signal_emit_by_name( piece, "changed", priv->dname, priv->infos, priv->account, priv->password );
}

/**
 * ofa_dossier_new_piece_is_valid:
 *
 * The piece of dialog is valid if :
 * - the dossier name is set and doesn't exist yet
 * - the connection informations and the DBMS root credentials are valid
 */
gboolean
ofa_dossier_new_piece_is_valid( const ofaDossierNewPiece *piece )
{
	ofaDossierNewPiecePrivate *priv;
	gboolean ok, oka, okb, okc;
	gchar *str;

	g_return_val_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ), FALSE );

	priv = piece->priv;
	ok = FALSE;
	set_message( piece, "" );

	if( !priv->dispose_has_run ){

		oka = FALSE;

		/* check for dossier name */
		if( !priv->dname || !g_utf8_strlen( priv->dname, -1 )){
			set_message( piece, _( "Dossier name is not set" ));

		} else if( ofa_settings_has_dossier( priv->dname )){
			str = g_strdup_printf( _( "%s: dossier is already defined" ), priv->dname );
			set_message( piece, str );
			g_free( str );

		} else {
			oka = TRUE;
		}

		/* check for connection informations */
		okb = ofa_idbms_connect_enter_is_valid( priv->prov_module, GTK_CONTAINER( priv->connect_infos ));

		/* check for credentials */
		okc = ofa_idbms_connect_ex( priv->prov_module, priv->infos, priv->account, priv->password );
		ofa_dbms_root_piece_set_valid( priv->dbms_credentials, okc );

		ok = oka && okb && okc;
	}

	return( ok );
}

/**
 * ofa_dossier_new_piece_apply:
 *
 * Define the dossier in user settings
 */
gboolean
ofa_dossier_new_piece_apply( const ofaDossierNewPiece *piece )
{
	ofaDossierNewPiecePrivate *priv;
	gboolean ok;

	g_return_val_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ), FALSE );

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		ok = ofa_idbms_connect_enter_apply( priv->prov_module, priv->dname, priv->infos );

		return( ok );
	}

	return( FALSE );
}

/**
 * ofa_dossier_new_piece_get_dname:
 *
 * Return the dossier name
 */
void
ofa_dossier_new_piece_get_dname( const ofaDossierNewPiece *piece, gchar **dname )
{
	ofaDossierNewPiecePrivate *priv;

	g_return_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ));
	g_return_if_fail( dname );

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		*dname = g_strdup( priv->dname );
	}
}

/**
 * ofa_dossier_new_piece_get_database:
 *
 * Return the dossier name
 */
void
ofa_dossier_new_piece_get_database( const ofaDossierNewPiece *piece, gchar **database )
{
	ofaDossierNewPiecePrivate *priv;

	g_return_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ));
	g_return_if_fail( database );

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		*database = ofa_idbms_connect_enter_get_database(
							priv->prov_module, GTK_CONTAINER( priv->connect_infos ));
	}
}

/**
 * ofa_dossier_new_piece_get_credentials:
 *
 * Return the content of DBMS root credentials
 */
void
ofa_dossier_new_piece_get_credentials( const ofaDossierNewPiece *piece, gchar **account, gchar **password )
{
	ofaDossierNewPiecePrivate *priv;

	g_return_if_fail( piece && OFA_IS_DOSSIER_NEW_PIECE( piece ));

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		*account = g_strdup( priv->account );
		*password = g_strdup( priv->password );
	}
}

static void
set_message( const ofaDossierNewPiece *piece, const gchar *msg )
{
	ofaDossierNewPiecePrivate *priv;

	priv = piece->priv;

	if( priv->msg_label ){

		gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg );
		gtk_widget_override_color( priv->msg_label, GTK_STATE_FLAG_NORMAL, &priv->color );
	}
}
