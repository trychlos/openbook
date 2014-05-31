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
#include "ui/ofa-class-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-class.h"
#include "ui/ofo-dossier.h"

/* private instance data
 */
struct _ofaClassPropertiesPrivate {

	/* internals
	 */
	ofoClass     *class;
	gboolean      is_new;
	gboolean      updated;

	/* data
	 */
	gint          number;
	gchar        *label;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-class-properties.ui";
static const gchar  *st_ui_id  = "ClassPropertiesDlg";

G_DEFINE_TYPE( ofaClassProperties, ofa_class_properties, OFA_TYPE_BASE_DIALOG )

static void      v_init_dialog( ofaBaseDialog *dialog );
static void      on_label_changed( GtkEntry *entry, ofaClassProperties *self );
static void      check_for_enable_dlg( ofaClassProperties *self );
static gboolean  is_dialog_validable( ofaClassProperties *self );
static gboolean  v_quit_on_ok( ofaBaseDialog *dialog );
static gboolean  do_update( ofaClassProperties *self );

static void
class_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_class_properties_finalize";
	ofaClassPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_CLASS_PROPERTIES( instance ));

	priv = OFA_CLASS_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free members here */
	g_free( priv->label );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_properties_parent_class )->finalize( instance );
}

static void
class_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_CLASS_PROPERTIES( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_properties_parent_class )->dispose( instance );
}

static void
ofa_class_properties_init( ofaClassProperties *self )
{
	static const gchar *thisfn = "ofa_class_properties_init";

	g_return_if_fail( OFA_IS_CLASS_PROPERTIES( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaClassPropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_class_properties_class_init( ofaClassPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_class_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_properties_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_class_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an class
 */
gboolean
ofa_class_properties_run( ofaMainWindow *main_window, ofoClass *class )
{
	static const gchar *thisfn = "ofa_class_properties_run";
	ofaClassProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, class=%p",
			thisfn, ( void * ) main_window, ( void * ) class );

	self = g_object_new(
				OFA_TYPE_CLASS_PROPERTIES,
				OFA_PROP_MAIN_WINDOW, main_window,
				OFA_PROP_DIALOG_XML,  st_ui_xml,
				OFA_PROP_DIALOG_NAME, st_ui_id,
				NULL );

	self->private->class = class;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( ofaBaseDialog *dialog )
{
	ofaClassPropertiesPrivate *priv;
	gchar *title;
	gint number;
	GtkEntry *entry;
	gchar *str;

	priv = OFA_CLASS_PROPERTIES( dialog )->private;

	number = ofo_class_get_number( priv->class );
	if( number < 1 ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new class" ));
	} else {
		title = g_strdup_printf( _( "Updating class « %d »" ), number );
	}
	gtk_window_set_title( GTK_WINDOW( dialog->prot->dialog ), title );

	priv->number = number;
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-number" ));
	str = g_strdup_printf( "%d", priv->number );
	gtk_entry_set_text( entry, str );
	g_free( str );

	priv->label = g_strdup( ofo_class_get_label( priv->class ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	my_utils_init_notes_ex( dialog->prot->dialog, class );
	my_utils_init_maj_user_stamp_ex( dialog->prot->dialog, class );

	check_for_enable_dlg( OFA_CLASS_PROPERTIES( dialog ));
}

static void
on_label_changed( GtkEntry *entry, ofaClassProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaClassProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaClassProperties *self )
{
	ofaClassPropertiesPrivate *priv;
	gboolean ok;
	ofoClass *exists;

	priv = self->private;

	ok = ofo_class_is_valid( priv->number, priv->label );
	if( ok ){
		exists = ofo_class_get_by_number(
				ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self )), priv->number );
		ok &= !exists ||
				( ofo_class_get_number( exists ) == ofo_class_get_number( priv->class ));
	}

	return( ok );
}

static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_CLASS_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaClassProperties *self )
{
	ofaClassPropertiesPrivate *priv;
	ofoDossier *dossier;

	priv = self->private;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );
	g_return_val_if_fail( !priv->is_new, FALSE );

	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));

	ofo_class_set_label( priv->class, priv->label );
	my_utils_getback_notes_ex( OFA_BASE_DIALOG( self )->prot->dialog, class );

	priv->updated = ofo_class_update( priv->class, dossier );

	return( priv->updated );
}
