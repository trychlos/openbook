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

#include "core/my-utils.h"
#include "ui/ofa-base-dialog.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-main-window.h"
#include "api/ofo-dossier.h"

#if 0
static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;
#endif

/* private instance data
 */
struct _ofaBaseDialogPrivate {

	/* properties
	 */
	ofaMainWindow *main_window;
	gchar         *dialog_xml;
	gchar         *dialog_name;

	/* internals
	 */
	gboolean       init_has_run;
};

/* class properties
 */
enum {
	OFA_PROP_MAIN_WINDOW_ID = 1,
	OFA_PROP_DIALOG_XML_ID,
	OFA_PROP_DIALOG_NAME_ID
};

G_DEFINE_TYPE( ofaBaseDialog, ofa_base_dialog, G_TYPE_OBJECT )

static gboolean load_from_builder( ofaBaseDialog *dialog );
static void     do_init_dialog( ofaBaseDialog *dialog );
static gint     do_run_dialog( ofaBaseDialog *dialog );
static gint     v_run_dialog( ofaBaseDialog *dialog );
static gboolean ok_to_terminate( ofaBaseDialog *self, gint code );
static gboolean do_quit_on_delete_event( ofaBaseDialog *dialog );
static gboolean v_quit_on_delete_event( ofaBaseDialog *dialog );
static gboolean do_quit_on_cancel( ofaBaseDialog *dialog );
static gboolean v_quit_on_cancel( ofaBaseDialog *dialog );
static gboolean do_quit_on_close( ofaBaseDialog *dialog );
static gboolean v_quit_on_close( ofaBaseDialog *dialog );
static gboolean do_quit_on_ok( ofaBaseDialog *dialog );
static gboolean v_quit_on_ok( ofaBaseDialog *dialog );

static void
base_dialog_finalize( GObject *instance )
{
	ofaBaseDialog *self;

	self = OFA_BASE_DIALOG( instance );

	/* free data members here */
	g_free( self->priv->dialog_xml );
	g_free( self->priv->dialog_name );
	g_free( self->priv );
	g_free( self->prot );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_base_dialog_parent_class )->finalize( instance );
}

static void
base_dialog_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_BASE_DIALOG( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		OFA_BASE_DIALOG( instance )->prot->dispose_has_run = TRUE;

		/* unref member objects here */

		gtk_widget_destroy( GTK_WIDGET( OFA_BASE_DIALOG( instance )->prot->dialog ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_base_dialog_parent_class )->dispose( instance );
}

static void
base_dialog_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaBaseDialogPrivate *priv;

	g_return_if_fail( OFA_IS_BASE_DIALOG( object ));
	priv = OFA_BASE_DIALOG( object )->priv;

	if( !OFA_BASE_DIALOG( object )->prot->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_MAIN_WINDOW_ID:
				g_value_set_pointer( value, priv->main_window );
				break;

			case OFA_PROP_DIALOG_XML_ID:
				g_value_set_string( value, priv->dialog_xml);
				break;

			case OFA_PROP_DIALOG_NAME_ID:
				g_value_set_string( value, priv->dialog_name );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
base_dialog_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaBaseDialogPrivate *priv;

	g_return_if_fail( OFA_IS_BASE_DIALOG( object ));
	priv = OFA_BASE_DIALOG( object )->priv;

	if( !OFA_BASE_DIALOG( object )->prot->dispose_has_run ){

		switch( property_id ){
			case OFA_PROP_MAIN_WINDOW_ID:
				priv->main_window = g_value_get_pointer( value );
				break;

			case OFA_PROP_DIALOG_XML_ID:
				g_free( priv->dialog_xml );
				priv->dialog_xml = g_value_dup_string( value );
				break;

			case OFA_PROP_DIALOG_NAME_ID:
				g_free( priv->dialog_name );
				priv->dialog_name = g_value_dup_string( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
ofa_base_dialog_init( ofaBaseDialog *self )
{
	static const gchar *thisfn = "ofa_base_dialog_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = g_new0( ofaBaseDialogPrivate, 1 );
	self->prot = g_new0( ofaBaseDialogProtected, 1 );
}

static void
ofa_base_dialog_class_init( ofaBaseDialogClass *klass )
{
	static const gchar *thisfn = "ofa_base_dialog_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = base_dialog_get_property;
	G_OBJECT_CLASS( klass )->set_property = base_dialog_set_property;
	G_OBJECT_CLASS( klass )->dispose = base_dialog_dispose;
	G_OBJECT_CLASS( klass )->finalize = base_dialog_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = NULL;
	OFA_BASE_DIALOG_CLASS( klass )->run_dialog = v_run_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_delete_event = v_quit_on_delete_event;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_cancel = v_quit_on_cancel;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_close = v_quit_on_close;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ), OFA_PROP_MAIN_WINDOW_ID,
			g_param_spec_pointer(
					OFA_PROP_MAIN_WINDOW,
					"Main window",
					"The main window of the application",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ), OFA_PROP_DIALOG_XML_ID,
			g_param_spec_string(
					OFA_PROP_DIALOG_XML,
					"UI XML pathname",
					"The pathname to the file which contains the UI definition",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ), OFA_PROP_DIALOG_NAME_ID,
			g_param_spec_string(
					OFA_PROP_DIALOG_NAME,
					"Dialog box name",
					"The unique name of the managed dialog box",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

/**
 * ofa_base_dialog_init_dialog:
 */
gboolean
ofa_base_dialog_init_dialog( ofaBaseDialog *dialog )
{
	g_return_val_if_fail( OFA_IS_BASE_DIALOG( dialog ), FALSE );

	if( !dialog->prot->dispose_has_run ){

		if( load_from_builder( dialog )){

			do_init_dialog( dialog );

			gtk_widget_show_all( GTK_WIDGET( dialog->prot->dialog ));

			dialog->priv->init_has_run = TRUE;

			return( TRUE );
		}
	}

	return( FALSE );
}

static void
do_init_dialog( ofaBaseDialog *dialog )
{
	static const gchar *thisfn = "ofa_base_dialog_do_init_dialog";

	g_return_if_fail( dialog && OFA_IS_BASE_DIALOG( dialog ));

	if( OFA_BASE_DIALOG_GET_CLASS( dialog )->init_dialog ){
		OFA_BASE_DIALOG_GET_CLASS( dialog )->init_dialog( dialog );

	} else {
		g_debug( "%s: dialog=%p (%s)",
				thisfn, ( void * ) dialog, G_OBJECT_TYPE_NAME( dialog ));
	}
}

static gboolean
load_from_builder( ofaBaseDialog *dialog )
{
	static const gchar *thisfn = "ofa_base_dialog_load_from_builder";
	gboolean loaded;
	GError *error;
	GtkBuilder *builder;

	loaded = FALSE;
	error = NULL;
	builder = gtk_builder_new();

	if( gtk_builder_add_from_file( builder, dialog->priv->dialog_xml, &error )){
		dialog->prot->dialog = GTK_DIALOG(
									gtk_builder_get_object( builder, dialog->priv->dialog_name ));
		if( !dialog->prot->dialog ){
			g_warning( "%s: unable to find '%s' object in '%s' file",
									thisfn, dialog->priv->dialog_name, dialog->priv->dialog_xml );
		} else {
			loaded = TRUE;
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}

	g_object_unref( builder );
	return( loaded );
}

/**
 * ofa_base_dialog_run_dialog:
 */
gint
ofa_base_dialog_run_dialog( ofaBaseDialog *dialog )
{
	gint code;

	code = GTK_RESPONSE_CANCEL;

	g_return_val_if_fail( OFA_IS_BASE_DIALOG( dialog ), code );

	if( !dialog->prot->dispose_has_run ){

		if( dialog->priv->init_has_run || ofa_base_dialog_init_dialog( dialog )){

			code = do_run_dialog( dialog );
		}
	}

	return( code );
}

static gint
do_run_dialog( ofaBaseDialog *dialog )
{
	gint code;

	g_return_val_if_fail( dialog && OFA_IS_BASE_DIALOG( dialog ), GTK_RESPONSE_CANCEL );

	if( OFA_BASE_DIALOG_GET_CLASS( dialog )->run_dialog ){
		code = OFA_BASE_DIALOG_GET_CLASS( dialog )->run_dialog( dialog );

	} else {
		code = v_run_dialog( dialog );
	}

	return( code );
}

static gint
v_run_dialog( ofaBaseDialog *dialog )
{
	static const gchar *thisfn = "ofa_base_dialog_v_run_dialog";
	gint code;

	g_debug( "%s: calling gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( dialog->prot->dialog );
		g_debug( "%s: gtk_dialog_run return code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( dialog, code ));

	return( code );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaBaseDialog *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_DELETE_EVENT:
			quit = do_quit_on_delete_event( self );
			break;
		case GTK_RESPONSE_CLOSE:
			quit = do_quit_on_close( self );
			break;
		case GTK_RESPONSE_CANCEL:
			quit = do_quit_on_cancel( self );
			break;

		case GTK_RESPONSE_OK:
			quit = do_quit_on_ok( self );
			break;
	}

	return( quit );
}

static gboolean
do_quit_on_delete_event( ofaBaseDialog *dialog )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( dialog && OFA_IS_BASE_DIALOG( dialog ), FALSE );

	if( OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_delete_event ){
		ok_to_quit = OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_delete_event( dialog );

	} else {
		ok_to_quit = v_quit_on_delete_event( dialog );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_delete_event( ofaBaseDialog *dialog )
{
	return( TRUE );
}

static gboolean
do_quit_on_cancel( ofaBaseDialog *dialog )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( dialog && OFA_IS_BASE_DIALOG( dialog ), FALSE );

	if( OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_cancel ){
		ok_to_quit = OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_cancel( dialog );

	} else {
		ok_to_quit = v_quit_on_cancel( dialog );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_cancel( ofaBaseDialog *dialog )
{
	return( TRUE );
}

static gboolean
do_quit_on_close( ofaBaseDialog *dialog )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( dialog && OFA_IS_BASE_DIALOG( dialog ), FALSE );

	if( OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_close ){
		ok_to_quit = OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_close( dialog );

	} else {
		ok_to_quit = v_quit_on_close( dialog );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_close( ofaBaseDialog *dialog )
{
	return( TRUE );
}

static gboolean
do_quit_on_ok( ofaBaseDialog *dialog )
{
	gboolean ok_to_quit;

	g_return_val_if_fail( dialog && OFA_IS_BASE_DIALOG( dialog ), FALSE );

	if( OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_ok ){
		ok_to_quit = OFA_BASE_DIALOG_GET_CLASS( dialog )->quit_on_ok( dialog );

	} else {
		ok_to_quit = v_quit_on_ok( dialog );
	}

	return( ok_to_quit );
}

static gboolean
v_quit_on_ok( ofaBaseDialog *dialog )
{
	return( TRUE );
}

/**
 * ofa_base_dialog_get_dossier:
 */
ofoDossier *
ofa_base_dialog_get_dossier( const ofaBaseDialog *dialog )
{
	g_return_val_if_fail( OFA_IS_BASE_DIALOG( dialog ), NULL );

	if( !dialog->prot->dispose_has_run ){

		return( ofa_main_window_get_dossier( dialog->priv->main_window ));
	}

	return( NULL );
}

/**
 * ofa_base_dialog_get_main_window:
 */
ofaMainWindow *
ofa_base_dialog_get_main_window( const ofaBaseDialog *dialog )
{
	g_return_val_if_fail( OFA_IS_BASE_DIALOG( dialog ), NULL );

	if( !dialog->prot->dispose_has_run ){

		return( dialog->priv->main_window );
	}

	return( NULL );
}
