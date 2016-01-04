/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-editable-date.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-record-new.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
struct _ofaTVARecordNewPrivate {

	/* internals
	 */
	ofoDossier    *dossier;
	ofoTVARecord  *tva_record;
	gboolean       updated;

	/* UI
	 */
	GtkWidget     *end_date;
	GtkWidget     *ok_btn;
	GtkWidget     *msg_label;
};

static const gchar *st_ui_xml           = PLUGINUIDIR "/ofa-tva-record-new.ui";
static const gchar *st_ui_id            = "TVARecordNewDialog";

static void     v_init_dialog( myDialog *dialog );
static void     init_properties( ofaTVARecordNew *self, GtkContainer *container );
static void     on_end_changed( GtkEntry *entry, ofaTVARecordNew *self );
static void     check_for_enable_dlg( ofaTVARecordNew *self );
static gboolean v_quit_on_ok( myDialog *dialog );
static gboolean do_update( ofaTVARecordNew *self );
static void     set_message( ofaTVARecordNew *dialog, const gchar *msg );

G_DEFINE_TYPE( ofaTVARecordNew, ofa_tva_record_new, MY_TYPE_DIALOG )

static void
tva_record_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_new_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_TVA_RECORD_NEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_new_parent_class )->finalize( instance );
}

static void
tva_record_new_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_TVA_RECORD_NEW( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_new_parent_class )->dispose( instance );
}

static void
ofa_tva_record_new_init( ofaTVARecordNew *self )
{
	static const gchar *thisfn = "ofa_tva_record_new_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_TVA_RECORD_NEW( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNewPrivate );

	self->priv->updated = FALSE;
}

static void
ofa_tva_record_new_class_init( ofaTVARecordNewClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_new_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaTVARecordNewPrivate ));
}

/**
 * ofa_tva_record_new_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @record: the new #ofoTVARecord.
 *
 * Let the user enter the end date of the declaration.
 *
 * Returns: %TRUE if the end date has been validated and the new
 * declaration recorded in the DBMS, %FALSE else
 */
gboolean
ofa_tva_record_new_run( const ofaMainWindow *main_window, ofoTVARecord *record )
{
	static const gchar *thisfn = "ofa_tva_record_new_run";
	ofaTVARecordNew *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, record=%p",
			thisfn, ( void * ) main_window, ( void * ) record );

	self = g_object_new(
					OFA_TYPE_TVA_RECORD_NEW,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->tva_record = record;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaTVARecordNew *self;
	ofaTVARecordNewPrivate *priv;
	GtkApplicationWindow *main_window;
	gchar *title;
	const gchar *mnemo;
	GtkContainer *container;

	self = OFA_TVA_RECORD_NEW( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	priv->dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
	title = g_strdup_printf( _( "New declaration from « %s » TVA form" ), mnemo );
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->ok_btn = my_utils_container_get_child_by_name( container, "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	init_properties( self, container );

	check_for_enable_dlg( self );
}

static void
init_properties( ofaTVARecordNew *self, GtkContainer *container )
{
	ofaTVARecordNewPrivate *priv;
	GtkWidget *entry, *label;
	const gchar *cstr;

	priv = self->priv;

	entry = my_utils_container_get_child_by_name( container, "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_mnemo( priv->tva_record );
	g_return_if_fail( my_strlen( cstr ));
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( container, "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( container, "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_label( priv->tva_record );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( container, "p1-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_editable_date_set_date( GTK_EDITABLE( entry ), NULL );
	my_utils_widget_set_editable( entry, TRUE );
	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
	priv->end_date = entry;

	label = my_utils_container_get_child_by_name( container, "p1-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( container, "p1-end-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
}

static void
on_end_changed( GtkEntry *entry, ofaTVARecordNew *self )
{
	ofaTVARecordNewPrivate *priv;
	const GDate *date;

	priv = self->priv;
	date = my_editable_date_get_date( GTK_EDITABLE( priv->end_date ), NULL );
	ofo_tva_record_set_end( priv->tva_record, date );

	check_for_enable_dlg( self );
}

/*
 * must have an end date to be able to record the declaration
 */
static void
check_for_enable_dlg( ofaTVARecordNew *self )
{
	ofaTVARecordNewPrivate *priv;
	const GDate *dend;
	const gchar *mnemo;
	gboolean ok_valid, exists;
	gchar *msgerr;

	priv = self->priv;
	msgerr = NULL;
	ok_valid = FALSE;

	dend = ofo_tva_record_get_end( priv->tva_record );
	if( !my_date_is_valid( dend )){
		msgerr = g_strdup( _( "End date is not valid" ));
	} else {
		mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
		exists = ( ofo_tva_record_get_by_key( priv->dossier, mnemo, dend ) != NULL );
		if( exists ){
			msgerr = g_strdup( _( "Same declaration is already defined" ));
		} else {
			ok_valid = TRUE;
		}
	}

	set_message( self, msgerr );
	g_free( msgerr );

	gtk_widget_set_sensitive(
			priv->ok_btn,
			ok_valid );
}

/*
 * OK button: records the update and quits the dialog
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_TVA_RECORD_NEW( dialog )));
}

/*
 * creating a new tva declaration
 */
static gboolean
do_update( ofaTVARecordNew *self )
{
	ofaTVARecordNewPrivate *priv;

	priv = self->priv;

	priv->updated = ofo_tva_record_insert( priv->tva_record, priv->dossier );

	return( priv->updated );
}

static void
set_message( ofaTVARecordNew *dialog, const gchar *msg )
{
	ofaTVARecordNewPrivate *priv;

	priv = dialog->priv;

	if( !priv->msg_label ){
		priv->msg_label =
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
						"px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
		my_utils_widget_set_style( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
