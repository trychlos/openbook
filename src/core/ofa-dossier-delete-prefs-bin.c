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

#include <stdlib.h>

#include "my/my-utils.h"

#include "api/ofa-settings.h"

#include "ofa-dossier-delete-prefs-bin.h"

/* private instance data
 */
typedef struct {
	gboolean   dispose_has_run;

	/* data
	 */
	gint       db_mode;
	gboolean   account_mode;

	/* UI
	 */
	GtkWidget *p2_db_drop;
	GtkWidget *p2_db_keep;
	GtkWidget *p3_account;
}
	ofaDossierDeletePrefsBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-dossier-delete-prefs-bin.ui";

static const gchar *st_delete_prefs     = "DossierDeletePrefs";

static void setup_bin( ofaDossierDeletePrefsBin *bin );
static void on_db_mode_toggled( GtkToggleButton *btn, ofaDossierDeletePrefsBin *bin );
static void on_account_toggled( GtkToggleButton *btn, ofaDossierDeletePrefsBin *bin );
static void changed_composite( ofaDossierDeletePrefsBin *bin );
static void setup_settings( ofaDossierDeletePrefsBin *bin );

G_DEFINE_TYPE_EXTENDED( ofaDossierDeletePrefsBin, ofa_dossier_delete_prefs_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaDossierDeletePrefsBin ))

static void
dossier_delete_prefs_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE_PREFS_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_prefs_bin_parent_class )->finalize( instance );
}

static void
dossier_delete_prefs_bin_dispose( GObject *instance )
{
	ofaDossierDeletePrefsBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE_PREFS_BIN( instance ));

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( OFA_DOSSIER_DELETE_PREFS_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_prefs_bin_parent_class )->dispose( instance );
}

static void
ofa_dossier_delete_prefs_bin_init( ofaDossierDeletePrefsBin *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_bin_init";
	ofaDossierDeletePrefsBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DELETE_PREFS_BIN( self ));

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_dossier_delete_prefs_bin_class_init( ofaDossierDeletePrefsBinClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_delete_prefs_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_delete_prefs_bin_finalize;

	/**
	 * ofaDossierDeletePrefsBin::changed:
	 *
	 * This signal is sent on the #ofaDossierDeletePrefsBin when the
	 * content has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDossierDeletePrefsBin *bin,
	 * 						guint                   db_mode,
	 * 						gboolean                account_mode,
	 * 						gpointer                user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_DOSSIER_DELETE_PREFS_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_UINT, G_TYPE_BOOLEAN );
}

/**
 * ofa_dossier_delete_prefs_bin_new:
 */
ofaDossierDeletePrefsBin *
ofa_dossier_delete_prefs_bin_new( void )
{
	ofaDossierDeletePrefsBin *bin;

	bin = g_object_new( OFA_TYPE_DOSSIER_DELETE_PREFS_BIN, NULL );

	setup_bin( bin );
	setup_settings( bin );

	return( bin );
}

static void
setup_bin( ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *radio, *check, *frame;

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "ddpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	radio = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-db-drop" );
	g_return_if_fail( radio && GTK_IS_RADIO_BUTTON( radio ));
	g_signal_connect( G_OBJECT( radio ), "toggled", G_CALLBACK( on_db_mode_toggled ), bin );
	priv->p2_db_drop = radio;

	radio = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p2-db-leave" );
	g_return_if_fail( radio && GTK_IS_RADIO_BUTTON( radio ));
	g_signal_connect( G_OBJECT( radio ), "toggled", G_CALLBACK( on_db_mode_toggled ), bin );
	priv->p2_db_keep = radio;

	check = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p3-account-drop" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	g_signal_connect( G_OBJECT( check ), "toggled", G_CALLBACK( on_account_toggled ), bin );
	priv->p3_account = check;

	/* #303: unable to remove administrative accounts */
	frame = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "frame-account-drop" );
	g_return_if_fail( frame && GTK_IS_FRAME( frame ));
	gtk_widget_set_sensitive( frame, FALSE );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_db_mode_toggled( GtkToggleButton *btn, ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;
	gboolean is_active;

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	is_active = gtk_toggle_button_get_active( btn );
	priv->db_mode = 0;

	if( is_active ){
		if(( GtkWidget * ) btn == priv->p2_db_drop ){
			priv->db_mode = DBMODE_DROP;

		} else if(( GtkWidget * ) btn == priv->p2_db_keep ){
			priv->db_mode = DBMODE_KEEP;
		}
	}

	changed_composite( bin );
}

static void
on_account_toggled( GtkToggleButton *btn, ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	priv->account_mode = gtk_toggle_button_get_active( btn );

	changed_composite( bin );
}

static void
changed_composite( ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	g_signal_emit_by_name( bin, "ofa-changed", priv->db_mode, priv->account_mode );
}

/**
 * ofa_dossier_delete_prefs_bin_get_db_mode:
 * @bin:
 *
 * Returns: what to do about the database when deleting a dossier.
 * See definition in api/ofa-idbms.h
 */
gint
ofa_dossier_delete_prefs_bin_get_db_mode( ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;
	gint mode;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_DELETE_PREFS_BIN( bin ), -1 );

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, -1 );

	mode = priv->db_mode;

	return( mode );
}

/**
 * ofa_dossier_delete_prefs_bin_set_db_mode:
 * @bin:
 * @mode: what to do about the database when deleting a dossier.
 * See definition in api/ofa-idbms.h
 */
void
ofa_dossier_delete_prefs_bin_set_db_mode( ofaDossierDeletePrefsBin *bin, gint mode )
{
	ofaDossierDeletePrefsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_DELETE_PREFS_BIN( bin ));

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( mode == DBMODE_DROP ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_db_drop ), TRUE );
		on_db_mode_toggled( GTK_TOGGLE_BUTTON( priv->p2_db_drop ), bin );

	} else if( mode == DBMODE_KEEP ){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_db_keep ), TRUE );
		on_db_mode_toggled( GTK_TOGGLE_BUTTON( priv->p2_db_keep ), bin );
	}
}

/**
 * ofa_dossier_delete_prefs_bin_get_account_mode:
 * @bin:
 *
 * Returns: %TRUE if administrative accounts should be remove from the
 * DBMS when deleting a dossier.
 */
gboolean
ofa_dossier_delete_prefs_bin_get_account_mode( ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;
	gboolean drop;

	g_return_val_if_fail( bin && OFA_IS_DOSSIER_DELETE_PREFS_BIN( bin ), FALSE );

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	drop = priv->account_mode;

	return( drop );
}

/**
 * ofa_dossier_delete_prefs_bin_set_account_mode:
 * @bin:
 * @drop: %TRUE if dossier administrative credentials should be dropped
 *  from the DBMS when deleting the dossier.
 */
void
ofa_dossier_delete_prefs_bin_set_account_mode( ofaDossierDeletePrefsBin *bin, gboolean drop )
{
	ofaDossierDeletePrefsBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DOSSIER_DELETE_PREFS_BIN( bin ));

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p3_account ), drop );
	on_account_toggled( GTK_TOGGLE_BUTTON( priv->p3_account ), bin );
}

/*
 * settings: dbmode;drop_account;
 */
static void
setup_settings( ofaDossierDeletePrefsBin *bin )
{
	GList *strlist, *it;
	const gchar *cstr;
	gint dbmode;
	gboolean drop_account;

	strlist = ofa_settings_user_get_string_list( st_delete_prefs );
	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		dbmode = atoi( cstr );
		ofa_dossier_delete_prefs_bin_set_db_mode( bin, dbmode );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		drop_account = my_utils_boolean_from_str( cstr );
		ofa_dossier_delete_prefs_bin_set_account_mode( bin, drop_account );
	}
}

/**
 * ofa_dossier_delete_prefs_bin_set_settings:
 * @bin:
 */
void
ofa_dossier_delete_prefs_bin_set_settings( ofaDossierDeletePrefsBin *bin )
{
	ofaDossierDeletePrefsBinPrivate *priv;
	gchar *str;

	g_return_if_fail( bin && OFA_IS_DOSSIER_DELETE_PREFS_BIN( bin ));

	priv = ofa_dossier_delete_prefs_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	str = g_strdup_printf( "%d;%s;", priv->db_mode, priv->account_mode ? "True":"False" );
	ofa_settings_user_set_string( st_delete_prefs, str );
	g_free( str );
}
