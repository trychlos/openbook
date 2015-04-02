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
#include "api/ofa-dbms.h"

#include "core/ofa-dbms-root-bin.h"

/* private instance data
 */
struct _ofaDBMSRootBinPrivate {
	gboolean      dispose_has_run;

	/* initialization
	 */
	gchar        *dname;

	/* UI
	 */
	GtkSizeGroup *group;
	GtkWidget    *account_entry;
	GtkWidget    *password_entry;
	GtkWidget    *msg_label;

	/* runtime data
	 */
	gchar        *account;
	gchar        *password;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-dbms-root-bin.ui";
static const gchar *st_bin_id           = "DBMSRootBin";

G_DEFINE_TYPE( ofaDBMSRootBin, ofa_dbms_root_bin, GTK_TYPE_BIN )

static void     setup_composite( ofaDBMSRootBin *bin );
static void     on_account_changed( GtkEditable *entry, ofaDBMSRootBin *self );
static void     on_password_changed( GtkEditable *entry, ofaDBMSRootBin *self );
static void     changed_composite( ofaDBMSRootBin *self );
static gboolean is_valid_composite( const ofaDBMSRootBin *bin );

static void
dbms_root_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_finalize";
	ofaDBMSRootBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DBMS_ROOT_BIN( instance ));

	/* free data members here */
	priv = OFA_DBMS_ROOT_BIN( instance )->priv;

	g_free( priv->dname );
	g_free( priv->account );
	g_free( priv->password );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_root_bin_parent_class )->finalize( instance );
}

static void
dbms_root_bin_dispose( GObject *instance )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DBMS_ROOT_BIN( instance ));

	priv = OFA_DBMS_ROOT_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dbms_root_bin_parent_class )->dispose( instance );
}

static void
ofa_dbms_root_bin_init( ofaDBMSRootBin *self )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DBMS_ROOT_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_DBMS_ROOT_BIN, ofaDBMSRootBinPrivate );
}

static void
ofa_dbms_root_bin_class_init( ofaDBMSRootBinClass *klass )
{
	static const gchar *thisfn = "ofa_dbms_root_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dbms_root_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dbms_root_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaDBMSRootBinPrivate ));

	/**
	 * ofaDBMSRootBin::changed:
	 *
	 * This signal is sent on the #ofaDBMSRootBin when the account or
	 * the password are changed.
	 *
	 * Arguments are new account and password.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDBMSRootBin *bin,
	 * 						const gchar    *account,
	 * 						const gchar    *password,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DBMS_ROOT_BIN,
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
 * ofa_dbms_root_bin_new:
 */
ofaDBMSRootBin *
ofa_dbms_root_bin_new( void )
{
	ofaDBMSRootBin *bin;

	bin = g_object_new( OFA_TYPE_DBMS_ROOT_BIN, NULL );

	setup_composite( bin );

	return( bin );
}

static void
setup_composite( ofaDBMSRootBin *bin )
{
	ofaDBMSRootBinPrivate *priv;
	GtkWidget *window, *top_widget, *label;

	priv =bin->priv;

	window = my_utils_builder_load_from_path( st_bin_xml, st_bin_id );
	g_return_if_fail( window && GTK_IS_CONTAINER( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));
	gtk_widget_destroy( window );

	priv->account_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dra-account" );
	g_return_if_fail( priv->account_entry && GTK_IS_ENTRY( priv->account_entry ));
	g_signal_connect(
			G_OBJECT( priv->account_entry ), "changed", G_CALLBACK( on_account_changed ), bin );

	priv->password_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dra-password" );
	g_return_if_fail( priv->password_entry && GTK_IS_ENTRY( priv->password_entry ));
	g_signal_connect(
			G_OBJECT( priv->password_entry ), "changed", G_CALLBACK( on_password_changed ), bin );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dra-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->msg_label = label;
}

/**
 * ofa_dbms_root_bin_get_label:
 * @bin:
 *
 * Returns: a label of the column 0
 * (use case: let the container build a #GtkSizeGroup).
 */
GtkWidget *
ofa_dbms_root_bin_get_label( const ofaDBMSRootBin *bin )
{
	ofaDBMSRootBinPrivate *priv;
	GtkWidget *label;

	g_return_val_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ), NULL );

	priv = bin->priv;
	label = NULL;

	if( !priv->dispose_has_run ){

		label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "dra-label2" );
		g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	}

	return( label );
}

/**
 * ofa_dbms_root_bin_set_dossier:
 * @bin:
 * @dname: the name of the dossier.
 *
 * When set, this let the composite widget validate the account and the
 * password against the actual DBMS which manages this dossier.
 * Else, we only check if account and password are set.
 */
void
ofa_dbms_root_bin_set_dossier( ofaDBMSRootBin *bin, const gchar *dname )
{
	ofaDBMSRootBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DBMS_ROOT_BIN( bin ));
	g_return_if_fail( my_strlen( dname ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		g_free( priv->dname );
		priv->dname = g_strdup( dname );
	}
}

static void
on_account_changed( GtkEditable *entry, ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;

	priv = self->priv;

	g_free( priv->account );
	priv->account = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

static void
on_password_changed( GtkEditable *entry, ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;

	priv = self->priv;

	g_free( priv->password );
	priv->password = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry )));

	changed_composite( self );
}

/*
 * test the DBMS root connection by trying to connect with empty dbname
 */
static void
changed_composite( ofaDBMSRootBin *self )
{
	ofaDBMSRootBinPrivate *priv;

	priv = self->priv;

	g_signal_emit_by_name( self, "ofa-changed", priv->account, priv->password );
}

/**
 * ofa_dbms_root_bin_is_valid:
 * @bin:
 * @error_message: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if the composite widget is valid: both account and
 * password are set. If dossier is set and is defined in the settings,
 * then check that account and password let have a successful connection
 * to the DBMS.
 */
gboolean
ofa_dbms_root_bin_is_valid( const ofaDBMSRootBin *bin, gchar **error_message )
{
	ofaDBMSRootBinPrivate *priv;
	gboolean ok;

	priv = bin->priv;
	ok = FALSE;

	if( !priv->dispose_has_run ){

		gtk_label_set_text( GTK_LABEL( priv->msg_label ), "" );
		ok = is_valid_composite( bin );

		if( error_message ){
			*error_message = ok ?
					NULL :
					g_strdup( _( "Unable to connect to DB server" ));
		}

		ofa_dbms_root_bin_set_valid( bin, ok );
	}

	return( ok );
}

static gboolean
is_valid_composite( const ofaDBMSRootBin *bin )
{
	ofaDBMSRootBinPrivate *priv;
	ofaDbms *dbms;
	gboolean ok;

	priv = bin->priv;
	ok = FALSE;

	if( my_strlen( priv->dname ) && my_strlen( priv->account )){

		if( my_strlen( priv->dname )){
			dbms = ofa_dbms_new();
			ok = ofa_dbms_connect(
							dbms, priv->dname, NULL, priv->account, priv->password, FALSE );
			g_object_unref( dbms );

		} else {
			ok = TRUE;
		}
	}

	return( ok );
}

/**
 * ofa_dbms_root_bin_set_valid:
 * @bin:
 * @valid:
 *
 * This let us turn the 'connection ok' message on, which is useful
 * when checking for a connection which is not yet referenced in the
 * settings.
 */
void
ofa_dbms_root_bin_set_valid( const ofaDBMSRootBin *bin, gboolean valid )
{
	ofaDBMSRootBinPrivate *priv;

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		gtk_label_set_text(
				GTK_LABEL( priv->msg_label ), valid ? _( "DB server connection is OK" ) : "" );
	}
}

/**
 * ofa_dbms_root_bin_set_credentials:
 * @bin:
 * @account:
 * @password:
 */
void
ofa_dbms_root_bin_set_credentials( ofaDBMSRootBin *bin, const gchar *account, const gchar *password )
{
	ofaDBMSRootBinPrivate *priv;

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), account );
		gtk_entry_set_text( GTK_ENTRY( priv->password_entry ), password );
	}
}
