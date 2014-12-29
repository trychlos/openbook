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
#include "api/ofa-dbms.h"

#include "core/ofa-dbms-root-piece.h"

/* private instance data
 */
struct _ofaDBMSRootPiecePrivate {
	gboolean      dispose_has_run;

	/* initialization
	 */
	gchar        *dname;

	/* UI
	 */
	GtkContainer *parent;
	GtkContainer *container;
	GtkSizeGroup *group;
	GtkWidget    *account_entry;
	GtkWidget    *password_entry;
	GtkWidget    *msg_label;

	/* runtime data
	 */
	gchar        *account;
	gchar        *password;
	gboolean      ok;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_piece_xml        = PKGUIDIR "/ofa-dbms-root-piece.ui";
static const gchar *st_piece_id         = "DBMSRootPiece";

G_DEFINE_TYPE( ofaDBMSRootPiece, ofa_dbms_root_piece, G_TYPE_OBJECT )

static void     on_parent_finalized( ofaDBMSRootPiece *piece, GObject *finalized_parent );
static void     setup_dialog( ofaDBMSRootPiece *piece );
static void     on_account_changed( GtkEditable *entry, ofaDBMSRootPiece *self );
static void     on_password_changed( GtkEditable *entry, ofaDBMSRootPiece *self );
static void     check_for_enable_dlg( ofaDBMSRootPiece *self );
static void     set_message( ofaDBMSRootPiece *piece );

static void
dbms_root_piece_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbms_root_piece_finalize";
	ofaDBMSRootPiecePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBMS_ROOT_PIECE( instance ));

	/* free data members here */
	priv = OFA_DBMS_ROOT_PIECE( instance )->priv;

	g_free( priv->dname );
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_root_piece_parent_class )->finalize( instance );
}

static void
dbms_root_piece_dispose( GObject *instance )
{
	ofaDBMSRootPiecePrivate *priv;

	g_return_if_fail( instance && OFA_IS_DBMS_ROOT_PIECE( instance ));

	priv = OFA_DBMS_ROOT_PIECE( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_root_piece_parent_class )->dispose( instance );
}

static void
ofa_dbms_root_piece_init( ofaDBMSRootPiece *self )
{
	static const gchar *thisfn = "ofa_dbms_root_piece_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DBMS_ROOT_PIECE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_DBMS_ROOT_PIECE, ofaDBMSRootPiecePrivate );
}

static void
ofa_dbms_root_piece_class_init( ofaDBMSRootPieceClass *klass )
{
	static const gchar *thisfn = "ofa_dbms_root_piece_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbms_root_piece_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbms_root_piece_finalize;

	g_type_class_add_private( klass, sizeof( ofaDBMSRootPiecePrivate ));

	/**
	 * ofaDBMSRootPiece::changed:
	 *
	 * This signal is sent on the #ofaDBMSRootPiece when the account or
	 * the password are changed.
	 *
	 * Arguments are new account and password.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDBMSRootPiece *piece,
	 * 						const gchar    *account,
	 * 						const gchar    *password,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_DBMS_ROOT_PIECE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_STRING, G_TYPE_STRING );
}

/**
 * ofa_dbms_root_piece_new:
 */
ofaDBMSRootPiece *
ofa_dbms_root_piece_new( void )
{
	ofaDBMSRootPiece *piece;

	piece = g_object_new( OFA_TYPE_DBMS_ROOT_PIECE, NULL );

	return( piece );
}

/**
 * ofa_dbms_root_piece_attach_to:
 */
void
ofa_dbms_root_piece_attach_to( ofaDBMSRootPiece *piece, GtkContainer *parent, GtkSizeGroup *group )
{
	ofaDBMSRootPiecePrivate *priv;
	GtkWidget *window, *widget;

	g_return_if_fail( piece && OFA_IS_DBMS_ROOT_PIECE( piece ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	g_return_if_fail( !group || GTK_IS_SIZE_GROUP( group ));

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		window = my_utils_builder_load_from_path( st_piece_xml, st_piece_id );
		g_return_if_fail( window && GTK_IS_CONTAINER( window ));

		widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "dra-top" );
		g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));

		gtk_widget_reparent( widget, GTK_WIDGET( parent ));
		priv->parent = parent;
		priv->container = GTK_CONTAINER( widget );
		priv->group = group;

		g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, piece );

		setup_dialog( piece );

		gtk_widget_show_all( GTK_WIDGET( parent ));
	}
}

static void
on_parent_finalized( ofaDBMSRootPiece *piece, GObject *finalized_parent )
{
	static const gchar *thisfn = "ofa_dbms_root_piece_on_parent_finalized";

	g_debug( "%s: piece=%p, finalized_parent=%p",
			thisfn, ( void * ) piece, ( void * ) finalized_parent );

	g_object_unref( piece );
}

static void
setup_dialog( ofaDBMSRootPiece *piece )
{
	ofaDBMSRootPiecePrivate *priv;
	GtkWidget *label;

	priv =piece->priv;

	if( priv->group ){
		label = my_utils_container_get_child_by_name( priv->container, "dra-label1" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );

		label = my_utils_container_get_child_by_name( priv->container, "dra-label2" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		gtk_size_group_add_widget( priv->group, label );
	}

	priv->account_entry = my_utils_container_get_child_by_name( priv->container, "dra-account" );
	g_return_if_fail( priv->account_entry && GTK_IS_ENTRY( priv->account_entry ));
	g_signal_connect(
			G_OBJECT( priv->account_entry ), "changed", G_CALLBACK( on_account_changed ), piece );

	priv->password_entry = my_utils_container_get_child_by_name( priv->container, "dra-password" );
	g_return_if_fail( priv->password_entry && GTK_IS_ENTRY( priv->password_entry ));
	g_signal_connect(
			G_OBJECT( priv->password_entry ), "changed", G_CALLBACK( on_password_changed ), piece );

	priv->msg_label = my_utils_container_get_child_by_name( priv->container, "dra-msg" );
	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));

	check_for_enable_dlg( piece );
}

/**
 * ofa_dbms_root_piece_set_dossier:
 */
void
ofa_dbms_root_piece_set_dossier( ofaDBMSRootPiece *piece, const gchar *dname )
{
	ofaDBMSRootPiecePrivate *priv;

	g_return_if_fail( piece && OFA_IS_DBMS_ROOT_PIECE( piece ));
	g_return_if_fail( dname && g_utf8_strlen( dname, -1 ));

	priv = piece->priv;

	g_return_if_fail( !priv->dname );

	if( !priv->dispose_has_run ){

		priv->dname = g_strdup( dname );
	}
}

static void
on_account_changed( GtkEditable *entry, ofaDBMSRootPiece *self )
{
	ofaDBMSRootPiecePrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

static void
on_password_changed( GtkEditable *entry, ofaDBMSRootPiece *self )
{
	ofaDBMSRootPiecePrivate *priv;

	priv = self->priv;

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	check_for_enable_dlg( self );
}

/*
 * test the DBMS root connection by trying to connect with empty dbname
 */
static void
check_for_enable_dlg( ofaDBMSRootPiece *self )
{
	ofaDBMSRootPiecePrivate *priv;
	ofaDbms *dbms;

	priv = self->priv;
	priv->ok = FALSE;

	if( priv->dname && g_utf8_strlen( priv->dname, -1 ) &&
			priv->account && g_utf8_strlen( priv->account, -1 )){

		dbms = ofa_dbms_new();

		priv->ok = ofa_dbms_connect(
						dbms, priv->dname, NULL, priv->account, priv->password, FALSE );

		g_object_unref( dbms );
	}

	set_message( self );
	g_signal_emit_by_name( self, "changed", priv->account, priv->password );
}

/**
 * ofa_dbms_root_piece_is_valid:
 */
gboolean
ofa_dbms_root_piece_is_valid( const ofaDBMSRootPiece *piece )
{
	ofaDBMSRootPiecePrivate *priv;

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		return( priv->ok );
	}

	return( FALSE );
}

/**
 * ofa_dbms_root_piece_set_credentials:
 */
void
ofa_dbms_root_piece_set_credentials( ofaDBMSRootPiece *piece, const gchar *account, const gchar *password )
{
	ofaDBMSRootPiecePrivate *priv;

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account );
		gtk_entry_set_text( GTK_ENTRY( priv->password_entry ), password );
	}
}

/**
 * ofa_dbms_root_piece_set_valid:
 */
void
ofa_dbms_root_piece_set_valid( ofaDBMSRootPiece *piece, gboolean valid )
{
	ofaDBMSRootPiecePrivate *priv;

	priv = piece->priv;

	if( !priv->dispose_has_run ){

		priv->ok = valid;
		set_message( piece );
	}
}

static void
set_message( ofaDBMSRootPiece *piece )
{
	ofaDBMSRootPiecePrivate *priv;
	GdkRGBA color;

	priv = piece->priv;

	if( priv->msg_label ){
		gtk_label_set_text( GTK_LABEL( priv->msg_label ),
				priv->ok ? _( "DB server connection is OK" ) : _( "Unable to connect to DB server" ));
		gdk_rgba_parse( &color, priv->ok ? "#000000" : "#ff0000" );
		gtk_widget_override_color(
				GTK_WIDGET( priv->msg_label ), GTK_STATE_FLAG_NORMAL, &color );
	}
}
