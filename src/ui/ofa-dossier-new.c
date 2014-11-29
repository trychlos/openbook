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
#include <glib/gprintf.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-sgbd.h"

#include "core/my-window-prot.h"

#include "ui/ofa-dossier-delete.h"
#include "ui/ofa-dossier-new.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierNewPrivate {

	/* p1: SGDB Provider
	 */
	gchar         *p1_sgdb_provider;
	ofaIDbms      *p1_module;
	GtkContainer  *p1_parent;

	/* p3: dossier properties
	 */
	gchar         *p3_label;
	gchar         *p3_account;
	gchar         *p3_password;
	gchar         *p3_bis;
	gboolean       p3_open_dossier;
	gboolean       p3_update_properties;

	gboolean       p3_label_is_ok;
	gboolean       p3_account_is_ok;
	gboolean       p3_passwd_are_equals;

	/* UI
	 */
	GtkLabel      *p1_message;
	GtkWidget     *p3_browse_btn;
	GtkWidget     *p3_properties_btn;
	GtkLabel      *msg_label;
	GtkWidget     *apply_btn;

	/* result
	 */
	gboolean       dossier_created;
};

/* columns in SGDB provider combo box
 */
enum {
	SGDB_COL_PROVIDER = 0,
	SGDB_N_COLUMNS
};

static const gchar *st_ui_xml   = PKGUIDIR "/ofa-dossier-new.ui";
static const gchar *st_ui_id    = "DossierNewDlg";

G_DEFINE_TYPE( ofaDossierNew, ofa_dossier_new, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_p1_sgdb_provider( ofaDossierNew *self );
static void      on_sgdb_provider_changed( GtkComboBox *combo, ofaDossierNew *self );
static void      init_p3_dossier_properties( ofaDossierNew *self );
static void      on_db_label_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_db_account_changed( GtkEntry *entry, ofaDossierNew *self );
static gboolean  is_account_ok( ofaDossierNew *self );
static void      on_db_password_changed( GtkEntry *entry, ofaDossierNew *self );
static void      on_db_bis_changed( GtkEntry *entry, ofaDossierNew *self );
static gboolean  db_passwords_are_equals( ofaDossierNew *self );
static void      on_db_open_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      on_db_properties_toggled( GtkToggleButton *button, ofaDossierNew *self );
static void      set_message( ofaDossierNew *self, const gchar *msg );
static void      check_for_enable_dlg( ofaDossierNew *self );
static gboolean  v_quit_on_ok( myDialog *dialog );

static void
dossier_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_new_finalize";
	ofaDossierNewPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	/* free data members here */
	priv = OFA_DOSSIER_NEW( instance )->priv;
	g_free( priv->p1_sgdb_provider );
	g_free( priv->p3_label );
	g_free( priv->p3_account );
	g_free( priv->p3_password );
	g_free( priv->p3_bis );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->finalize( instance );
}

static void
dossier_new_dispose( GObject *instance )
{
	ofaDossierNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_NEW( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		priv = OFA_DOSSIER_NEW( instance )->priv;

		/* unref object members here */
		if( priv->p1_module ){
			g_clear_object( &priv->p1_module );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_new_parent_class )->dispose( instance );
}

static void
ofa_dossier_new_init( ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_NEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_NEW, ofaDossierNewPrivate );
	self->priv->dossier_created = FALSE;
}

static void
ofa_dossier_new_class_init( ofaDossierNewClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_new_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierNewPrivate ));
}

/**
 * ofa_dossier_new_run:
 * @main: the main window of the application.
 *
 * Returns: %TRUE if a dossier has been created, and opened in the UI.
 */
gboolean
ofa_dossier_new_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_dossier_new_run";
	ofaDossierNew *self;
	gboolean dossier_created, open_dossier, open_properties;
	ofsDossierOpen *sdo;
	gboolean dossier_opened;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, main_window );

	self = g_object_new( OFA_TYPE_DOSSIER_NEW,
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	dossier_opened = FALSE;
	dossier_created = self->priv->dossier_created;
	open_dossier = self->priv->p3_open_dossier;
	open_properties = self->priv->p3_update_properties;

	if( dossier_created ){
		if( open_dossier ){
			sdo = g_new0( ofsDossierOpen, 1 );
			sdo->dname = g_strdup( self->priv->p3_label );
			sdo->dbname = NULL;
			sdo->account = g_strdup( self->priv->p3_account );
			sdo->password = g_strdup( self->priv->p3_password );
		}
	}

	g_object_unref( self );

	if( dossier_created ){
		if( open_dossier ){
			g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_ACTION_DOSSIER_OPEN, sdo );
			if( open_properties ){
				g_signal_emit_by_name( G_OBJECT( main_window ), OFA_SIGNAL_ACTION_DOSSIER_PROPERTIES );
			}
			dossier_opened = TRUE;
		}
	}

	return( dossier_opened );
}

/*
 * the provided 'page' is the toplevel widget of the asistant's page
 */
static void
v_init_dialog( myDialog *dialog )
{
	init_p3_dossier_properties( OFA_DOSSIER_NEW( dialog ));
	init_p1_sgdb_provider( OFA_DOSSIER_NEW( dialog ));
	check_for_enable_dlg( OFA_DOSSIER_NEW( dialog ));
}

static void
init_p1_sgdb_provider( ofaDossierNew *self )
{
	GtkContainer *container;
	GtkComboBox *combo;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeIter iter;
	GSList *prov_list, *ip;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	combo = ( GtkComboBox * ) my_utils_container_get_child_by_name( container, "p1-provider" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			SGDB_N_COLUMNS,
			G_TYPE_STRING ));
	gtk_combo_box_set_model( combo, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", SGDB_COL_PROVIDER );

	prov_list = ofa_idbms_get_providers_list();

	for( ip=prov_list ; ip ; ip=ip->next ){
		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				SGDB_COL_PROVIDER, ip->data,
				-1 );
	}

	ofa_idbms_free_providers_list( prov_list );

	g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_sgdb_provider_changed ), self );

	gtk_combo_box_set_active( combo, 0 );
}

static void
on_sgdb_provider_changed( GtkComboBox *combo, ofaDossierNew *self )
{
	static const gchar *thisfn = "ofa_dossier_new_on_sgdb_provider_changed";
	ofaDossierNewPrivate *priv;
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	GtkContainer *container;
	GtkWidget *parent, *child;
	gchar *str;
	GtkSizeGroup *group;
	GtkWidget *label;

	g_debug( "%s: combo=%p, self=%p", thisfn, ( void * ) combo, ( void * ) self );

	priv = self->priv;

	/* remove previous provider if any */
	g_free( priv->p1_sgdb_provider );
	priv->p1_sgdb_provider = NULL;
	if( priv->p1_module ){
		g_clear_object( &priv->p1_module );
	}
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));
	parent = my_utils_container_get_child_by_name( container, "sgdb-container" );
	g_return_if_fail( parent && GTK_IS_BIN( parent ));

	priv->p1_parent = GTK_CONTAINER( parent );

	child = gtk_bin_get_child( GTK_BIN( parent ));
	if( child ){
		gtk_container_remove( priv->p1_parent, child );
	}

	/* setup current provider */
	if( gtk_combo_box_get_active_iter( combo, &iter )){
		tmodel = gtk_combo_box_get_model( combo );
		gtk_tree_model_get( tmodel, &iter,
				SGDB_COL_PROVIDER, &priv->p1_sgdb_provider,
				-1 );

		priv->p1_module = ofa_idbms_get_provider_by_name( priv->p1_sgdb_provider );

		if( priv->p1_module ){
			/* have a size group */
			group = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
			label = my_utils_container_get_child_by_name( container, "p1-provider-label" );
			g_return_if_fail( label && GTK_IS_LABEL( label ));
			gtk_size_group_add_widget( group, label );
			g_object_unref( group );

			/* and let the DBMS initialize its own part */
			ofa_idbms_properties_new_init( priv->p1_module, priv->p1_parent, group );

		} else {
			str = g_strdup_printf( _( "Unable to handle %s DBMS provider" ), priv->p1_sgdb_provider );
			set_message( self, str );
			g_free( str );
		}
	}
}

static void
init_p3_dossier_properties( ofaDossierNew *self )
{
	GtkContainer *container;
	GtkWidget *widget;
	gboolean value;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	widget = my_utils_container_get_child_by_name( container, "p3-label" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_label_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-account" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_account_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-password" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_password_changed ), self );

	widget = my_utils_container_get_child_by_name( container, "p3-bis" );
	g_return_if_fail( widget && GTK_IS_ENTRY( widget ));
	g_signal_connect( G_OBJECT( widget ), "changed", G_CALLBACK( on_db_bis_changed ), self );

	/* before p3-open so that the later may update the former */
	widget = my_utils_container_get_child_by_name( container, "p3-properties" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_db_properties_toggled ), self );
	value = ofa_settings_get_boolean( "DossierNewDlg-properties" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), value );
	self->priv->p3_properties_btn = widget;

	widget = my_utils_container_get_child_by_name( container, "p3-open" );
	g_return_if_fail( widget && GTK_IS_CHECK_BUTTON( widget ));
	g_signal_connect( G_OBJECT( widget ), "toggled", G_CALLBACK( on_db_open_toggled ), self );
	/* force a signal to be triggered */
	value = ofa_settings_get_boolean( "DossierNewDlg-opendossier" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), !value );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( widget ), value );
}

static void
on_db_label_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *label;

	priv = self->priv;

	label = gtk_entry_get_text( entry );
	g_free( priv->p3_label );
	priv->p3_label = g_strdup( label );

	priv->p3_label_is_ok = ( priv->p3_label && g_utf8_strlen( priv->p3_label, -1 ));
	check_for_enable_dlg( self );
}

static void
on_db_account_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *account;

	priv = self->priv;

	account = gtk_entry_get_text( entry );
	g_free( priv->p3_account );
	priv->p3_account = g_strdup( account );

	priv->p3_account_is_ok = is_account_ok( self );
	check_for_enable_dlg( self );
}

/*
 * the dossier administrative account must be distinct of the DBserver
 * admin account (root)
 */
static gboolean
is_account_ok( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = FALSE;

	if( !priv->p3_account || !g_utf8_strlen( priv->p3_account, -1 )){
		set_message( self, _( "Dossier administrative account is not set" ));

	} else if( g_utf8_collate( priv->p3_account, "root" ) == 0 ){
		set_message( self, _( "Dossier administrative account must be distinct from DB server admin account" ));

	} else {
		ok = TRUE;
		set_message( self, NULL );
	}

	return( ok );
}

static void
on_db_password_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *password;

	priv = self->priv;

	password = gtk_entry_get_text( entry );
	g_free( priv->p3_password );
	priv->p3_password = g_strdup( password );

	priv->p3_passwd_are_equals = db_passwords_are_equals( self );
	check_for_enable_dlg( self );
}

static void
on_db_bis_changed( GtkEntry *entry, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	const gchar *bis;

	priv = self->priv;

	bis = gtk_entry_get_text( entry );
	g_free( priv->p3_bis );
	priv->p3_bis = g_strdup( bis );

	priv->p3_passwd_are_equals = db_passwords_are_equals( self );
	check_for_enable_dlg( self );
}

static gboolean
db_passwords_are_equals( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean are_equals;

	priv = self->priv;

	are_equals = (( !priv->p3_password && !priv->p3_bis ) ||
				( priv->p3_password && g_utf8_strlen( priv->p3_password, -1 ) &&
					priv->p3_bis && g_utf8_strlen( priv->p3_bis, -1 ) &&
					!g_utf8_collate( priv->p3_password, priv->p3_bis )));

	if( are_equals ){
		set_message( self, "" );
	} else {
		set_message( self, _( "Dossier administrative passwords are not set or not equal" ));
	}

	return( are_equals );
}

static void
on_db_open_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;

	priv = self->priv;

	priv->p3_open_dossier = gtk_toggle_button_get_active( button );

	g_return_if_fail( priv->p3_properties_btn && GTK_IS_CHECK_BUTTON( priv->p3_properties_btn ));
	gtk_widget_set_sensitive( priv->p3_properties_btn, priv->p3_open_dossier );
}

static void
on_db_properties_toggled( GtkToggleButton *button, ofaDossierNew *self )
{
	self->priv->p3_update_properties = gtk_toggle_button_get_active( button );
}

static void
set_message( ofaDossierNew *self, const gchar *msg )
{
	ofaDossierNewPrivate *priv;
	GdkRGBA color;

	priv = self->priv;

	if( !priv->msg_label ){
		priv->msg_label = ( GtkLabel * )
								my_utils_container_get_child_by_name(
										GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
										"px-msg" );
	}

	g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));

	if( msg && g_utf8_strlen( msg, -1 )){
		gtk_label_set_text( priv->msg_label, msg );
		if( gdk_rgba_parse( &color, "#FF0000" )){
			gtk_widget_override_color( GTK_WIDGET( priv->msg_label ), GTK_STATE_FLAG_NORMAL, &color );
		}
	} else {
		gtk_label_set_text( priv->msg_label, "" );
	}
}

/*
 * enable the various dynamic buttons
 *
 * as each check may send an error message which will supersede the
 * previously set, we start from the end so that the user will first
 * see the message at the top of the stack (from the first field in
 * the focus chain)
 */
static void
check_for_enable_dlg( ofaDossierNew *self )
{
	ofaDossierNewPrivate *priv;
	gboolean enabled;

	priv = self->priv;

	/* #288: enable dlg should not depend of connection check */
	enabled = priv->p3_label_is_ok &&
				priv->p3_account_is_ok &&
				priv->p3_password &&
				priv->p3_bis &&
				priv->p3_passwd_are_equals &&
				ofa_idbms_properties_new_check( priv->p1_module, priv->p1_parent );

	if( !priv->apply_btn ){
		priv->apply_btn = my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
									"btn-ok" );
	}

	g_return_if_fail( priv->apply_btn && GTK_IS_BUTTON( priv->apply_btn ));
	gtk_widget_set_sensitive( priv->apply_btn, enabled );

	if( enabled ){
		set_message( self, NULL );
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	ofaDossierNewPrivate *priv;
	gboolean ok;

	priv = OFA_DOSSIER_NEW( dialog )->priv;

	ok = ofa_idbms_properties_new_apply(
			priv->p1_module, priv->p1_parent,
			priv->p3_label, priv->p3_account, priv->p3_password );

	if( ok ){
		ofa_settings_set_boolean( "DossierNewDlg-opendossier", priv->p3_open_dossier );
		ofa_settings_set_boolean( "DossierNewDlg-properties", priv->p3_update_properties );
	}

	priv->dossier_created = ok;

	return( ok );
}
