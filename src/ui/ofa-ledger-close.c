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
	gboolean            done;				/* whether we have actually done something */
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
	guint               uncloseable;
	guint               count;
}
	ofaLedgerClosePrivate;

/* a data structure to handle the iteration on ledgers
 */
typedef struct {
	/* input data
	 */
	ofaHub        *hub;
	GtkWindow     *parent;
	GList         *ledgers;
	const GDate   *closing_date;
	GList         *hub_handlers;

	/* running data
	 */
	guint          count;
	myProgressBar *bar;
	guint          entries_count;			/* count of validated entries for the ledger */
	guint          entries_num;
}
	sClose;

static const gchar  *st_resource_ui     = "/org/trychlos/openbook/ui/ofa-ledger-close.ui";
static const gchar  *st_settings        = "LedgerClose";

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      setup_ledgers_treeview( ofaLedgerClose *self );
static void      setup_date( ofaLedgerClose *self );
static void      setup_others( ofaLedgerClose *self );
static void      on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgerClose *self );
static void      on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgerClose *self );
static void      on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerClose *self );
static void      on_date_changed( GtkEditable *entry, ofaLedgerClose *self );
static void      check_for_enable_dlg( ofaLedgerClose *self, GList *selected );
static gboolean  is_dialog_validable( ofaLedgerClose *self, GList *selected );
static void      check_foreach_ledger( ofaLedgerClose *self, const gchar *ledger );
static void      on_ok_clicked( GtkButton *button, ofaLedgerClose *self );
static gboolean  do_close( ofaLedgerClose *self );
static void      do_close_ledgers( sClose *close_data );
static void      prepare_grid( sClose *close_data, const gchar *mnemo, GtkWidget *grid );
static gboolean  close_foreach_ledger( sClose *close_data, const gchar *mnemo, GtkWidget *grid );
static void      do_end_close( sClose *close_data );
static void      on_hub_entry_status_count( ofaHub *hub, ofaEntryStatus new_status, guint count, sClose *close_data );
static void      on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, sClose *close_data );
static void      load_settings( ofaLedgerClose *self );
static void      set_settings( ofaLedgerClose *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerClose, ofa_ledger_close, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaLedgerClose )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
ledger_close_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_close_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_CLOSE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_close_parent_class )->finalize( instance );
}

static void
ledger_close_dispose( GObject *instance )
{
	ofaLedgerClosePrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_CLOSE( instance ));

	priv = ofa_ledger_close_get_instance_private( OFA_LEDGER_CLOSE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_close_parent_class )->dispose( instance );
}

static void
ofa_ledger_close_init( ofaLedgerClose *self )
{
	static const gchar *thisfn = "ofa_ledger_close_init";
	ofaLedgerClosePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_CLOSE( self ));

	priv = ofa_ledger_close_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	my_date_clear( &priv->closing );

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_ledger_close_class_init( ofaLedgerCloseClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_close_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_close_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_close_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_ledger_close_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Close the selected ledgers.
 */
void
ofa_ledger_close_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_ledger_close_run";
	ofaLedgerClose *self;
	ofaLedgerClosePrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_LEDGER_CLOSE, NULL );
	my_iwindow_set_parent( MY_IWINDOW( self ), parent );
	my_iwindow_set_settings( MY_IWINDOW( self ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

	priv = ofa_ledger_close_get_instance_private( self );

	priv->getter = getter;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/**
 * ofa_ledger_close_do_close_all:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @closing_date: the closing date.
 *
 * Close all ledgers.
 */
void
ofa_ledger_close_do_close_all( ofaIGetter *getter, GtkWindow *parent, const GDate *closing_date )
{
	static const gchar *thisfn = "ofa_ledger_close_do_close_all";
	ofaHub *hub;
	GList *led_mnemos, *dataset, *it;
	sClose *close_data;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( my_date_is_valid( closing_date ));

	hub = ofa_igetter_get_hub( getter );
	dataset = ofo_ledger_get_dataset( hub );
	led_mnemos = NULL;
	for( it=dataset ; it ; it=it->next ){
		led_mnemos = g_list_prepend( led_mnemos, ( gpointer ) ofo_ledger_get_mnemo( OFO_LEDGER( it->data )));
	}

	close_data = g_new0( sClose, 1 );
	close_data->hub = hub;
	close_data->parent = parent;
	close_data->ledgers = led_mnemos;
	close_data->closing_date = closing_date;

	do_close_ledgers( close_data );

	g_free( close_data );
	g_list_free( led_mnemos );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_close_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_ledger_close_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

/*
 * first setup the UI fields, then fills them up with the data
 * when entering, only initialization data are set: main_window
 */
static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_ledger_close_idialog_init";
	ofaLedgerClosePrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ledger_close_get_instance_private( OFA_LEDGER_CLOSE( instance ));

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	setup_ledgers_treeview( OFA_LEDGER_CLOSE( instance ));
	setup_date( OFA_LEDGER_CLOSE( instance ));
	setup_others( OFA_LEDGER_CLOSE( instance ));

	load_settings( OFA_LEDGER_CLOSE( instance ));
}

static void
setup_ledgers_treeview( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *tview_parent, *label;

	priv = ofa_ledger_close_get_instance_private( self );

	tview_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-treeview-parent" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));
	ofa_ledger_treeview_set_columns( priv->tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LABEL | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_hub( priv->tview, priv->hub);
	ofa_ledger_treeview_set_selection_mode( priv->tview, GTK_SELECTION_MULTIPLE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-frame-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), ofa_ledger_treeview_get_treeview( priv->tview ));

	g_signal_connect( priv->tview, "ofa-changed", G_CALLBACK( on_rows_selected ), self );
	g_signal_connect( priv->tview, "ofa-activated", G_CALLBACK( on_rows_activated ), self );
}

static void
setup_date( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *label;

	priv = ofa_ledger_close_get_instance_private( self );

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
	my_date_editable_set_overwrite( GTK_EDITABLE( priv->closing_entry ), ofa_prefs_date_overwrite());

	g_signal_connect( priv->closing_entry, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *button, *label;

	priv = ofa_ledger_close_get_instance_private( self );

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
on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgerClose *self )
{
	if( is_dialog_validable( self, selected )){
		do_close( self );
	}
}

/*
 * LedgerTreeview callback
 */
static void
on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgerClose *self )
{
	check_for_enable_dlg( self, selected );
}

static void
on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GtkTreeSelection *selection;

	priv = ofa_ledger_close_get_instance_private( self );

	priv->all_ledgers = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( GTK_WIDGET( priv->tview ), !priv->all_ledgers );

	if( priv->all_ledgers ){
		selection = ofa_ledger_treeview_get_selection( priv->tview );
		gtk_tree_selection_select_all( selection );
	}
}

static void
on_date_changed( GtkEditable *entry, ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;

	priv = ofa_ledger_close_get_instance_private( self );

	my_date_set_from_date( &priv->closing,
			my_date_editable_get_date( GTK_EDITABLE( priv->closing_entry ), NULL ));

	check_for_enable_dlg( self, NULL );
}

static void
check_for_enable_dlg( ofaLedgerClose *self, GList *selected )
{
	ofaLedgerClosePrivate *priv;
	gboolean ok;

	priv = ofa_ledger_close_get_instance_private( self );

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
is_dialog_validable( ofaLedgerClose *self, GList *selected )
{
	ofaLedgerClosePrivate *priv;
	ofoDossier *dossier;
	GList *it;
	gboolean ok;
	const GDate *exe_begin, *exe_end;
	gboolean selected_here;

	priv = ofa_ledger_close_get_instance_private( self );

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
check_foreach_ledger( ofaLedgerClose *self, const gchar *mnemo )
{
	ofaLedgerClosePrivate *priv;
	ofoLedger *ledger;
	const GDate *last;

	priv = ofa_ledger_close_get_instance_private( self );

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
on_ok_clicked( GtkButton *button, ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *close_btn;
	GtkWindow *toplevel;

	priv = ofa_ledger_close_get_instance_private( self );

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
do_close( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GList *selected;
	gboolean ok;
	sClose *close_data;

	priv = ofa_ledger_close_get_instance_private( self );

	selected = ofa_ledger_treeview_get_selected( priv->tview );
	ok = is_dialog_validable( self, selected );
	g_return_val_if_fail( ok, FALSE );


	close_data = g_new0( sClose, 1 );
	close_data->hub = priv->hub;
	close_data->parent = GTK_WINDOW( self );
	close_data->closing_date = &priv->closing;
	close_data->ledgers = selected;

	do_close_ledgers( close_data );

	g_free( close_data );
	ofa_ledger_treeview_free_selected( selected );

	return( TRUE );
}

static void
do_close_ledgers( sClose *close_data )
{
	GList *it;
	GtkWidget *dialog, *content, *grid, *button;
	gulong handler;

	handler = g_signal_connect(
					close_data->hub, SIGNAL_HUB_STATUS_COUNT,
					G_CALLBACK( on_hub_entry_status_count ), close_data );
	close_data->hub_handlers = g_list_prepend( close_data->hub_handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					close_data->hub, SIGNAL_HUB_STATUS_CHANGE,
					G_CALLBACK( on_hub_entry_status_change ), close_data );
	close_data->hub_handlers = g_list_prepend( close_data->hub_handlers, ( gpointer ) handler );

	dialog = gtk_dialog_new_with_buttons(
					_( "Closing ledger" ),
					close_data->parent,
					GTK_DIALOG_MODAL,
					_( "_Close" ), GTK_RESPONSE_OK,
					NULL );

	my_utils_window_position_restore( GTK_WINDOW( dialog ),
			ofa_settings_get_settings( SETTINGS_TARGET_USER ), "ofaLedgerClosing" );

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( dialog ), GTK_RESPONSE_OK );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	my_utils_widget_set_margins( button, 4, 4, 0, 8 );
	gtk_widget_set_sensitive( button, FALSE );

	content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));
	g_return_if_fail( content && GTK_IS_CONTAINER( content ));

	grid = gtk_grid_new();
	my_utils_widget_set_margin_left( grid, 8 );
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 3 );
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );
	gtk_container_add( GTK_CONTAINER( content ), grid );

	close_data->count = 0;
	for( it=close_data->ledgers ; it ; it=it->next ){
		prepare_grid( close_data, ( const gchar * ) it->data, grid );
	}

	gtk_widget_show_all( dialog );

	close_data->count = 0;
	for( it=close_data->ledgers ; it ; it=it->next ){
		close_foreach_ledger( close_data, ( const gchar * ) it->data, grid );
	}

	do_end_close( close_data );

	gtk_widget_set_sensitive( button, TRUE );
	gtk_dialog_run( GTK_DIALOG( dialog ));

	my_utils_window_position_save( GTK_WINDOW( dialog ),
			ofa_settings_get_settings( SETTINGS_TARGET_USER ), "ofaLedgerClosing" );

	gtk_widget_destroy( dialog );

	ofa_hub_disconnect_handlers( close_data->hub, &close_data->hub_handlers );
}

static void
prepare_grid( sClose *close_data, const gchar *mnemo, GtkWidget *grid )
{
	gchar *str;
	GtkWidget *label;
	myProgressBar *bar;

	str = g_strdup_printf( "%s :", mnemo );
	label = gtk_label_new( str );
	g_free( str );
	gtk_widget_set_halign( label, GTK_ALIGN_START );
	gtk_widget_set_valign( label, GTK_ALIGN_END );
	gtk_grid_attach( GTK_GRID( grid ), label, 0, close_data->count, 1, 1 );

	bar = my_progress_bar_new();
	my_utils_widget_set_margins( GTK_WIDGET( bar ), 2, 2, 0, 10 );
	gtk_grid_attach( GTK_GRID( grid ), GTK_WIDGET( bar ), 1, close_data->count, 1, 1 );

	close_data->count += 1;
}

static gboolean
close_foreach_ledger( sClose *close_data, const gchar *mnemo, GtkWidget *grid )
{
	GtkWidget *bar;
	ofoLedger *ledger;
	gboolean ok;

	bar = gtk_grid_get_child_at( GTK_GRID( grid ), 1, close_data->count );
	g_return_val_if_fail( bar && MY_IS_PROGRESS_BAR( bar ), FALSE );
	close_data->bar = MY_PROGRESS_BAR( bar );

	ledger = ofo_ledger_get_by_mnemo( close_data->hub, mnemo );
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

	ok = ofo_ledger_close( ledger, close_data->closing_date );

	close_data->count += 1;

	return( ok );
}

static void
do_end_close( sClose *close_data )
{
	gchar *str;

	if( close_data->count == 0 ){
		str = g_strdup( _( "No closed ledger" ));

	} else if( close_data->count == 1 ){
		str = g_strdup( _( "Ledger has been successfully closed" ));

	} else {
		str = g_strdup_printf( _( "%u ledgers have been successfully closed" ), close_data->count );
	}

	my_utils_msg_dialog( close_data->parent, GTK_MESSAGE_INFO, str );

	g_free( str );
}

/*
 * SIGNAL_HUB_STATUS_COUNT signal handler
 */
static void
on_hub_entry_status_count( ofaHub *hub, ofaEntryStatus new_status, guint count, sClose *close_data )
{
	close_data->entries_count = count;

	if( close_data->entries_count == 0 ){
		g_signal_emit_by_name( close_data->bar, "my-text", "0/0" );
	}

	close_data->entries_num = 0;
}

/*
 * SIGNAL_HUB_STATUS_CHANGE signal handler
 */
static void
on_hub_entry_status_change( ofaHub *hub, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, sClose *close_data )
{
	gdouble progress;
	gchar *text;

	close_data->entries_num += 1;
	progress = ( gdouble ) close_data->entries_num / ( gdouble ) close_data->entries_count;
	g_signal_emit_by_name( close_data->bar, "my-double", progress );

	text = g_strdup_printf( "%u/%u", close_data->entries_num, close_data->entries_count );
	g_signal_emit_by_name( close_data->bar, "my-text", text );
	g_free( text );
}

/*
 * settings: a string list:
 * all_ledgers;
 */
static void
load_settings( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GList *list, *it;
	const gchar *cstr;

	priv = ofa_ledger_close_get_instance_private( self );

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
set_settings( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	gchar *str;

	priv = ofa_ledger_close_get_instance_private( self );

	str = g_strdup_printf( "%s;",
			priv->all_ledgers ? "True":"False" );

	ofa_settings_user_set_string( st_settings, str );

	g_free( str );
}
