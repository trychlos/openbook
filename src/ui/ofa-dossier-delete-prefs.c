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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/my-utils.h"
#include "api/ofa-idbms.h"
#include "api/ofa-settings.h"

#include "ui/ofa-dossier-delete-prefs.h"

/* private instance data
 */
struct _ofaDossierDeletePrefsPrivate {
	gboolean        dispose_has_run;

	/* data
	 */
	gint            db_mode;
	gboolean        account_mode;

	/* UI
	 */
	GtkRadioButton *p2_db_drop;
	GtkRadioButton *p2_db_leave;
};

G_DEFINE_TYPE( ofaDossierDeletePrefs, ofa_dossier_delete_prefs, G_TYPE_OBJECT )

static void      on_db_mode_toggled( GtkToggleButton *btn, ofaDossierDeletePrefs *self );
static void      on_account_toggled( GtkToggleButton *btn, ofaDossierDeletePrefs *self );

static void
dossier_delete_prefs_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE_PREFS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_prefs_parent_class )->finalize( instance );
}

static void
dossier_delete_prefs_dispose( GObject *instance )
{
	ofaDossierDeletePrefsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_DELETE_PREFS( instance ));

	priv = OFA_DOSSIER_DELETE_PREFS( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_delete_prefs_parent_class )->dispose( instance );
}

static void
ofa_dossier_delete_prefs_init( ofaDossierDeletePrefs *self )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_DELETE_PREFS( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_DOSSIER_DELETE_PREFS, ofaDossierDeletePrefsPrivate );
}

static void
ofa_dossier_delete_prefs_class_init( ofaDossierDeletePrefsClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_delete_prefs_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_delete_prefs_finalize;

	g_type_class_add_private( klass, sizeof( ofaDossierDeletePrefsPrivate ));
}

/**
 * ofa_dossier_delete_prefs_new:
 */
ofaDossierDeletePrefs *
ofa_dossier_delete_prefs_new( void )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_new";
	ofaDossierDeletePrefs *self;

	g_debug( "%s:", thisfn );

	self = g_object_new( OFA_TYPE_DOSSIER_DELETE_PREFS, NULL );

	return( self );
}

/**
 * ofa_dossier_delete_prefs_init:
 */
void
ofa_dossier_delete_prefs_init_dialog( ofaDossierDeletePrefs *prefs, GtkContainer *container )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_init_dialog";
	ofaDossierDeletePrefsPrivate *priv;
	GtkWidget *radio, *check;
	gint ivalue;
	gboolean bvalue;

	g_debug( "%s: prefs=%p, container=%p (%s)",
			thisfn, ( void * ) prefs, ( void * ) container, G_OBJECT_TYPE_NAME( container ));

	g_return_if_fail( prefs && OFA_IS_DOSSIER_DELETE_PREFS( prefs ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	if( !prefs->priv->dispose_has_run ){

		priv = prefs->priv;

		radio = my_utils_container_get_child_by_name( container, "p2-db-drop" );
		g_return_if_fail( radio && GTK_IS_RADIO_BUTTON( radio ));
		priv->p2_db_drop = GTK_RADIO_BUTTON( radio );
		g_signal_connect( G_OBJECT( radio ), "toggled", G_CALLBACK( on_db_mode_toggled ), prefs );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( radio ), TRUE );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( radio ), FALSE );

		radio = my_utils_container_get_child_by_name( container, "p2-db-leave" );
		g_return_if_fail( radio && GTK_IS_RADIO_BUTTON( radio ));
		priv->p2_db_leave = GTK_RADIO_BUTTON( radio );
		g_signal_connect( G_OBJECT( radio ), "toggled", G_CALLBACK( on_db_mode_toggled ), prefs );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( radio ), TRUE );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( radio ), FALSE );

		ivalue = ofa_settings_get_int( "DossierDeletePrefsDlg-db_mode" );
		if( ivalue < 0 ){
			ivalue = DBMODE_REINIT;
		}
		if( ivalue == DBMODE_REINIT ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_db_drop ), TRUE );
		} else if( ivalue == DBMODE_LEAVE_AS_IS ){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->p2_db_leave ), TRUE );
		}

		check = my_utils_container_get_child_by_name( container, "p3-account-drop" );
		g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
		g_signal_connect( G_OBJECT( check ), "toggled", G_CALLBACK( on_account_toggled ), prefs );

		bvalue = ofa_settings_get_boolean( "DossierDeletePrefsDlg-account_mode" );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), !bvalue );
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), bvalue );

		/* #303: unable to remove administrative accounts */
		gtk_widget_set_sensitive( check, FALSE );
	}
}

static void
on_db_mode_toggled( GtkToggleButton *btn, ofaDossierDeletePrefs *self )
{
	ofaDossierDeletePrefsPrivate *priv;
	gboolean is_active;

	priv = self->priv;
	is_active = gtk_toggle_button_get_active( btn );
	priv->db_mode = 0;

	if( is_active ){
		if( btn == ( GtkToggleButton * ) priv->p2_db_drop ){
			priv->db_mode = DBMODE_REINIT;
		} else if( btn == ( GtkToggleButton * ) priv->p2_db_leave ){
			priv->db_mode = DBMODE_LEAVE_AS_IS;
		}
	}
}

static void
on_account_toggled( GtkToggleButton *btn, ofaDossierDeletePrefs *self )
{
	self->priv->account_mode = gtk_toggle_button_get_active( btn );
}

/**
 * ofa_dossier_delete_prefs_get_db_mode:
 */
gint
ofa_dossier_delete_prefs_get_db_mode( ofaDossierDeletePrefs *prefs )
{
	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_DELETE_PREFS( prefs ), -1 );

	if( !prefs->priv->dispose_has_run ){

		return( prefs->priv->db_mode );
	}

	return( -1 );
}

/**
 * ofa_dossier_delete_prefs_get_account_mode:
 */
gboolean
ofa_dossier_delete_prefs_get_account_mode( ofaDossierDeletePrefs *prefs )
{
	g_return_val_if_fail( prefs && OFA_IS_DOSSIER_DELETE_PREFS( prefs ), FALSE );

	if( !prefs->priv->dispose_has_run ){

		return( prefs->priv->account_mode );
	}

	return( FALSE );
}

/**
 * ofa_dossier_delete_prefs_set_settings:
 */
void
ofa_dossier_delete_prefs_set_settings( ofaDossierDeletePrefs *prefs )
{
	static const gchar *thisfn = "ofa_dossier_delete_prefs_set_settings";
	ofaDossierDeletePrefsPrivate *priv;

	g_debug( "%s: prefs=%p", thisfn, ( void * ) prefs );

	g_return_if_fail( prefs && OFA_IS_DOSSIER_DELETE_PREFS( prefs ));

	priv = prefs->priv;

	if( !priv->dispose_has_run ){

		ofa_settings_set_int( "DossierDeletePrefsDlg-db_mode", priv->db_mode );
		ofa_settings_set_boolean( "DossierDeletePrefsDlg-account_mode", priv->account_mode );
	}
}
