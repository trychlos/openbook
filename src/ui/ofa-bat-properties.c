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

#include "ui/my-utils.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-bat-list.h"
#include "ui/ofa-bat-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-bat.h"
#include "ui/ofo-dossier.h"

/* private instance data
 */
struct _ofaBatPropertiesPrivate {

	/* internals
	 */
	ofoBat        *bat;
	gboolean       is_new;				/* always FALSE here */
	gboolean       updated;
	ofaBatList    *child_box;

	/* data
	 */
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-bat-properties.ui";
static const gchar  *st_ui_id        = "BatPropertiesDlg";

G_DEFINE_TYPE( ofaBatProperties, ofa_bat_properties, OFA_TYPE_BASE_DIALOG )

static void      v_init_dialog( ofaBaseDialog *dialog );
static void      check_for_enable_dlg( ofaBatProperties *self );
static gboolean  is_dialog_validable( ofaBatProperties *self );
static gboolean  v_quit_on_ok( ofaBaseDialog *dialog );
static gboolean  do_update( ofaBatProperties *self );

static void
bat_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_properties_finalize";
	ofaBatPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_BAT_PROPERTIES( instance ));

	priv = OFA_BAT_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_parent_class )->finalize( instance );
}

static void
bat_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_BAT_PROPERTIES( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_parent_class )->dispose( instance );
}

static void
ofa_bat_properties_init( ofaBatProperties *self )
{
	static const gchar *thisfn = "ofa_bat_properties_init";

	g_return_if_fail( OFA_IS_BAT_PROPERTIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaBatPropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_bat_properties_class_init( ofaBatPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_bat_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_properties_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_bat_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an bat
 */
gboolean
ofa_bat_properties_run( ofaMainWindow *main_window, ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_properties_run";
	ofaBatProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, bat=%p",
			thisfn, ( void * ) main_window, ( void * ) bat );

	self = g_object_new(
				OFA_TYPE_BAT_PROPERTIES,
				OFA_PROP_MAIN_WINDOW, main_window,
				OFA_PROP_DIALOG_XML,  st_ui_xml,
				OFA_PROP_DIALOG_NAME, st_ui_id,
				NULL );

	self->private->bat = bat;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( ofaBaseDialog *dialog )
{
	ofaBatProperties *self;
	ofaBatPropertiesPrivate *priv;
	gchar *title;
	ofaBatListParms parms;
	GtkWidget *container;

	title = g_strdup( _( "Updating the BAT properties" ));
	gtk_window_set_title( GTK_WINDOW( dialog->prot->dialog ), title );

	self = OFA_BAT_PROPERTIES( dialog );
	priv = self->private;

	container = my_utils_container_get_child_by_name(
								GTK_CONTAINER( dialog->prot->dialog ), "containing-frame" );
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	parms.container = GTK_CONTAINER( container );
	parms.dossier = ofa_base_dialog_get_dossier( dialog );
	parms.with_tree_view = FALSE;
	parms.editable = TRUE;
	parms.pfnSelection = NULL;
	parms.pfnActivation = NULL;
	parms.user_data = NULL;

	priv->child_box = ofa_bat_list_init_dialog( &parms );
	ofa_bat_list_set_bat( priv->child_box, priv->bat );

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaBatProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaBatProperties *self )
{

	return( TRUE );
}

static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_BAT_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaBatProperties *self )
{
	ofaBatPropertiesPrivate *priv;
	ofoDossier *dossier;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );
	g_return_val_if_fail( !self->private->is_new, FALSE );

	priv = self->private;
	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));

	my_utils_getback_notes_ex( OFA_BASE_DIALOG( self )->prot->dialog, bat );

	priv->updated = ofo_bat_update( priv->bat, dossier );

	return( priv->updated );
}
