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
#include "api/my-window-prot.h"
#include "api/ofa-settings.h"

#include "ui/ofa-iprintable.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-pdf-dialog.h"

/* private instance data
 */
struct _ofaPDFDialogPrivate {

	/* properties
	 */
	gchar     *label;					/* the tab label */
	gchar     *def_name;				/* the exported default name */
	gchar     *pref_name;				/* the user preference key */

	/* UI
	 */
	GtkWidget *filechooser;

	/* runtime data
	 */
	gchar     *uri;
};

/* class properties
 */
enum {
	PROP_LABEL_ID = 1,
	PROP_DEF_NAME_ID,
	PROP_PREF_NAME_ID
};

static const gchar *st_default_label    = N_( "Export as" );
static const gchar *st_default_def_name = N_( "Untitled" );

static void     init_filechooser( ofaPDFDialog *self );
static void     on_file_activated( GtkFileChooser *chooser, ofaPDFDialog *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean apply_on_filechooser( ofaPDFDialog *self );
static gboolean confirm_overwrite( const ofaPDFDialog *self, const gchar *fname );

G_DEFINE_TYPE( ofaPDFDialog, ofa_pdf_dialog, MY_TYPE_DIALOG );

static void
pdf_dialog_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_pdf_dialog_finalize";
	ofaPDFDialogPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PDF_DIALOG( instance ));

	/* free data members here */
	priv = OFA_PDF_DIALOG( instance )->priv;
	g_free( priv->label );
	g_free( priv->def_name );
	g_free( priv->pref_name );
	g_free( priv->uri );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_dialog_parent_class )->finalize( instance );
}

static void
pdf_dialog_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PDF_DIALOG( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_dialog_parent_class )->dispose( instance );
}

static void
pdf_dialog_constructed( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PDF_DIALOG( instance ));
	g_return_if_fail( !MY_WINDOW( instance )->prot->dispose_has_run );

	/* first, chain up to the parent class */
	G_OBJECT_CLASS( ofa_pdf_dialog_parent_class )->constructed( instance );

	/* add and initialize our GtkFileChooser */
	init_filechooser( OFA_PDF_DIALOG( instance ));

#if 0
	g_return_if_fail( OFA_IS_IPRINTABLE( instance ));
	ofa_iprintable_init( OFA_IPRINTABLE( instance ));
#endif
}

static void
pdf_dialog_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaPDFDialogPrivate *priv;

	g_return_if_fail( object && OFA_IS_PDF_DIALOG( object ));

	if( !MY_WINDOW( object )->prot->dispose_has_run ){

		priv = OFA_PDF_DIALOG( object )->priv;

		switch( property_id ){
			case PROP_LABEL_ID:
				g_value_set_string( value, priv->label );
				break;

			case PROP_DEF_NAME_ID:
				g_value_set_string( value, priv->def_name );
				break;

			case PROP_PREF_NAME_ID:
				g_value_set_string( value, priv->pref_name );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
pdf_dialog_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaPDFDialogPrivate *priv;

	g_return_if_fail( object && OFA_IS_PDF_DIALOG( object ));

	if( !MY_WINDOW( object )->prot->dispose_has_run ){

		priv = OFA_PDF_DIALOG( object )->priv;

		switch( property_id ){
			case PROP_LABEL_ID:
				g_free( priv->label );
				priv->label = g_value_dup_string( value );
				break;

			case PROP_DEF_NAME_ID:
				g_free( priv->def_name );
				priv->def_name = g_value_dup_string( value );
				break;

			case PROP_PREF_NAME_ID:
				g_free( priv->pref_name );
				priv->pref_name = g_value_dup_string( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
ofa_pdf_dialog_init( ofaPDFDialog *self )
{
	static const gchar *thisfn = "ofa_pdf_dialog_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PDF_DIALOG( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PDF_DIALOG, ofaPDFDialogPrivate );

	self->priv->label = g_strdup( st_default_label );
	self->priv->def_name = g_strdup( st_default_def_name );
}

static void
ofa_pdf_dialog_class_init( ofaPDFDialogClass *klass )
{
	static const gchar *thisfn = "ofa_pdf_dialog_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->set_property = pdf_dialog_set_property;
	G_OBJECT_CLASS( klass )->get_property = pdf_dialog_get_property;
	G_OBJECT_CLASS( klass )->constructed = pdf_dialog_constructed;
	G_OBJECT_CLASS( klass )->dispose = pdf_dialog_dispose;
	G_OBJECT_CLASS( klass )->finalize = pdf_dialog_finalize;

	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaPDFDialogPrivate ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_LABEL_ID,
			g_param_spec_string(
					PDF_PROP_LABEL,
					"Tab label",
					"The label of the GtkFileChooser added tab",
					st_default_label,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_DEF_NAME_ID,
			g_param_spec_string(
					PDF_PROP_DEF_NAME,
					"Default name",
					"The default name of the exported PDF file",
					st_default_def_name,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_PREF_NAME_ID,
			g_param_spec_string(
					PDF_PROP_PREF_NAME,
					"Preference key",
					"The key of the user preference which stores the last selected URI",
					"",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

/*
 * this is called at construction time
 */
static void
init_filechooser( ofaPDFDialog *self )
{
	ofaPDFDialogPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *book, *label;
	gchar *last_uri, *basename;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));
	book = my_utils_container_get_child_by_type( GTK_CONTAINER( toplevel ), GTK_TYPE_NOTEBOOK );

	/* create the new tab, adding it at the end of the notebook */
	if( book ){
		priv->filechooser = gtk_file_chooser_widget_new( GTK_FILE_CHOOSER_ACTION_SAVE );
		gtk_widget_set_margin_left( priv->filechooser, 4 );
		gtk_widget_set_margin_top( priv->filechooser, 4 );
		gtk_widget_set_margin_right( priv->filechooser, 4 );
		gtk_widget_set_margin_bottom( priv->filechooser, 4 );

		label = gtk_label_new( priv->label );
		gtk_notebook_append_page( GTK_NOTEBOOK( book ), priv->filechooser, label );

		gtk_file_chooser_set_do_overwrite_confirmation(
				GTK_FILE_CHOOSER( priv->filechooser ), TRUE );

		g_signal_connect(
				G_OBJECT( priv->filechooser ),
				"file-activated", G_CALLBACK( on_file_activated ), self );
	}

	/* get the last stored filename (if any) */
	last_uri = NULL;
	if( my_strlen( priv->pref_name )){
		last_uri = ofa_settings_get_string( priv->pref_name );
	}

	if( my_strlen( last_uri )){
		gtk_file_chooser_set_uri( GTK_FILE_CHOOSER( priv->filechooser ), last_uri );
		basename = g_path_get_basename( last_uri );
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->filechooser ), basename );
		g_free( basename );

	} else {
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->filechooser ), priv->def_name );
	}
}

static void
on_file_activated( GtkFileChooser *chooser, ofaPDFDialog *self )
{
	g_return_if_fail( OFA_IS_PDF_DIALOG( self ));

	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

/*
 * Check that the filename is not empty
 *
 * Cannot call ofa_iprintable_apply() here as we are not sure that the
 * child class has already done its own checks. So it will be to the
 * child class to trigger the printing...
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	gboolean ok;

	ok = apply_on_filechooser( OFA_PDF_DIALOG( dialog ));

	return( ok );
}

/*
 * all fields are optional
 */
static gboolean
apply_on_filechooser( ofaPDFDialog *self )
{
	ofaPDFDialogPrivate *priv;

	priv = self->priv;

	g_free( priv->uri );
	priv->uri = NULL;

	/* the export filename is the only mandatory argument */
	priv->uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->filechooser ));
	if( !my_strlen( priv->uri )){
		my_utils_dialog_warning( _( "Empty export selection: unable to continue" ));
		return( FALSE );
	}

	if( my_utils_uri_exists( priv->uri )){
		if( !confirm_overwrite( self, priv->uri )){
			return( FALSE );
		}
	}

	if( my_strlen( priv->pref_name )){
		ofa_settings_set_string( priv->pref_name, priv->uri );
	}

	return( TRUE );
}

/*
 * should be directly managed by the GtkFileChooser class, but doesn't
 * seem to work :(
 *
 * Returns: %TRUE in order to confirm overwrite.
 */
static gboolean
confirm_overwrite( const ofaPDFDialog *self, const gchar *fname )
{
	GtkWidget *dialog;
	gchar *str;
	gint response;

	str = g_strdup_printf(
				_( "The file '%s' already exists.\n"
					"Are you sure you want to overwrite it ?" ), fname );

	dialog = gtk_message_dialog_new(
			my_window_get_toplevel( MY_WINDOW( self )),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_NONE,
			"%s", str);

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			_( "Cancel" ), GTK_RESPONSE_CANCEL,
			_( "Overwrite" ), GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}

/**
 * ofa_pdf_dialog_get_filename:
 *
 * Returns: the filename from the selected URI as a newly allocated
 * string which should be g_free() by the caller.
 *
 * This must be called after the user has validated the dialog, and the
 * checks have been OK.
 */
gchar *
ofa_pdf_dialog_get_filename( const ofaPDFDialog *dialog )
{
	static const gchar *thisfn = "ofa_pdf_dialog_get_filename";
	gchar *fname;
	GError *error;

	g_return_val_if_fail( OFA_IS_PDF_DIALOG( dialog ), NULL );

	if( !MY_WINDOW( dialog )->prot->dispose_has_run ){

		error = NULL;
		fname = g_filename_from_uri( dialog->priv->uri, NULL, &error );
		if( error ){
			g_warning( "%s: %s", thisfn, error->message );
			g_free( fname );
			fname = NULL;
		}

		return( fname );
	}

	return( NULL );
}

/**
 * ofa_pdf_dialog_get_uri:
 *
 * Returns: the selected URI.
 *
 * This must be called after the user has validated the dialog, and the
 * checks have been OK.
 */
const gchar *
ofa_pdf_dialog_get_uri( const ofaPDFDialog *dialog )
{
	g_return_val_if_fail( OFA_IS_PDF_DIALOG( dialog ), NULL );

	if( !MY_WINDOW( dialog )->prot->dispose_has_run ){

		return( dialog->priv->uri );
	}

	return( NULL );
}
