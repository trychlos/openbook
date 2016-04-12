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
#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ui/ofa-check-balances.h"
#include "ui/ofa-check-integrity.h"
#include "ui/ofa-ledger-close.h"
#include "ui/ofa-ledger-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	ofaIGetter         *getter;
	ofaHub             *hub;
	GList              *hub_handlers;
	gboolean            done;			/* whether we have actually done something */
	GDate               closing;

	/* UI
	 */
	ofaLedgerTreeview *tview;
	GtkWidget          *do_close_btn;
	GtkWidget          *message_label;
	GtkWidget          *closing_entry;
	GtkWidget          *all_ledgers_btn;

	/* during the iteration on each selected ledger
	 */
	gboolean            all_ledgers;
	gint                count;			/* count of ledgers */
	gint                uncloseable;
	guint               entries_count;	/* count of validated entries for the ledger */
	guint               entries_num;
	myProgressBar      *bar;
}
	ofaPeriodClosePrivate;

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-ledger-close.ui";
static const gchar  *st_settings        = "PeriodClose";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      setup_ledgers_treeview( ofaPeriodClose *self );
static void      setup_date( ofaPeriodClose *self );
static void      setup_others( ofaPeriodClose *self );
static void      on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaPeriodClose *self );
static void      on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaPeriodClose *self );
static void      on_all_ledgers_toggled( GtkToggleButton *button, ofaPeriodClose *self );
static void      on_date_changed( GtkEditable *entry, ofaPeriodClose *self );
static void      check_for_enable_dlg( ofaPeriodClose *self, GList *selected );
static gboolean  is_dialog_validable( ofaPeriodClose *self, GList *selected );
static void      check_foreach_ledger( ofaPeriodClose *self, const gchar *ledger );
static void      on_ok_clicked( GtkButton *button, ofaPeriodClose *self );
static gboolean  do_close( ofaPeriodClose *self );
static void      prepare_grid( ofaPeriodClose *self, const gchar *mnemo, GtkWidget *grid );
static gboolean  close_foreach_ledger( ofaPeriodClose *self, const gchar *mnemo, GtkWidget *grid );
static void      do_end_close( ofaPeriodClose *self );
static void      on_hub_entry_status_count( ofaHub *hub, ofaEntryStatus new_status, guint count, ofaPeriodClose *self );
static void      on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaPeriodClose *self );
static void      load_settings( ofaPeriodClose *self );
static void      set_settings( ofaPeriodClose *self );

G_DEFINE_TYPE_EXTENDED( ofaPeriodClose, ofa_period_close, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPeriodClose )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
period_close_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_period_close_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PERIOD_CLOSE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_period_close_parent_class )->finalize( instance );
}

static void
period_close_dispose( GObject *instance )
{
	ofaPeriodClosePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PERIOD_CLOSE( instance ));

	priv = ofa_period_close_get_instance_private( OFA_PERIOD_CLOSE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		ofa_hub_disconnect_handlers( priv->hub, &priv->hub_handlers );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_period_close_parent_class )->dispose( instance );
}

static void
ofa_period_close_init( ofaPeriodClose *self )
{
	static const gchar *thisfn = "ofa_period_close_init";
	ofaPeriodClosePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PERIOD_CLOSE( self ));

	priv = ofa_period_close_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	my_date_clear( &priv->closing );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_period_close_class_init( ofaPeriodCloseClass *klass )
{
	static const gchar *thisfn = "ofa_period_close_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = period_close_dispose;
	G_OBJECT_CLASS( klass )->finalize = period_close_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_period_close_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Run an intermediate closing on selected ledgers
 */
void
ofa_period_close_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_period_close_run";
	ofaPeriodClose *self;
	ofaPeriodClosePrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_PERIOD_CLOSE, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_period_close_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_period_close_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_period_close_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * this dialog is subject to 'is_current' property
 * so first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_period_close_idialog_init";
	ofaPeriodClosePrivate *priv;
	gulong handler;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_period_close_get_instance_private( OFA_PERIOD_CLOSE( instance ));

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	setup_ledgers_treeview( OFA_PERIOD_CLOSE( instance ));
	setup_date( OFA_PERIOD_CLOSE( instance ));
	setup_others( OFA_PERIOD_CLOSE( instance ));

	handler = g_signal_connect(
					priv->hub,
					SIGNAL_HUB_STATUS_COUNT,
					G_CALLBACK( on_hub_entry_status_count ),
					instance );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					priv->hub,
					SIGNAL_HUB_STATUS_CHANGE,
					G_CALLBACK( on_hub_entry_status_change ),
					instance );
	priv->hub_handlers = g_list_prepend( priv->hub_handlers, ( gpointer ) handler );

	load_settings( OFA_PERIOD_CLOSE( instance ));
}

static void
setup_ledgers_treeview( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *tview_parent, *label;

	priv = ofa_period_close_get_instance_private( self );

	tview_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-treeview-parent" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));
	ofa_ledger_treeview_set_columns( priv->tview,
			PERIOD_DISP_MNEMO | PERIOD_DISP_LABEL | PERIOD_DISP_LAST_ENTRY | PERIOD_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_hub( priv->tview, priv->hub);
	ofa_ledger_treeview_set_selection_mode( priv->tview, GTK_SELECTION_MULTIPLE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-frame-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), ofa_ledger_treeview_get_treeview( priv->tview ));

	g_signal_connect( G_OBJECT( priv->tview ), "ofa-changed", G_CALLBACK( on_rows_selected ), self );
	g_signal_connect( G_OBJECT( priv->tview ), "ofa-activated", G_CALLBACK( on_rows_activated ), self );
}

static void
setup_date( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *label;

	priv = ofa_period_close_get_instance_private( self );

	priv->closing_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-date" );
	g_return_if_fail( priv->closing_entry && GTK_IS_ENTRY( priv->closing_entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-frame-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), priv->closing_entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p2-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( priv->closing_entry ));
	my_date_editable_set_format( GTK_EDITABLE( priv->closing_entry ), ofa_prefs_date_display());
	my_date_editable_set_label( GTK_EDITABLE( priv->closing_entry ), label, ofa_prefs_date_check());

	g_signal_connect( priv->closing_entry, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *button, *label;

	priv = ofa_period_close_get_instance_private( self );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "all-ledgers-btn" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_all_ledgers_toggled ), self );
	priv->all_ledgers_btn = button;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "btn-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( button, "clicked", G_CALLBACK( on_ok_clicked ), self );
	priv->do_close_btn = button;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;
	my_utils_widget_set_style( label, "labelerror" );
}

/*
 * LedgerTreeview callback
 */
static void
on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaPeriodClose *self )
{
	if( is_dialog_validable( self, selected )){
		do_close( self );
	}
}

/*
 * LedgerTreeview callback
 */
static void
on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaPeriodClose *self )
{
	check_for_enable_dlg( self, selected );
}

static void
on_all_ledgers_toggled( GtkToggleButton *button, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkTreeSelection *selection;

	priv = ofa_period_close_get_instance_private( self );

	priv->all_ledgers = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( GTK_WIDGET( priv->tview ), !priv->all_ledgers );

	if( priv->all_ledgers ){
		selection = ofa_ledger_treeview_get_selection( priv->tview );
		gtk_tree_selection_select_all( selection );
	}
}

static void
on_date_changed( GtkEditable *entry, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;

	priv = ofa_period_close_get_instance_private( self );

	my_date_set_from_date( &priv->closing,
			my_date_editable_get_date( GTK_EDITABLE( priv->closing_entry ), NULL ));

	check_for_enable_dlg( self, NULL );
}

static void
check_for_enable_dlg( ofaPeriodClose *self, GList *selected )
{
	ofaPeriodClosePrivate *priv;
	gboolean ok;

	priv = ofa_period_close_get_instance_private( self );

	ok = is_dialog_validable( self, selected );

	gtk_widget_set_sensitive( priv->do_close_btn, ok );

	if( ok ){
		set_settings( self );
	}
}

/*
 * the closing date is valid:
 * - if it is itself valid
 * - greater or equal to the begin of the exercice (if set)
 * - stricly lesser than the end of the exercice (if set)
 * - greater or equal than all selected ledger closing dates (if set)
 */
static gboolean
is_dialog_validable( ofaPeriodClose *self, GList *selected )
{
	ofaPeriodClosePrivate *priv;
	ofoDossier *dossier;
	GList *it;
	gboolean ok;
	const GDate *exe_begin, *exe_end;
	gboolean selected_here;

	priv = ofa_period_close_get_instance_private( self );

	ok = FALSE;
	gtk_label_set_text( GTK_LABEL( priv->message_label ), "" );
	dossier = ofa_hub_get_dossier( priv->hub );

	/* do we have a intrinsically valid proposed closing date
	 * + compare it to the limits of the exercice */
	if( !my_date_is_valid( &priv->closing )){
		gtk_label_set_text( GTK_LABEL( priv->message_label ), _( "Invalid closing date" ));

	} else {
		exe_begin = ofo_dossier_get_exe_begin( dossier );
		if( my_date_is_valid( exe_begin ) && my_date_compare( &priv->closing, exe_begin ) < 0 ){
			gtk_label_set_text( GTK_LABEL( priv->message_label ),
					_( "Closing date must be greater or equal to the beginning of exercice" ));

		} else {
			exe_end = ofo_dossier_get_exe_end( dossier );
			if( my_date_is_valid( exe_end ) && my_date_compare( &priv->closing, exe_end ) >= 0 ){
				gtk_label_set_text( GTK_LABEL( priv->message_label ),
						_( "Closing date must be lesser than the end of exercice" ));

			} else {
				ok = TRUE;
			}
		}
	}

	/* check that each selecter ledger is not yet closed for this date */
	if( ok ){
		priv->count = 0;
		priv->uncloseable = 0;
		ok = FALSE;

		selected_here = FALSE;
		if( !selected ){
			selected = ofa_ledger_treeview_get_selected( priv->tview );
			selected_here = TRUE;
		}

		for( it=selected ; it ; it=it->next ){
			check_foreach_ledger( self, ( const gchar * ) it->data );
		}

		if( priv->count == 0 ){
			gtk_label_set_text( GTK_LABEL( priv->message_label ),
					_( "No selected ledger" ));

		} else if( priv->uncloseable > 0 ){
			gtk_label_set_text( GTK_LABEL( priv->message_label ),
					_( "At least one of the selected ledgers is not closeable at the proposed date" ));

		} else {
			ok = TRUE;
		}

		if( selected_here ){
			ofa_ledger_treeview_free_selected( selected );
		}
	}

	return( ok );
}

static void
check_foreach_ledger( ofaPeriodClose *self, const gchar *mnemo )
{
	ofaPeriodClosePrivate *priv;
	ofoLedger *ledger;
	const GDate *last;

	priv = ofa_period_close_get_instance_private( self );

	g_return_if_fail( my_date_is_valid( &priv->closing ));
	priv->count += 1;

	ledger = ofo_ledger_get_by_mnemo( priv->hub, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	last = ofo_ledger_get_last_close( ledger );
	if( my_date_is_valid( last ) && my_date_compare( &priv->closing, last ) < 0 ){
		priv->uncloseable += 1;
	}
}

static void
on_ok_clicked( GtkButton *button, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *close_btn;
	GtkWindow *toplevel;

	priv = ofa_period_close_get_instance_private( self );

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	/* check balances and dbms integrity */
	if( !ofa_check_balances_check( priv->hub )){
		my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
				_( "We have detected losses of balance in your books.\n\n"
					"In this current state, we will be unable to close any "
					"ledger until you fix your balances." ));

	} else if( !ofa_check_integrity_check( priv->hub )){
		my_utils_msg_dialog( toplevel, GTK_MESSAGE_WARNING,
				_( "Integrity check of the DBMS has failed.\\"
					"In this current state, we will be unable to close any "
					"ledger until you fix the errors." ));

	} else if( do_close( self )){
		gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );

		close_btn = gtk_dialog_get_widget_for_response( GTK_DIALOG( self ), GTK_RESPONSE_CANCEL );
		g_return_if_fail( close_btn && GTK_IS_BUTTON( close_btn ));
		gtk_button_set_label( GTK_BUTTON( close_btn ), _( "_Close" ));
		gtk_button_set_use_underline( GTK_BUTTON( close_btn ), TRUE );
	}
}

static gboolean
do_close( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GList *selected, *it;
	gboolean ok;
	GtkWidget *dialog, *content, *grid, *button;

	priv = ofa_period_close_get_instance_private( self );

	selected = ofa_ledger_treeview_get_selected( priv->tview );
	ok = is_dialog_validable( self, selected );
	g_return_val_if_fail( ok, FALSE );

	dialog = gtk_dialog_new_with_buttons(
					_( "Closing ledger" ),
					GTK_WINDOW( self ),
					GTK_DIALOG_MODAL,
					_( "_Close" ), GTK_RESPONSE_OK,
					NULL );

	my_utils_window_position_restore( GTK_WINDOW( dialog ),
			ofa_settings_get_settings( SETTINGS_TARGET_USER ), "ofaLedgerClosing" );

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( dialog ), GTK_RESPONSE_OK );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), FALSE );
	my_utils_widget_set_margins( button, 4, 4, 0, 8 );
	gtk_widget_set_sensitive( button, FALSE );

	content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));
	g_return_val_if_fail( content && GTK_IS_CONTAINER( content ), FALSE );

	grid = gtk_grid_new();
	my_utils_widget_set_margin_left( grid, 8 );
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 3 );
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );
	gtk_container_add( GTK_CONTAINER( content ), grid );

	priv->count = 0;
	for( it=selected ; it ; it=it->next ){
		prepare_grid( self, ( const gchar * ) it->data, grid );
	}

	gtk_widget_show_all( dialog );

	priv->count = 0;
	for( it=selected ; it ; it=it->next ){
		close_foreach_ledger( self, ( const gchar * ) it->data, grid );
	}

	ofa_ledger_treeview_free_selected( selected );
	do_end_close( self );

	gtk_widget_set_sensitive( button, TRUE );
	gtk_dialog_run( GTK_DIALOG( dialog ));

	my_utils_window_position_save( GTK_WINDOW( dialog ),
			ofa_settings_get_settings( SETTINGS_TARGET_USER ), "ofaLedgerClosing" );

	gtk_widget_destroy( dialog );

	return( TRUE );
}

static void
prepare_grid( ofaPeriodClose *self, const gchar *mnemo, GtkWidget *grid )
{
	ofaPeriodClosePrivate *priv;
	gchar *str;
	GtkWidget *label;
	myProgressBar *bar;

	priv = ofa_period_close_get_instance_private( self );

	str = g_strdup_printf( "%s :", mnemo );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_halign( label, GTK_ALIGN_START );
	gtk_widget_set_valign( label, GTK_ALIGN_END );
	gtk_grid_attach( GTK_GRID( grid ), label, 0, priv->count, 1, 1 );

	bar = my_progress_bar_new();
	my_utils_widget_set_margins( GTK_WIDGET( bar ), 2, 2, 0, 10 );
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( bar ), 1, priv->count, 1, 1 );

	priv->count += 1;
}

static gboolean
close_foreach_ledger( ofaPeriodClose *self, const gchar *mnemo, GtkWidget *grid )
{
	ofaPeriodClosePrivate *priv;
	GtkWidget *bar;
	ofoLedger *ledger;
	gboolean ok;

	priv = ofa_period_close_get_instance_private( self );

	bar = gtk_grid_get_child_at( GTK_GRID( grid ), 1, priv->count );
	g_return_val_if_fail( bar && MY_IS_PROGRESS_BAR( bar ), FALSE );
	priv->bar = MY_PROGRESS_BAR( bar );

	ledger = ofo_ledger_get_by_mnemo( priv->hub, mnemo );
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

	ok = ofo_period_close( ledger, &priv->closing );

	priv->count += 1;

	return( ok );
}

static void
do_end_close( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	gchar *str;

	priv = ofa_period_close_get_instance_private( self );

	if( priv->count == 0 ){
		str = g_strdup( _( "No closed ledger" ));

	} else if( priv->count == 1 ){
		str = g_strdup( _( "Ledger has been successfully closed" ));

	} else {
		str = g_strdup_printf( _( "%u ledgers have been successfully closed" ), priv->count );
	}

	my_iwindow_msg_dialog( MY_IWINDOW( self ), GTK_MESSAGE_INFO, str );

	g_free( str );
}

/*
 * SIGNAL_HUB_STATUS_COUNT signal handler
 */
static void
on_hub_entry_status_count( ofaHub *hub, ofaEntryStatus new_status, guint count, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;

	priv = ofa_period_close_get_instance_private( self );

	priv->entries_count = count;

	if( priv->entries_count == 0 ){
		g_signal_emit_by_name( priv->bar, "my-text", "0/0" );
	}

	priv->entries_num = 0;
}

/*
 * SIGNAL_HUB_STATUS_CHANGE signal handler
 */
static void
on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	gdouble progress;
	gchar *text;

	priv = ofa_period_close_get_instance_private( self );

	priv->entries_num += 1;
	progress = ( gdouble ) priv->entries_num / ( gdouble ) priv->entries_count;
	g_signal_emit_by_name( priv->bar, "my-double", progress );

	text = g_strdup_printf( "%u/%u", priv->entries_num, priv->entries_count );
	g_signal_emit_by_name( priv->bar, "my-text", text );
	g_free( text );
}

/*
 * settings: a string list:
 * all_ledgers;
 */
static void
load_settings( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = ofa_period_close_get_instance_private( self );

	list = ofa_settings_user_get_string_list( st_settings );

	it = list;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->all_ledgers_btn ), my_utils_boolean_from_str( cstr ));
		on_all_ledgers_toggled( GTK_TOGGLE_BUTTON( priv->all_ledgers_btn ), self );
	}

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaPeriodClose *self )
{
	ofaPeriodClosePrivate *priv;
	gchar *str;

	priv = ofa_period_close_get_instance_private( self );

	str = g_strdup_printf( "%s;",
			priv->all_ledgers ? "True":"False" );

	ofa_settings_user_set_string( st_settings, str );

	g_free( str );
}
