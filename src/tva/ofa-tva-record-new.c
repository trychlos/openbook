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

#include "my/my-date-editable.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itheme-manager.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-record-new.h"
#include "tva/ofo-tva-record.h"
#include "tva/ofa-tva-record-properties.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* internals
	 */
	ofaIGetter   *getter;
	ofoTVARecord *tva_record;

	/* UI
	 */
	GtkWidget    *end_date;
	GtkWidget    *ok_btn;
	GtkWidget    *msg_label;
}
	ofaTVARecordNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/tva/ofa-tva-record-new.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static gchar   *iwindow_get_identifier( const myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_properties( ofaTVARecordNew *self );
static void     on_end_changed( GtkEntry *entry, ofaTVARecordNew *self );
static void     check_for_enable_dlg( ofaTVARecordNew *self );
static gboolean do_update( ofaTVARecordNew *self, gchar **msgerr );
static void     set_msgerr( ofaTVARecordNew *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaTVARecordNew, ofa_tva_record_new, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaTVARecordNew )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
tva_record_new_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_record_new_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_NEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_new_parent_class )->finalize( instance );
}

static void
tva_record_new_dispose( GObject *instance )
{
	ofaTVARecordNewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_RECORD_NEW( instance ));

	priv = ofa_tva_record_new_get_instance_private( OFA_TVA_RECORD_NEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_record_new_parent_class )->dispose( instance );
}

static void
ofa_tva_record_new_init( ofaTVARecordNew *self )
{
	static const gchar *thisfn = "ofa_tva_record_new_init";
	ofaTVARecordNewPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_RECORD_NEW( self ));

	priv = ofa_tva_record_new_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_tva_record_new_class_init( ofaTVARecordNewClass *klass )
{
	static const gchar *thisfn = "ofa_tva_record_new_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_record_new_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_record_new_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_tva_record_new_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 * @record: the new #ofoTVARecord.
 *
 * Let the user enter the end date of the declaration.
 */
void
ofa_tva_record_new_run( ofaIGetter *getter, GtkWindow *parent, ofoTVARecord *record )
{
	static const gchar *thisfn = "ofa_tva_record_new_run";
	ofaTVARecordNew *self;
	ofaTVARecordNewPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, record=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) record );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	self = g_object_new( OFA_TYPE_TVA_RECORD_NEW, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_tva_record_new_get_instance_private( self );

	priv->getter = getter;
	priv->tva_record = record;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_record_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_identifier = iwindow_get_identifier;
}

/*
 * identifier is built with class name and template mnemo
 */
static gchar *
iwindow_get_identifier( const myIWindow *instance )
{
	ofaTVARecordNewPrivate *priv;
	gchar *id;

	priv = ofa_tva_record_new_get_instance_private( OFA_TVA_RECORD_NEW( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_tva_record_get_mnemo( priv->tva_record ));

	return( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_record_new_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_writable' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window and
 * VAT record
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_tva_record_new_idialog_init";
	ofaTVARecordNewPrivate *priv;
	gchar *title;
	const gchar *mnemo;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_record_new_get_instance_private( OFA_TVA_RECORD_NEW( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	my_idialog_click_to_update( instance, priv->ok_btn, ( myIDialogUpdateCb ) do_update );

	mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
	title = g_strdup_printf( _( "New declaration from « %s » TVA form" ), mnemo );
	gtk_window_set_title( GTK_WINDOW( instance ), title );

	init_properties( OFA_TVA_RECORD_NEW( instance ));

	check_for_enable_dlg( OFA_TVA_RECORD_NEW( instance ));
}

static void
init_properties( ofaTVARecordNew *self )
{
	ofaTVARecordNewPrivate *priv;
	GtkWidget *entry, *label;
	const gchar *cstr;

	priv = ofa_tva_record_new_get_instance_private( self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_mnemo( priv->tva_record );
	g_return_if_fail( my_strlen( cstr ));
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_label( priv->tva_record );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* declaration date */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->end_date = entry;
	my_utils_widget_set_editable( entry, TRUE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end-date" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), FALSE );
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_date_editable_set_date( GTK_EDITABLE( entry ), NULL );

	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
}

static void
on_end_changed( GtkEntry *entry, ofaTVARecordNew *self )
{
	ofaTVARecordNewPrivate *priv;
	const GDate *date;

	priv = ofa_tva_record_new_get_instance_private( self );

	date = my_date_editable_get_date( GTK_EDITABLE( priv->end_date ), NULL );
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
	ofaHub *hub;

	priv = ofa_tva_record_new_get_instance_private( self );

	msgerr = NULL;
	ok_valid = FALSE;
	hub = ofa_igetter_get_hub( priv->getter );

	dend = ofo_tva_record_get_end( priv->tva_record );
	if( !my_date_is_valid( dend )){
		msgerr = g_strdup( _( "End date is not valid" ));
	} else {
		mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
		exists = ( ofo_tva_record_get_by_key( hub, mnemo, dend ) != NULL );
		if( exists ){
			msgerr = g_strdup( _( "End date overlaps with an already defined declaration" ));
		} else {
			ok_valid = TRUE;
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	gtk_widget_set_sensitive( priv->ok_btn, ok_valid );
}

/*
 * Creating a new tva declaration.
 *
 * When the creation of a new VAT record is confirmed, then:
 * - activate (or open) the declarations management page
 * - open the declaration for edition.
 *
 * The dialog will be closed by the myIWindow interface.
 */
static gboolean
do_update( ofaTVARecordNew *self, gchar **msgerr )
{
	ofaTVARecordNewPrivate *priv;
	gboolean ok;
	ofaHub *hub;
	ofaIThemeManager *manager;
	ofaPage *page;
	GtkWindow *toplevel;
	GDate last_end;
	const gchar *mnemo;

	priv = ofa_tva_record_new_get_instance_private( self );

	hub = ofa_igetter_get_hub( priv->getter );
	manager = ofa_igetter_get_theme_manager( priv->getter );

	/* setup a default begin date
	 * = previous end date + 1 */
	mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
	ofo_tva_record_get_last_end( hub, mnemo, &last_end );
	if( my_date_is_valid( &last_end )){
		g_date_add_days( &last_end, 1 );
		ofo_tva_record_set_begin( priv->tva_record, &last_end );
	}

	ok = ofo_tva_record_insert( priv->tva_record, hub );
	if( !ok ){
		*msgerr = g_strdup( _( "Unable to create this new VAT declaration" ));
	}

	if( ok ){
		/* activate the page */
		page = ofa_itheme_manager_activate( manager, OFA_TYPE_TVA_DECLARE_PAGE );
		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( page ));
		/* edit the declaration */
		ofa_tva_record_properties_run( priv->getter, toplevel, priv->tva_record );
	}

	return( ok );
}

static void
set_msgerr( ofaTVARecordNew *self, const gchar *msg )
{
	ofaTVARecordNewPrivate *priv;

	priv = ofa_tva_record_new_get_instance_private( self );

	if( !priv->msg_label ){
		priv->msg_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( priv->msg_label && GTK_IS_LABEL( priv->msg_label ));
		my_utils_widget_set_style( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
