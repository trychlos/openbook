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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-dossier.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-currency-combo.h"
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
	GDate         *last_closed;		/* may be NULL */

	/* data
	 */
	gchar         *label;
	gint           duree;
	gchar         *currency;
	GDate          begin;
	GDate          end;

	/* UI
	 */
	GtkWidget     *begin_entry;
	GtkWidget     *end_entry;
};

static const gchar *st_ui_xml = PKGUIDIR "/ofa-dossier-properties.ui";
static const gchar *st_ui_id  = "DossierPropertiesDlg";

G_DEFINE_TYPE( ofaDossierProperties, ofa_dossier_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_properties_page( ofaDossierProperties *self );
static void      init_current_exe_page( ofaDossierProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_duree_changed( GtkEntry *entry, ofaDossierProperties *self );
static void      on_currency_changed( const gchar *code, ofaDossierProperties *self );
static void      check_for_enable_dlg( ofaDossierProperties *self );
static gboolean  is_dialog_valid( ofaDossierProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaDossierProperties *self );

static void
dossier_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_properties_finalize";
	ofaDossierPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPERTIES( instance ));

	priv = OFA_DOSSIER_PROPERTIES( instance )->private;

	/* free data members here */
	g_free( priv->label );
	g_free( priv->currency );
	g_free( priv->last_closed );
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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_PROPERTIES( self ));

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
	init_current_exe_page( self );

	my_utils_init_notes_ex( container, dossier );
	my_utils_init_upd_user_stamp_ex( container, dossier );

	check_for_enable_dlg( self );
}

static void
init_properties_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	GtkWidget *entry;
	gchar *str;
	ofaCurrencyComboParms parms;
	const gchar *costr;
	gint ivalue;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	entry = my_utils_container_get_child_by_name( container, "p1-label" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );
	costr = ofo_dossier_get_label( priv->dossier );
	if( costr ){
		gtk_entry_set_text( GTK_ENTRY( entry ), costr );
	}

	entry = my_utils_container_get_child_by_name( container, "p1-exe-length" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_duree_changed ), self );
	ivalue = ofo_dossier_get_exercice_length( priv->dossier );
	str = g_strdup_printf( "%d", ivalue );
	gtk_entry_set_text( GTK_ENTRY( entry ), str );
	g_free( str );

	parms.container = container;
	parms.dossier = priv->dossier;
	parms.combo_name = "p1-currency";
	parms.label_name = NULL;
	parms.disp_code = TRUE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaCurrencyComboCb ) on_currency_changed;
	parms.user_data = self;
	parms.initial_code = ofo_dossier_get_default_currency( priv->dossier );
	ofa_currency_combo_new( &parms );
}

static void
init_current_exe_page( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	GtkContainer *container;
	GtkWidget *label, *entry;
	gchar *str;

	priv = self->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	priv->last_closed = ofo_dossier_get_last_closed_exercice( priv->dossier );

	my_date_set_from_date( &priv->begin, ofo_dossier_get_current_exe_begin( priv->dossier ));
	entry = my_utils_container_get_child_by_name( container, "p2-begin" );
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), MY_DATE_DMYY );
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->begin );
	label = my_utils_container_get_child_by_name( container, "p2-begin-label" );
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, MY_DATE_DMMM );
	priv->begin_entry = entry;

	my_date_set_from_date( &priv->end, ofo_dossier_get_current_exe_end( priv->dossier ));
	entry = my_utils_container_get_child_by_name( container, "p2-end" );
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), MY_DATE_DMYY );
	my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->end );
	label = my_utils_container_get_child_by_name( container, "p2-end-label" );
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, MY_DATE_DMMM );
	priv->end_entry = entry;

	label = my_utils_container_get_child_by_name( container, "p2-id" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%u", ofo_dossier_get_current_exe_id( priv->dossier ));
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	label = my_utils_container_get_child_by_name( container, "p2-last-entry" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = g_strdup_printf( "%u", ofo_dossier_get_current_exe_last_entry( priv->dossier ));
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
 * ofaCurrencyComboCb
 */
static void
on_currency_changed( const gchar *code, ofaDossierProperties *self )
{
	g_free( self->private->currency );
	self->private->currency = g_strdup( code );

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
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	my_date_set_from_date( &priv->begin, my_editable_date_get_date( GTK_EDITABLE( priv->begin_entry ), NULL ));
	my_date_set_from_date( &priv->end, my_editable_date_get_date( GTK_EDITABLE( priv->end_entry ), NULL ));

	/*g_debug( "label=%s, duree=%u, currency=%s", priv->label, priv->duree, priv->currency );*/
	ok = is_dialog_valid( self );

	gtk_widget_set_sensitive( button, ok );
}

static gboolean
is_dialog_valid( ofaDossierProperties *self )
{
	ofaDossierPropertiesPrivate *priv;
	gboolean ok;

	priv = self->private;

	ok = !my_date_is_valid( &priv->begin ) || my_date_compare( &priv->begin, priv->last_closed ) < 0;
	ok &= ofo_dossier_is_valid( priv->label, priv->duree, priv->currency, &priv->begin, &priv->end );

	return( ok );
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

	g_return_val_if_fail( is_dialog_valid( self ), FALSE );

	priv = self->private;

	ofo_dossier_set_label( priv->dossier, priv->label );
	ofo_dossier_set_exercice_length( priv->dossier, priv->duree );
	ofo_dossier_set_default_currency( priv->dossier, priv->currency );

	if( my_date_is_valid( &priv->begin )){
		ofo_dossier_set_current_exe_begin( priv->dossier, &priv->begin );
	}
	if( my_date_is_valid( &priv->end )){
		ofo_dossier_set_current_exe_end( priv->dossier, &priv->end );
	}

	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), dossier );

	priv->updated = ofo_dossier_update( priv->dossier );

	return( priv->updated );
}
