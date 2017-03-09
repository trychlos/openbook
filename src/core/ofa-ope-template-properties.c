/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "my/my-idialog.h"
#include "my/my-igridlist.h"
#include "my/my-iwindow.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-paimean-editable.h"
#include "api/ofo-dossier.h"
#include "api/ofo-account.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-ledger-combo.h"
#include "core/ofa-ledger-store.h"
#include "core/ofa-ope-template-help.h"
#include "core/ofa-ope-template-properties.h"

/* private instance data
 *
 * each line of the grid is :
 * - button 'Add' or line number
 * - comment
 * - pam target
 * - account entry
 * - account locked
 * - label entry
 * - label locked
 * - debit entry
 * - debit locked
 * - credit entry
 * - credit locked
 * - button up
 * - button down
 * - button remove
 */
typedef struct {
	gboolean        dispose_has_run;

	/* initialization
	 */
	ofaIGetter     *getter;
	GtkWindow      *parent;
	ofoOpeTemplate *ope_template;
	gchar          *ledger;				/* ledger mnemo */

	/* runtime
	 */
	gboolean        is_writable;
	gboolean        is_new;

	/* data
	 */
	gchar          *mnemo;
	gchar          *label;
	gboolean        ledger_locked;
	gchar          *ref;				/* piece reference */
	gboolean        ref_locked;
	gchar          *upd_user;
	GTimeVal        upd_stamp;

	/* UI
	 */
	ofaLedgerCombo *ledger_combo;
	GtkWidget      *ledger_parent;
	GtkWidget      *ref_entry;
	GtkWidget      *details_grid;
	GtkWidget      *msg_label;
	GtkWidget      *ok_btn;
}
	ofaOpeTemplatePropertiesPrivate;

/* columns in the detail treeview
 */
enum {
	DET_COL_COMMENT = 0,
	DET_COL_PAM,
	DET_COL_ACCOUNT,
	DET_COL_ACCOUNT_LOCKED,
	DET_COL_LABEL,
	DET_COL_LABEL_LOCKED,
	DET_COL_DEBIT,
	DET_COL_DEBIT_LOCKED,
	DET_COL_CREDIT,
	DET_COL_CREDIT_LOCKED,
	DET_N_COLUMNS
};

/* horizontal space between widgets in a detail line */
#define DETAIL_SPACE                    0

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-ope-template-properties.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     idialog_init( myIDialog *instance );
static void     init_dialog_title( ofaOpeTemplateProperties *self );
static void     init_mnemo( ofaOpeTemplateProperties *self );
static void     init_label( ofaOpeTemplateProperties *self );
static void     init_ledger( ofaOpeTemplateProperties *self );
static void     init_ledger_locked( ofaOpeTemplateProperties *self );
static void     init_ref( ofaOpeTemplateProperties *self );
static void     init_detail( ofaOpeTemplateProperties *self );
static void     igridlist_iface_init( myIGridlistInterface *iface );
static guint    igridlist_get_interface_version( void );
static void     igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *empty );
static void     setup_detail_widgets( ofaOpeTemplateProperties *self, guint row );
static void     set_detail_values( ofaOpeTemplateProperties *self, guint row );
static void     on_mnemo_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void     on_label_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void     on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaOpeTemplateProperties *self );
static void     on_ledger_locked_toggled( GtkToggleButton *toggle, ofaOpeTemplateProperties *self );
static void     on_ref_locked_toggled( GtkToggleButton *toggle, ofaOpeTemplateProperties *self );
static void     on_pam_toggled( GtkToggleButton *btn, ofaOpeTemplateProperties *self );
static void     on_account_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void     on_help_clicked( GtkButton *btn, ofaOpeTemplateProperties *self );
static void     check_for_enable_dlg( ofaOpeTemplateProperties *self );
static gboolean is_dialog_validable( ofaOpeTemplateProperties *self );
static void     on_ok_clicked( ofaOpeTemplateProperties *self );
static gboolean do_update( ofaOpeTemplateProperties *self, gchar **msgerr );
static void     get_detail_list( ofaOpeTemplateProperties *self, gint row, gboolean update );
static void     set_msgerr( ofaOpeTemplateProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateProperties, ofa_ope_template_properties, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaOpeTemplateProperties )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ))

static void
ope_template_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_properties_finalize";
	ofaOpeTemplatePropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PROPERTIES( instance ));

	/* free data members here */
	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->ledger );
	g_free( priv->ref );
	g_free( priv->upd_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_properties_parent_class )->finalize( instance );
}

static void
ope_template_properties_dispose( GObject *instance )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PROPERTIES( instance ));

	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_properties_parent_class )->dispose( instance );
}

static void
ofa_ope_template_properties_init( ofaOpeTemplateProperties *self )
{
	static const gchar *thisfn = "ofa_ope_template_properties_init";
	ofaOpeTemplatePropertiesPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_PROPERTIES( self ));

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->is_new = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_ope_template_properties_class_init( ofaOpeTemplatePropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_properties_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_ope_template_properties_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent of this dialog.
 * @template: [allow-none]: a #ofoOpeTemplate to be edited.
 * @ledger: [allow-none]: a #ofoLedger to be attached to @template.
 *
 * Creates or represents a #ofaOpeTemplateProperties non-modal dialog
 * to edit the @template.
 */
void
ofa_ope_template_properties_run(
		ofaIGetter *getter, GtkWindow *parent, ofoOpeTemplate *template, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_template_properties_run";
	ofaOpeTemplateProperties *self;
	ofaOpeTemplatePropertiesPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, template=%p, ledger=%s",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) template, ledger );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( !template || OFO_IS_OPE_TEMPLATE( template ));

	self = g_object_new( OFA_TYPE_OPE_TEMPLATE_PROPERTIES, NULL );

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->ope_template = template;
	priv->ledger = g_strdup( ledger );

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_properties_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_ope_template_properties_iwindow_init";
	ofaOpeTemplatePropertiesPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ), ofo_ope_template_get_mnemo( priv->ope_template ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_ope_template_properties_idialog_init";
	ofaOpeTemplatePropertiesPrivate *priv;
	ofaHub *hub;
	GtkWidget *btn;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));

	/* validate and record the properties on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	hub = ofa_igetter_get_hub( priv->getter );
	priv->is_writable = ofa_hub_is_writable_dossier( hub );

	init_dialog_title( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	init_mnemo( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	init_label( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	init_ledger( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	init_ledger_locked( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	init_ref( OFA_OPE_TEMPLATE_PROPERTIES( instance ));

	my_utils_container_notes_init( instance, ope_template );
	my_utils_container_updstamp_init( instance, ope_template );

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "help-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect( btn, "clicked", G_CALLBACK( on_help_clicked ), instance );

	if( priv->is_writable ){
		gtk_widget_grab_focus(
				my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-entry" ));
	}

	/* if not the current exercice, then only have a 'Close' btn */
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_writable );
	if( !priv->is_writable ){
		my_idialog_set_close_button( instance );
		priv->ok_btn = NULL;
	}

	/* init dialog detail rows after having globally set the fields
	 * sensitivity so that IGridList can individually adjust rows sensitivity */
	init_detail( OFA_OPE_TEMPLATE_PROPERTIES( instance ));

	gtk_widget_show_all( GTK_WIDGET( instance ));

	check_for_enable_dlg( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
}

static void
init_dialog_title( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	const gchar *mnemo;
	gchar *title;

	priv = ofa_ope_template_properties_get_instance_private( self );

	mnemo = ofo_ope_template_get_mnemo( priv->ope_template );
	priv->is_new = !my_strlen( mnemo );

	if( priv->is_new ){
		title = g_strdup( _( "Defining a new operation template" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » operation template" ), mnemo );
	}

	gtk_window_set_title( GTK_WINDOW( self ), title );
	g_free( title );
}

static void
init_mnemo( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->mnemo = g_strdup( ofo_ope_template_get_mnemo( priv->ope_template ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	if( priv->mnemo ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->mnemo );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_mnemo_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));
}

static void
init_label( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->label = g_strdup( ofo_ope_template_get_label( priv->ope_template ));
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	if( priv->label ){
		gtk_entry_set_text( GTK_ENTRY( entry ), priv->label );
	}
	g_signal_connect( entry, "changed", G_CALLBACK( on_label_changed ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));
}

static void
init_ledger( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *label;
	static const gint st_ledger_cols[] = {
			LEDGER_COL_LABEL,
			-1 };

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->ledger_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-parent" );
	g_return_if_fail( priv->ledger_parent && GTK_IS_CONTAINER( priv->ledger_parent ));

	priv->ledger_combo = ofa_ledger_combo_new();
	gtk_container_add( GTK_CONTAINER( priv->ledger_parent ), GTK_WIDGET( priv->ledger_combo ));
	ofa_ledger_combo_set_columns( priv->ledger_combo, st_ledger_cols );
	ofa_ledger_combo_set_getter( priv->ledger_combo, priv->getter );

	g_signal_connect( priv->ledger_combo, "ofa-changed", G_CALLBACK( on_ledger_changed ), self );

	ofa_ledger_combo_set_selected( priv->ledger_combo,
			priv->is_new ? priv->ledger : ofo_ope_template_get_ledger( priv->ope_template ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( priv->ledger_combo ));
}

static void
init_ledger_locked( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *btn;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->ledger_locked = ofo_ope_template_get_ledger_locked( priv->ope_template );
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-jou-locked" );
	g_return_if_fail( btn && GTK_IS_TOGGLE_BUTTON( btn ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), priv->ledger_locked );

	g_signal_connect( btn, "toggled", G_CALLBACK( on_ledger_locked_toggled ), self );
}

static void
init_ref( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *btn, *label;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->ref = g_strdup( ofo_ope_template_get_ref( priv->ope_template ));
	priv->ref_locked = ofo_ope_template_get_ref_locked( priv->ope_template );

	priv->ref_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ref-entry" );
	g_return_if_fail( priv->ref_entry && GTK_IS_ENTRY( priv->ref_entry ));
	ofa_paimean_editable_init( GTK_EDITABLE( priv->ref_entry ), priv->getter );

	if( priv->ref ){
		gtk_entry_set_text( GTK_ENTRY( priv->ref_entry ), priv->ref );
	}

	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ref-locked" );
	g_return_if_fail( btn && GTK_IS_TOGGLE_BUTTON( btn ));

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( btn ), priv->ref_locked );

	g_signal_connect( btn, "toggled", G_CALLBACK( on_ref_locked_toggled ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ref-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->ref_entry );
}

/*
 * add one line per detail record
 */
static void
init_detail( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gint count, i;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->details_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-details" );
	g_return_if_fail( priv->details_grid && GTK_IS_GRID( priv->details_grid ));

	my_igridlist_init(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			TRUE, priv->is_writable, DET_N_COLUMNS );

	count = ofo_ope_template_get_detail_count( priv->ope_template );
	for( i=1 ; i<=count ; ++i ){
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ), NULL );
	}
}

/*
 * myIGridlist interface management
 */
static void
igridlist_iface_init( myIGridlistInterface *iface )
{
	static const gchar *thisfn = "ofa_ofa_ope_template_properties_igridlist_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = igridlist_get_interface_version;
	iface->setup_row = igridlist_setup_row;
}

static guint
igridlist_get_interface_version( void )
{
	return( 1 );
}

static void
igridlist_setup_row( const myIGridlist *instance, GtkGrid *grid, guint row, void *empty )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PROPERTIES( instance ));

	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	g_return_if_fail( grid == GTK_GRID( priv->details_grid ));

	setup_detail_widgets( OFA_OPE_TEMPLATE_PROPERTIES( instance ), row );
	set_detail_values( OFA_OPE_TEMPLATE_PROPERTIES( instance ), row );
}

static void
setup_detail_widgets( ofaOpeTemplateProperties *self, guint row )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *toggle;
	GtkEntry *entry;

	priv = ofa_ope_template_properties_get_instance_private( self );

	/* ope template detail comment */
	entry = GTK_ENTRY( gtk_entry_new());
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_widget_set_halign( GTK_WIDGET( entry ), GTK_ALIGN_START );
	gtk_entry_set_alignment( entry, 0 );
	gtk_entry_set_max_length( entry, OTE_DET_COMMENT_MAX_LENGTH );
	gtk_entry_set_max_width_chars( entry, OTE_DET_COMMENT_MAX_LENGTH );
	if( priv->is_writable ){
		gtk_widget_grab_focus( GTK_WIDGET( entry ));
	}
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			GTK_WIDGET( entry ), 1+DET_COL_COMMENT, row, 1, 1 );

	/* mean of paiement target */
	toggle = gtk_check_button_new();
	my_utils_widget_set_margin_left( toggle, DETAIL_SPACE );
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	gtk_widget_set_halign( toggle, GTK_ALIGN_CENTER );
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_pam_toggled ), self );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			toggle, 1+DET_COL_PAM, row, 1, 1 );

	/* account identifier */
	entry = GTK_ENTRY( gtk_entry_new());
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_writable );
	ofa_account_editable_init( GTK_EDITABLE( entry ), priv->getter, ACCOUNT_ALLOW_ALL );
	g_signal_connect( entry, "changed", G_CALLBACK( on_account_changed ), self );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			GTK_WIDGET( entry ), 1+DET_COL_ACCOUNT, row, 1, 1 );

	/* account locked */
	toggle = gtk_check_button_new();
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			toggle, 1+DET_COL_ACCOUNT_LOCKED, row, 1, 1 );

	/* label */
	entry = GTK_ENTRY( gtk_entry_new());
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), TRUE );
	gtk_entry_set_max_length( entry, OTE_DET_LABEL_MAX_LENGTH );
	gtk_entry_set_max_width_chars( entry, OTE_DET_LABEL_MAX_LENGTH );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			GTK_WIDGET( entry ), 1+DET_COL_LABEL, row, 1, 1 );

	/* label locked */
	toggle = gtk_check_button_new();
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			toggle, 1+DET_COL_LABEL_LOCKED, row, 1, 1 );

	/* debit */
	entry = GTK_ENTRY( gtk_entry_new());
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, OTE_DET_AMOUNT_MAX_LENGTH );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_entry_set_max_width_chars( entry, OTE_DET_AMOUNT_MAX_LENGTH );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			GTK_WIDGET( entry ), 1+DET_COL_DEBIT, row, 1, 1 );

	/* debit locked */
	toggle = gtk_check_button_new();
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			toggle, 1+DET_COL_DEBIT_LOCKED, row, 1, 1 );

	/* credit */
	entry = GTK_ENTRY( gtk_entry_new());
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, OTE_DET_AMOUNT_MAX_LENGTH );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_entry_set_max_width_chars( entry, OTE_DET_AMOUNT_MAX_LENGTH );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			GTK_WIDGET( entry ), 1+DET_COL_CREDIT, row, 1, 1 );

	/* credit locked */
	toggle = gtk_check_button_new();
	gtk_widget_set_sensitive( toggle, priv->is_writable );
	my_igridlist_set_widget(
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
			toggle, 1+DET_COL_CREDIT_LOCKED, row, 1, 1 );
}

static void
set_detail_values( ofaOpeTemplateProperties *self, guint row )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *str;
	gint pam_row;

	priv = ofa_ope_template_properties_get_instance_private( self );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_COMMENT, row ));
	str = ofo_ope_template_get_detail_comment( priv->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	pam_row = ofo_ope_template_get_pam_row( priv->ope_template );
	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_PAM, row ));
	gtk_toggle_button_set_active( toggle, 1+pam_row == row );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_ACCOUNT, row ));
	str = ofo_ope_template_get_detail_account( priv->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_ACCOUNT_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_account_locked( priv->ope_template, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_LABEL, row ));
	str = ofo_ope_template_get_detail_label( priv->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_LABEL_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_label_locked( priv->ope_template, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_DEBIT, row ));
	str = ofo_ope_template_get_detail_debit( priv->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_DEBIT_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_debit_locked( priv->ope_template, row-1 ));

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_CREDIT, row ));
	str = ofo_ope_template_get_detail_credit( priv->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_CREDIT_LOCKED, row ));
	gtk_toggle_button_set_active( toggle, ofo_ope_template_get_detail_credit_locked( priv->ope_template, row-1 ));
}

static void
on_mnemo_changed( GtkEntry *entry, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	g_free( priv->ledger );
	priv->ledger = g_strdup( mnemo );

	check_for_enable_dlg( self );
}

static void
on_ledger_locked_toggled( GtkToggleButton *btn, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->ledger_locked = gtk_toggle_button_get_active( btn );

	/* doesn't change the validable status of the dialog */
}

static void
on_ref_locked_toggled( GtkToggleButton *btn, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->ref_locked = gtk_toggle_button_get_active( btn );

	/* doesn't change the validable status of the dialog */
}

/*
 * at most one row may be the target of a mean of paiement
 */
static void
on_pam_toggled( GtkToggleButton *btn, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gint count, i;
	GtkWidget *row_btn;

	priv = ofa_ope_template_properties_get_instance_private( self );

	count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ));

	if( gtk_toggle_button_get_active( btn )){
		for( i=1 ; i<=count ; ++i ){
			row_btn = gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_PAM, i );
			if( row_btn != GTK_WIDGET( btn )){
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( row_btn ), FALSE );
			}
		}
	}
}

static void
on_account_changed( GtkEntry *entry, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	const gchar *number, *label;
	ofoAccount *account;

	priv = ofa_ope_template_properties_get_instance_private( self );

	number = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( priv->getter, number );
	if( account && OFO_IS_ACCOUNT( account )){
		label = ofo_account_get_label( account );
		gtk_widget_set_tooltip_text( GTK_WIDGET( entry ), label );
	}
}

static void
on_help_clicked( GtkButton *btn, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	ofa_ope_template_help_run( priv->getter, GTK_WINDOW( self ));
}

/*
 * we accept to save uncomplete detail lines
 */
static void
check_for_enable_dlg( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	priv = ofa_ope_template_properties_get_instance_private( self );

	if( priv->is_writable ){
		gtk_widget_set_sensitive( priv->ok_btn, is_dialog_validable( self ));
	}
}

/*
 * we accept to save uncomplete detail lines
 */
static gboolean
is_dialog_validable( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	ofoOpeTemplate *exists;
	gboolean ok;
	gchar *msgerr;
	guint i, count, pam_count;
	GtkWidget *toggle;

	priv = ofa_ope_template_properties_get_instance_private( self );

	msgerr = NULL;

	ok = ofo_ope_template_is_valid_data( priv->mnemo, priv->label, priv->ledger, &msgerr );

	if( ok ){
		exists = ofo_ope_template_get_by_mnemo( priv->getter, priv->mnemo );
		ok = !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_ope_template_get_mnemo( priv->ope_template )));
		if( !ok ){
			msgerr = g_strdup_printf( _( "Operation template '%s' already exists" ), priv->mnemo );
		}
	}

	/* make sure that we have at most one mean of paiement target */
	if( ok && priv->details_grid ){
		count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ));
		pam_count = 0;
		for( i=1 ; i<=count ; ++i ){
			get_detail_list( self, i, FALSE );
			/* get target of mean of paiment */
			toggle = gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_PAM, i );
			if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ))){
				pam_count += 1;
			}
		}
		if( pam_count > 1 ){
			msgerr = g_strdup( "PROGRAM ERROR: more than one mean of paiement target" );
			ok = FALSE;
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static void
on_ok_clicked( ofaOpeTemplateProperties *self )
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
do_update( ofaOpeTemplateProperties *self, gchar **msgerr )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gchar *prev_mnemo;
	guint i, count;
	gint pam_row;
	gboolean ok;
	GtkWidget *toggle;

	priv = ofa_ope_template_properties_get_instance_private( self );

	prev_mnemo = g_strdup( ofo_ope_template_get_mnemo( priv->ope_template ));
	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même model (n'a pas été modifié)
	 */
	ofo_ope_template_set_mnemo( priv->ope_template, priv->mnemo );
	ofo_ope_template_set_label( priv->ope_template, priv->label );
	ofo_ope_template_set_ledger( priv->ope_template, priv->ledger );
	ofo_ope_template_set_ledger_locked( priv->ope_template, priv->ledger_locked );
	ofo_ope_template_set_ref( priv->ope_template, gtk_entry_get_text( GTK_ENTRY( priv->ref_entry )));
	ofo_ope_template_set_ref_locked( priv->ope_template, priv->ref_locked );
	my_utils_container_notes_get( GTK_WINDOW( self ), ope_template );

	ofo_ope_template_free_detail_all( priv->ope_template );
	count = my_igridlist_get_details_count( MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ));
	pam_row = -1;
	for( i=1 ; i<=count ; ++i ){
		get_detail_list( self, i, TRUE );
		/* get target of mean of paiment */
		toggle = gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_PAM, i );
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggle ))){
			pam_row = i-1;
		}
	}
	ofo_ope_template_set_pam_row( priv->ope_template, pam_row );

	if( !prev_mnemo ){
		ok = ofo_ope_template_insert( priv->ope_template );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to create this new operation template" ));
		}
	} else {
		ok = ofo_ope_template_update( priv->ope_template, prev_mnemo );
		if( !ok ){
			*msgerr = g_strdup( _( "Unable to update the operation template" ));
		}
	}

	g_free( prev_mnemo );

	return( ok );
}

static void
get_detail_list( ofaOpeTemplateProperties *self, gint row, gboolean update )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *comment, *account, *label, *debit, *credit;
	gboolean account_locked, label_locked, debit_locked, credit_locked;

	priv = ofa_ope_template_properties_get_instance_private( self );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_COMMENT, row ));
	comment = gtk_entry_get_text( entry );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_ACCOUNT, row ));
	account = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_ACCOUNT_LOCKED, row ));
	account_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_LABEL, row ));
	label = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_LABEL_LOCKED, row ));
	label_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_DEBIT, row ));
	debit = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_DEBIT_LOCKED, row ));
	debit_locked = gtk_toggle_button_get_active( toggle );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_CREDIT, row ));
	credit = gtk_entry_get_text( entry );

	toggle = GTK_TOGGLE_BUTTON( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_CREDIT_LOCKED, row ));
	credit_locked = gtk_toggle_button_get_active( toggle );

	if( update ){
		ofo_ope_template_add_detail( priv->ope_template,
					comment,
					account, account_locked,
					label, label_locked,
					debit, debit_locked,
					credit, credit_locked );
	}
}

static void
set_msgerr( ofaOpeTemplateProperties *self, const gchar *msg )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_ope_template_properties_get_instance_private( self );

	if( !priv->msg_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_style_add( label, "labelerror" );
		priv->msg_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msg_label ), msg ? msg : "" );
}
