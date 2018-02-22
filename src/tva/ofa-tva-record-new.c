/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-ipage-manager.h"
#include "api/ofa-prefs.h"
#include "api/ofo-dossier.h"

#include "tva/ofa-tva-main.h"
#include "tva/ofa-tva-record-new.h"
#include "tva/ofa-tva-record-page.h"
#include "tva/ofa-tva-record-properties.h"
#include "tva/ofo-tva-form.h"
#include "tva/ofo-tva-record.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	ofaIGetter   *getter;
	GtkWindow    *parent;
	ofoTVARecord *tva_record;

	/* runtime
	 */
	GtkWindow    *actual_parent;
	ofoTVAForm   *form;

	/* UI
	 */
	GtkWidget    *label_entry;
	GtkWidget    *end_date;
	GtkWidget    *ok_btn;
	GtkWidget    *msg_label;
}
	ofaTVARecordNewPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/vat/ofa-tva-record-new.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_properties( ofaTVARecordNew *self );
static void     on_label_changed( GtkEditable *entry, ofaTVARecordNew *self );
static void     on_end_changed( GtkEntry *entry, ofaTVARecordNew *self );
static void     check_for_enable_dlg( ofaTVARecordNew *self );
static void     on_ok_clicked( ofaTVARecordNew *self );
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

	priv = ofa_tva_record_new_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->tva_record = record;

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_record_new_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_tva_record_new_iwindow_init";
	ofaTVARecordNewPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_record_new_get_instance_private( OFA_TVA_RECORD_NEW( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ), ofo_tva_record_get_mnemo( priv->tva_record ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
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
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_tva_record_new_get_instance_private( OFA_TVA_RECORD_NEW( instance ));

	/* update properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	priv->form = ofo_tva_form_get_by_mnemo( priv->getter, ofo_tva_record_get_mnemo( priv->tva_record ));
	g_return_if_fail( priv->form && OFO_IS_TVA_FORM( priv->form ));

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

	/* mnemonic (invariant) */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_mnemo( priv->tva_record );
	g_return_if_fail( my_strlen( cstr ));
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	my_utils_widget_set_editable( entry, FALSE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* label */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	cstr = ofo_tva_record_get_label( priv->tva_record );
	g_return_if_fail( my_strlen( cstr ));
	gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );
	priv->label_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-prompt" );
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
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_date( GTK_EDITABLE( entry ), NULL );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_end_changed ), self );
}

static void
on_label_changed( GtkEditable *entry, ofaTVARecordNew *self )
{
	check_for_enable_dlg( self );
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
	const gchar *mnemo, *cstr;
	gboolean ok_valid, exists;
	gchar *msgerr;

	priv = ofa_tva_record_new_get_instance_private( self );

	msgerr = NULL;
	ok_valid = TRUE;

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->label_entry ));
	if( !my_strlen( cstr )){
		msgerr = g_strdup( _( "Label is empty" ));
		ok_valid = FALSE;

	} else {
		dend = ofo_tva_record_get_end( priv->tva_record );
		if( !my_date_is_valid( dend )){
			msgerr = g_strdup( _( "Ending date is not set or invalid" ));
			ok_valid = FALSE;

		} else {
			mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
			exists = ( ofo_tva_record_get_by_key( priv->getter, mnemo, dend ) != NULL );
			if( exists ){
				msgerr = g_strdup( _( "This new record overlaps with an already defined VAT declaration" ));
				ok_valid = FALSE;
			}
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
 */
static void
on_ok_clicked( ofaTVARecordNew *self )
{
	gchar *msgerr = NULL;

	do_update( self, &msgerr );

	if( my_strlen( msgerr )){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, msgerr );
		g_free( msgerr );
	}

	my_iwindow_close( MY_IWINDOW( self ));
}

static gboolean
do_update( ofaTVARecordNew *self, gchar **msgerr )
{
	ofaTVARecordNewPrivate *priv;
	gboolean ok;
	ofaIPageManager *manager;
	ofaPage *page;
	GtkWindow *toplevel;
	GDate last_end;
	const gchar *mnemo;

	priv = ofa_tva_record_new_get_instance_private( self );

	manager = ofa_igetter_get_page_manager( priv->getter );

	/* setup a default begin date
	 * = previous end date + 1 */
	mnemo = ofo_tva_record_get_mnemo( priv->tva_record );
	ofo_tva_record_get_last_end( priv->getter, mnemo, &last_end );
	if( my_date_is_valid( &last_end )){
		g_date_add_days( &last_end, 1 );
		ofo_tva_record_set_begin( priv->tva_record, &last_end );
	}

	ofo_tva_record_set_label( priv->tva_record, gtk_entry_get_text( GTK_ENTRY( priv->label_entry )));

	ok = ofo_tva_record_insert( priv->tva_record );
	if( !ok ){
		*msgerr = g_strdup( _( "Unable to create this new VAT declaration" ));
	}

	if( ok ){
		/* activate the page */
		page = ofa_ipage_manager_activate( manager, OFA_TYPE_TVA_RECORD_PAGE );
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
		my_style_add( priv->msg_label, "labelerror");
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
