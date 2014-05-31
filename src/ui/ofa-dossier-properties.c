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
#include <stdlib.h>

#include "ui/my-utils.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-dossier.h"

/* private instance data
 */
struct _ofaDossierPropertiesPrivate {

	/* internals
	 */
	ofoDossier    *dossier;
	gboolean       is_new;
	gboolean       updated;

	/* data
	 */
	gchar         *label;
	gint           duree;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-dossier-properties.ui";
static const gchar  *st_ui_id  = "DossierPropertiesDlg";

G_DEFINE_TYPE( ofaDossierProperties, ofa_dossier_properties, OFA_TYPE_BASE_DIALOG )

static void      v_init_dialog( ofaBaseDialog *dialog );
static void      on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      check_for_enable_dlg( ofaDossierProperties *self );
static gboolean  v_quit_on_ok( ofaBaseDialog *dialog );
static gboolean  do_update( ofaDossierProperties *self );

static void
dossier_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_finalize";
	ofaDossierPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_DOSSIER_PROPERTIES( instance ));

	priv = OFA_DOSSIER_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free members here */
	g_free( priv->label );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->finalize( instance );
}

static void
dossier_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_DOSSIER_PROPERTIES( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->dispose( instance );
}

static void
ofa_dossier_properties_init( ofaDossierProperties *self )
{
	static const gchar *thisfn = "ofa_dossier_properties_init";

	g_return_if_fail( OFA_IS_DOSSIER_PROPERTIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaDossierPropertiesPrivate, 1 );

	self->private->updated = FALSE;
}

static void
ofa_dossier_properties_class_init( ofaDossierPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_properties_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_dossier_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an dossier
 */
gboolean
ofa_dossier_properties_run( ofaMainWindow *main_window, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofa_dossier_properties_run";
	ofaDossierProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, dossier=%p",
			thisfn, ( void * ) main_window, ( void * ) dossier );

	self = g_object_new(
				OFA_TYPE_DOSSIER_PROPERTIES,
				OFA_PROP_MAIN_WINDOW, main_window,
				OFA_PROP_DIALOG_XML,  st_ui_xml,
				OFA_PROP_DIALOG_NAME, st_ui_id,
				NULL );

	self->private->dossier = dossier;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( ofaBaseDialog *dialog )
{
	ofaDossierPropertiesPrivate *priv;
	GtkEntry *entry;
	gchar *str;

	priv = OFA_DOSSIER_PROPERTIES( dialog )->private;

	priv->label = g_strdup( ofo_dossier_get_label( priv->dossier ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name(
					GTK_CONTAINER( dialog->prot->dialog ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	} else {
		priv->is_new = TRUE;
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	priv->duree = ofo_dossier_get_exercice_length( priv->dossier );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name(
					GTK_CONTAINER( dialog->prot->dialog ), "p1-exe-length" ));
	if( priv->duree > 0 ){
		str = g_strdup_printf( "%d", priv->duree );
	} else {
		str = g_strdup( "" );
	}
	gtk_entry_set_text( entry, str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_duree_changed ), dialog );

	my_utils_init_notes_ex( dialog->prot->dialog, dossier );
	my_utils_init_maj_user_stamp_ex( dialog->prot->dialog, dossier );

	check_for_enable_dlg( OFA_DOSSIER_PROPERTIES( dialog ));
}

static void
on_label_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_duree_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	const gchar *text;

	text = gtk_entry_get_text( entry );
	if( text && g_utf8_strlen( text, -1 )){
		self->private->duree = atoi( text );
	}

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *button;
	gboolean ok;

	priv = self->private;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );

	ok = ofo_dossier_is_valid( priv->label, priv->duree );

	gtk_widget_set_sensitive( button, ok );
}

static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_DOSSIER_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	g_return_val_if_fail(
			ofo_dossier_is_valid( self->private->label, self->private->duree ), FALSE );

	priv = self->private;

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_exercice_length( priv->dossier, priv->duree );
	my_utils_getback_notes_ex( OFA_BASE_DIALOG( self )->prot->dialog, dossier );

	priv->updated = ofo_dossier_update( priv->dossier );

	return( priv->updated );
}
