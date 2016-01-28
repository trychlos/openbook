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

#include "api/my-idialog.h"
#include "api/my-igridlist.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofo-dossier.h"
#include "api/ofo-account.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-ledger-combo.h"
#include "ui/ofa-ope-template-help.h"
#include "ui/ofa-ope-template-properties.h"

/* private instance data
 *
 * each line of the grid is :
 * - button 'Add' or line number
 * - comment
 * - account entry
 * - account btn selection
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
struct _ofaOpeTemplatePropertiesPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofoOpeTemplate      *ope_template;
	gchar               *ledger;			/* ledger mnemo */

	/* internals
	 */
	ofaHub              *hub;
	gboolean             is_current;
	gboolean             is_new;

	/* data
	 */
	gchar               *mnemo;
	gchar               *label;
	gboolean             ledger_locked;
	gchar               *ref;				/* piece reference */
	gboolean             ref_locked;
	gchar               *upd_user;
	GTimeVal             upd_stamp;

	/* UI
	 */
	ofaLedgerCombo      *ledger_combo;
	GtkWidget           *ledger_parent;
	GtkWidget           *ref_entry;
	GtkWidget           *details_grid;
	GtkWidget           *msgerr_label;
	ofaOpeTemplateHelp  *help_dlg;
	GtkWidget           *help_btn;
	GtkWidget           *ok_btn;
};

/* columns in the detail treeview
 */
enum {
	DET_COL_COMMENT = 0,
	DET_COL_ACCOUNT,
	DET_COL_ACCOUNT_SELECT,
	DET_COL_ACCOUNT_LOCKED,
	DET_COL_LABEL,
	DET_COL_LABEL_LOCKED,
	DET_COL_DEBIT,
	DET_COL_DEBIT_LOCKED,
	DET_COL_CREDIT,
	DET_COL_CREDIT_LOCKED,
	DET_N_COLUMNS
};

/* each widget of the grid brings its row number */
#define DATA_ROW                        "ofa-data-row"
#define DATA_COLUMN                     "ofa-data-column"

/* space between widgets in a detail line */
#define DETAIL_SPACE                    2

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ope-template-properties.ui";

static void      idialog_iface_init( myIDialogInterface *iface );
static guint     idialog_get_interface_version( const myIDialog *instance );
static gchar    *idialog_get_identifier( const myIDialog *instance );
static void      idialog_init( myIDialog *instance );
static void      init_dialog_title( ofaOpeTemplateProperties *self );
static void      init_mnemo( ofaOpeTemplateProperties *self );
static void      init_label( ofaOpeTemplateProperties *self );
static void      init_ledger( ofaOpeTemplateProperties *self );
static void      init_ledger_locked( ofaOpeTemplateProperties *self );
static void      init_ref( ofaOpeTemplateProperties *self );
static void      init_detail( ofaOpeTemplateProperties *self );
static void      igridlist_iface_init( myIGridListInterface *iface );
static guint     igridlist_get_interface_version( const myIGridList *instance );
static void      igridlist_set_row( const myIGridList *instance, GtkGrid *grid, guint row );
static void      set_detail_widgets( ofaOpeTemplateProperties *self, guint row );
static void      set_detail_values( ofaOpeTemplateProperties *self, guint row );
static void      on_mnemo_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaOpeTemplateProperties *self );
static void      on_ledger_changed( ofaLedgerCombo *combo, const gchar *mnemo, ofaOpeTemplateProperties *self );
static void      on_ledger_locked_toggled( GtkToggleButton *toggle, ofaOpeTemplateProperties *self );
static void      on_ref_locked_toggled( GtkToggleButton *toggle, ofaOpeTemplateProperties *self );
static void      on_account_selection( GtkButton *button, ofaOpeTemplateProperties *self );
static void      on_help_clicked( GtkButton *btn, ofaOpeTemplateProperties *self );
static void      on_help_closed( ofaOpeTemplateProperties *self, GObject *finalized_help );
static void      check_for_enable_dlg( ofaOpeTemplateProperties *self );
static gboolean  is_dialog_validable( ofaOpeTemplateProperties *self );
static void      on_ok_clicked( GtkButton *button, ofaOpeTemplateProperties *self );
static gboolean  do_update( ofaOpeTemplateProperties *self, gchar **msgerr );
static void      get_detail_list( ofaOpeTemplateProperties *self, gint row );
static void      set_msgerr( ofaOpeTemplateProperties *self, const gchar *msg );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateProperties, ofa_ope_template_properties, GTK_TYPE_DIALOG, 0, \
		G_ADD_PRIVATE( ofaOpeTemplateProperties ) \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ) \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IGRIDLIST, igridlist_iface_init ));

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

		/* close the help window */
		if( priv->help_dlg ){
			ofa_ope_template_help_close( priv->help_dlg );
		}
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

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_properties_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idialog_get_interface_version;
	iface->get_identifier = idialog_get_identifier;
	iface->init = idialog_init;
}

static guint
idialog_get_interface_version( const myIDialog *instance )
{
	return( 1 );
}

/*
 * identifier is built with class name and template mnemo
 */
static gchar *
idialog_get_identifier( const myIDialog *instance )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gchar *id;

	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ),
				ofo_ope_template_get_mnemo( priv->ope_template ));

	return( id );
}

static void
idialog_init( myIDialog *instance )
{
	ofaOpeTemplateProperties *self;
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkApplication *application;
	ofoDossier *dossier;
	GtkWidget *button;

	self = OFA_OPE_TEMPLATE_PROPERTIES( instance );
	priv = ofa_ope_template_properties_get_instance_private( self );

	main_window = my_idialog_get_main_window( instance );
	g_return_if_fail( main_window && GTK_IS_APPLICATION_WINDOW( main_window ));

	application = gtk_window_get_application( GTK_WINDOW( main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	priv->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	priv->is_current = ofo_dossier_is_current( dossier );
	//priv->is_current = FALSE;

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));
	g_signal_connect( priv->ok_btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );

	init_dialog_title( self );
	init_mnemo( self );
	init_label( self );
	init_ledger( self );
	init_ledger_locked( self );
	init_ref( self );

	my_utils_container_notes_init( instance, ope_template );
	my_utils_container_updstamp_init( instance, ope_template );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "help-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_help_clicked ), self );
	priv->help_btn = button;

	if( priv->is_current ){
		gtk_widget_grab_focus(
				my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "p1-mnemo-entry" ));
	}

	/* if not the current exercice, then only have a 'Close' button */
	my_utils_container_set_editable( GTK_CONTAINER( instance ), priv->is_current );
	if( !priv->is_current ){
		my_idialog_set_close_button( MY_IDIALOG( self ));
		priv->ok_btn = NULL;
	}

	/* init dialog detail rows after having globally set the fields
	 * sensitivity so that IGridList can individually adjust rows sensitivity */
	init_detail( self );

	check_for_enable_dlg( self );
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

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->ledger_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger-parent" );
	g_return_if_fail( priv->ledger_parent && GTK_IS_CONTAINER( priv->ledger_parent ));

	priv->ledger_combo = ofa_ledger_combo_new();
	gtk_container_add( GTK_CONTAINER( priv->ledger_parent ), GTK_WIDGET( priv->ledger_combo ));
	ofa_ledger_combo_set_columns( priv->ledger_combo, LEDGER_DISP_LABEL );
	ofa_ledger_combo_set_hub( priv->ledger_combo, priv->hub );

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
			MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ), priv->is_current, DET_N_COLUMNS );

	count = ofo_ope_template_get_detail_count( priv->ope_template );
	for( i=1 ; i<=count ; ++i ){
		my_igridlist_add_row( MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ));
	}
}

/*
 * myIGridList interface management
 */
static void
igridlist_iface_init( myIGridListInterface *iface )
{
	static const gchar *thisfn = "ofa_ofa_ope_template_properties_igridlist_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = igridlist_get_interface_version;
	iface->set_row = igridlist_set_row;
}

static guint
igridlist_get_interface_version( const myIGridList *instance )
{
	return( 1 );
}

static void
igridlist_set_row( const myIGridList *instance, GtkGrid *grid, guint row )
{
	ofaOpeTemplatePropertiesPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PROPERTIES( instance ));

	priv = ofa_ope_template_properties_get_instance_private( OFA_OPE_TEMPLATE_PROPERTIES( instance ));
	g_return_if_fail( grid == GTK_GRID( priv->details_grid ));

	set_detail_widgets( OFA_OPE_TEMPLATE_PROPERTIES( instance ), row );
	set_detail_values( OFA_OPE_TEMPLATE_PROPERTIES( instance ), row );
}

static void
set_detail_widgets( ofaOpeTemplateProperties *self, guint row )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *toggle, *button;
	GtkEntry *entry;

	priv = ofa_ope_template_properties_get_instance_private( self );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), 2*DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_grid_attach( GTK_GRID( priv->details_grid ), GTK_WIDGET( entry ), 1+DET_COL_COMMENT, row, 1, 1 );
	if( priv->is_current ){
		gtk_widget_grab_focus( GTK_WIDGET( entry ));
	}
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_current );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 20 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( GTK_GRID( priv->details_grid ), GTK_WIDGET( entry ), 1+DET_COL_ACCOUNT, row, 1, 1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_current );

	button = my_igridlist_add_button(
						MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ),
						"gtk-index", 1+DET_COL_ACCOUNT_SELECT, row, DETAIL_SPACE,
						G_CALLBACK( on_account_selection ), self );
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GUINT_TO_POINTER( row ));

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	gtk_grid_attach( GTK_GRID( priv->details_grid ), toggle, 1+DET_COL_ACCOUNT_LOCKED, row, 1, 1 );
	gtk_widget_set_sensitive( toggle, priv->is_current );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), TRUE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 20 );
	gtk_grid_attach( GTK_GRID( priv->details_grid ), GTK_WIDGET( entry ), 1+DET_COL_LABEL, row, 1, 1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_current );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	gtk_grid_attach( GTK_GRID( priv->details_grid ), toggle, 1+DET_COL_LABEL_LOCKED, row, 1, 1 );
	gtk_widget_set_sensitive( toggle, priv->is_current );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( GTK_GRID( priv->details_grid ), GTK_WIDGET( entry ), 1+DET_COL_DEBIT, row, 1, 1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_current );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	gtk_grid_attach( GTK_GRID( priv->details_grid ), toggle, 1+DET_COL_DEBIT_LOCKED, row, 1, 1 );
	gtk_widget_set_sensitive( toggle, priv->is_current );

	entry = GTK_ENTRY( gtk_entry_new());
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	my_utils_widget_set_margin_left( GTK_WIDGET( entry ), DETAIL_SPACE );
	gtk_entry_set_max_length( entry, 80 );
	gtk_entry_set_width_chars( entry, 10 );
	gtk_grid_attach( GTK_GRID( priv->details_grid ), GTK_WIDGET( entry ), 1+DET_COL_CREDIT, row, 1, 1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), priv->is_current );

	toggle = gtk_check_button_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	gtk_grid_attach( GTK_GRID( priv->details_grid ), toggle, 1+DET_COL_CREDIT_LOCKED, row, 1, 1 );
	gtk_widget_set_sensitive( toggle, priv->is_current );
}

static void
set_detail_values( ofaOpeTemplateProperties *self, guint row )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkEntry *entry;
	GtkToggleButton *toggle;
	const gchar *str;

	priv = ofa_ope_template_properties_get_instance_private( self );

	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_COMMENT, row ));
	str = ofo_ope_template_get_detail_comment( priv->ope_template, row-1 );
	gtk_entry_set_text( entry, str ? str : "" );

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

/**
 * ofa_ope_template_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @template: [allow-none]: a #ofoOpeTemplate to be edited.
 * @ledger: [allow-none]: a #ofoLedger to be attached to @template.
 *
 * Creates or represents a #ofaOpeTemplateProperties non-modal dialog
 * to edit the @template.
 */
void
ofa_ope_template_properties_run(
		const ofaMainWindow *main_window, ofoOpeTemplate *template, const gchar *ledger )
{
	static const gchar *thisfn = "ofa_ope_template_properties_run";
	ofaOpeTemplateProperties *self;
	ofaOpeTemplatePropertiesPrivate *priv;

	g_debug( "%s: main_window=%p, template=%p, ledger=%s",
			thisfn, ( void * ) main_window, ( void * ) template, ledger );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( !template || OFO_IS_OPE_TEMPLATE( template ));

	self = g_object_new( OFA_TYPE_OPE_TEMPLATE_PROPERTIES, NULL );
	my_idialog_set_main_window( MY_IDIALOG( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_ope_template_properties_get_instance_private( self );
	priv->ope_template = template;
	priv->ledger = g_strdup( ledger );

	/* after this call, @self may be invalid */
	my_idialog_present( MY_IDIALOG( self ));
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

static void
on_account_selection( GtkButton *button, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	GtkEntry *entry;
	gchar *number;
	guint row;

	priv = ofa_ope_template_properties_get_instance_private( self );

	main_window = my_idialog_get_main_window( MY_IDIALOG( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	entry = GTK_ENTRY( gtk_grid_get_child_at( GTK_GRID( priv->details_grid ), 1+DET_COL_ACCOUNT, row ));
	number = ofa_account_select_run(
					OFA_MAIN_WINDOW( main_window ),
					gtk_entry_get_text( entry ),
					ACCOUNT_ALLOW_DETAIL );
	if( my_strlen( number )){
		gtk_entry_set_text( entry, number );
	}
	g_free( number );
}

static void
on_help_clicked( GtkButton *btn, ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkApplicationWindow *main_window;

	priv = ofa_ope_template_properties_get_instance_private( self );

	main_window = my_idialog_get_main_window( MY_IDIALOG( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv->help_dlg = ofa_ope_template_help_run( OFA_MAIN_WINDOW( main_window ));

	gtk_widget_set_sensitive( GTK_WIDGET( btn ), FALSE );
	g_object_weak_ref( G_OBJECT( priv->help_dlg ), ( GWeakNotify ) on_help_closed, self );
}

static void
on_help_closed( ofaOpeTemplateProperties *self, GObject *finalized_help )
{
	static const gchar *thisfn = "ofa_ope_template_properties_on_help_closed";
	ofaOpeTemplatePropertiesPrivate *priv;

	g_debug( "%s: self=%p, finalized_help=%p",
			thisfn, ( void * ) self, ( void * ) finalized_help );

	priv = ofa_ope_template_properties_get_instance_private( self );

	priv->help_dlg = NULL;
	gtk_widget_set_sensitive( priv->help_btn, TRUE );
}

/*
 * we accept to save uncomplete detail lines
 */
static void
check_for_enable_dlg( ofaOpeTemplateProperties *self )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gboolean ok;

	priv = ofa_ope_template_properties_get_instance_private( self );

	ok = is_dialog_validable( self );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, ok );
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

	priv = ofa_ope_template_properties_get_instance_private( self );

	msgerr = NULL;
	set_msgerr( self, "" );

	ok = ofo_ope_template_is_valid( priv->mnemo, priv->label, priv->ledger, &msgerr );
	if( ok ){
		exists = ofo_ope_template_get_by_mnemo( priv->hub, priv->mnemo );
		ok = !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_ope_template_get_mnemo( priv->ope_template )));
		if( !ok ){
			msgerr = g_strdup_printf( _( "Operation template '%s' already exists" ), priv->mnemo );
		}
	}

	set_msgerr( self, msgerr );
	g_free( msgerr );

	return( ok );
}

static void
on_ok_clicked( GtkButton *button, ofaOpeTemplateProperties *self )
{
	gboolean ok;
	gchar *msgerr;

	msgerr = NULL;
	ok = do_update( self, &msgerr );

	if( ok ){
		my_idialog_close( MY_IDIALOG( self ));

	} else {
		my_utils_dialog_warning( msgerr );
		g_free( msgerr );
	}
}

static gboolean
do_update( ofaOpeTemplateProperties *self, gchar **msgerr )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	gchar *prev_mnemo;
	guint i, count;
	gboolean ok;

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
	count = my_igridlist_get_rows_count( MY_IGRIDLIST( self ), GTK_GRID( priv->details_grid ));
	for( i=1 ; i<=count ; ++i ){
		get_detail_list( self, i );
	}

	if( !prev_mnemo ){
		ok = ofo_ope_template_insert( priv->ope_template, priv->hub );
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
get_detail_list( ofaOpeTemplateProperties *self, gint row )
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

	ofo_ope_template_add_detail( priv->ope_template,
				comment,
				account, account_locked,
				label, label_locked,
				debit, debit_locked,
				credit, credit_locked );
}

static void
set_msgerr( ofaOpeTemplateProperties *self, const gchar *msg )
{
	ofaOpeTemplatePropertiesPrivate *priv;
	GtkWidget *label;

	priv = ofa_ope_template_properties_get_instance_private( self );

	if( !priv->msgerr_label ){
		label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "px-msgerr" );
		g_return_if_fail( label && GTK_IS_LABEL( label ));
		my_utils_widget_set_style( label, "labelerror" );
		priv->msgerr_label = label;
	}

	gtk_label_set_text( GTK_LABEL( priv->msgerr_label ), msg ? msg : "" );
}
