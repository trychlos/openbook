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

#include "api/ofo-dossier.h"

#include "core/my-utils.h"

#include "ui/my-window-prot.h"
#include "ui/ofa-devise-combo.h"
#include "ui/ofa-dossier-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaDossierPropertiesPrivate {

	/* internals
	 */
	ofoDossier    *dossier;
	gboolean       is_new;
	gboolean       updated;
	GDate          last_closed;

	/* data
	 */
	gchar         *label;
	gint           duree;
	gchar         *devise;
	GDate          begin;
	GDate          end;

	/* UI
	 */
	GtkLabel      *begin_label;
	GtkLabel      *end_label;
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-dossier-properties.ui";
static const gchar *st_ui_id  = "DossierPropertiesDlg";

G_DEFINE_TYPE( ofaDossierProperties, ofa_dossier_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_properties_page( ofaDossierProperties *self );
static void      init_current_exe_page( ofaDossierProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_devise_changed( const gchar *code, ofaDossierProperties *self );
static void      on_begin_date_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_end_date_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      check_for_enable_dlg( ofaDossierProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaDossierProperties *self );

static void
dossier_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_finalize";
	ofaDossierPropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	priv = OFA_DOSSIER_PROPERTIES( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv->label );
	g_free( priv->devise );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->finalize( instance );
}

static void
dossier_properties_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_properties_parent_class )->dispose( instance );
}

static void
ofa_dossier_properties_init( ofaDossierProperties *self )
{
	static const gchar *thisfn = "ofa_dossier_properties_init";

	g_return_if_fail( self && OFA_IS_DOSSIER_PROPERTIES( self ));

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

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
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
				MY_PROP_MAIN_WINDOW, main_window,
				MY_PROP_WINDOW_XML,  st_ui_xml,
				MY_PROP_WINDOW_NAME, st_ui_id,
				NULL );

	self->private->dossier = dossier;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierProperties *self;
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;

	self = OFA_DOSSIER_PROPERTIES( dialog );
	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	init_properties_page( self );

	init_current_exe_page( OFA_DOSSIER_PROPERTIES( dialog ));

	my_utils_init_notes_ex( container, dossier );
	my_utils_init_maj_user_stamp_ex( container, dossier );

	check_for_enable_dlg( self );
}

static void
init_properties_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	GtkWidget *entry;
	gchar *str;
	ofaDeviseComboParms parms;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	priv->label = g_strdup( ofo_dossier_get_label( priv->dossier ));
	entry = my_utils_container_get_child_by_name( container, "p1-label" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	if( priv->label ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

	priv->duree = ofo_dossier_get_exercice_length( priv->dossier );
	entry = my_utils_container_get_child_by_name( container, "p1-exe-length" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	if( priv->duree > 0 ){
		str = g_strdup_printf( "%d", priv->duree );
	} else {
		str = g_strdup_printf( "%d", DOS_DEFAULT_LENGTH );
	}
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_duree_changed ), self );

	priv->devise = g_strdup( ofo_dossier_get_default_devise( priv->dossier ));

	parms.container = container;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-devise";
	parms.label_name = NULL;
	parms.disp_code = TRUE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaDeviseComboCb ) on_devise_changed;
	parms.user_data = self;
	parms.initial_code = priv->devise;

	ofa_devise_combo_new( &parms );
}

static void
init_current_exe_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	GtkWidget *entry;
	GtkWidget *label;
	gchar *str;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	memcpy( &priv->last_closed, ofo_dossier_get_last_closed_exercice( priv->dossier ), sizeof( GDate ));

	entry = my_utils_container_get_child_by_name( container, "p2-begin" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_begin_date_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p2-begin-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->begin_label = GTK_LABEL( label );

	entry = my_utils_container_get_child_by_name( container, "p2-end" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_end_date_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p2-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->end_label = GTK_LABEL( label );

	label = my_utils_container_get_child_by_name( container, "p2-id" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%u", ofo_dossier_get_current_exe_id( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p2-last-ecr-number" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%u", ofo_dossier_get_current_exe_last_ecr( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p2-status" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_text( GTK_LABEL( label ), ofo_dossier_get_exe_status_label( DOS_STATUS_OPENED ));
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

/*
 * ofaDeviseComboCb
 */
static void
on_devise_changed( const gchar *code, ofaDossierProperties *self )
{
	g_free( self->private->devise );
	self->private->devise = g_strdup( code );

	check_for_enable_dlg( self );
}

static void
on_begin_date_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GDate date;
	gchar *str;

	priv = self->private;

	g_date_set_parse( &date, gtk_entry_get_text( entry ));

	if( g_date_valid( &date ) &&
		( !g_date_valid( &priv->last_closed ) || g_date_compare( &priv->last_closed, &date ) < 0 )){

			str = my_utils_display_from_date( &date, MY_UTILS_DATE_DMMM );
			memcpy( &priv->begin, &date, sizeof( GDate ));

	} else {
		str = g_strdup( "" );
	}

	gtk_label_set_text( priv->begin_label, str );
	g_free( str );
}

static void
on_end_date_changed( GtkEntry *entry, ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GDate date;
	gchar *str;

	priv = self->private;

	g_date_set_parse( &date, gtk_entry_get_text( entry ));

	if( g_date_valid( &date ) &&
		( !g_date_valid( &priv->begin ) || g_date_compare( &priv->begin, &date ) < 0 )){

		str = my_utils_display_from_date( &date, MY_UTILS_DATE_DMMM );
		memcpy( &priv->end, &date, sizeof( GDate ));

	} else {
		str = g_strdup( "" );
	}

	gtk_label_set_text( priv->end_label, str );
	g_free( str );
}

static void
check_for_enable_dlg( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkWidget *button;
	gboolean ok;

	priv = self->private;

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	ok = ofo_dossier_is_valid( priv->label, priv->duree, priv->devise );

	gtk_widget_set_sensitive( button, ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_DOSSIER_PROPERTIES( dialog )));
}

static gboolean
do_update( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;

	g_return_val_if_fail(
			ofo_dossier_is_valid( self->private->label, self->private->duree, self->private->devise ),
			FALSE );

	priv = self->private;

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_exercice_length( priv->dossier, priv->duree );
	ofo_dossier_set_default_devise( priv->dossier, priv->devise );

	if( g_date_valid( &priv->begin )){
		ofo_dossier_set_current_exe_deb( priv->dossier, &priv->begin );
	}
	if( g_date_valid( &priv->end )){
		ofo_dossier_set_current_exe_fin( priv->dossier, &priv->end );
	}

	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), dossier );

	priv->updated = ofo_dossier_update( priv->dossier );

	return( priv->updated );
}
