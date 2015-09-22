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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ui/my-editable-date.h"
#include "ui/my-progress-bar.h"
#include "ui/ofa-ledger-close.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaLedgerClosePrivate {

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
	GList              *handlers;
	guint               entries_count;	/* count of validated entries for the ledger */
	guint               entries_num;
	myProgressBar      *bar;
};

static const gchar  *st_ui_xml          = PKGUIDIR "/ofa-ledger-close.ui";
static const gchar  *st_ui_id           = "LedgerCloseDlg";
static const gchar  *st_settings        = "LedgerClose";

G_DEFINE_TYPE( ofaLedgerClose, ofa_ledger_close, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      setup_ledgers_treeview( ofaLedgerClose *self, GtkContainer *parent );
static void      setup_date( ofaLedgerClose *self, GtkContainer *parent );
static void      setup_others( ofaLedgerClose *self, GtkContainer *parent );
static void      connect_to_dossier( ofaLedgerClose *dialog );
static void      on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaLedgerClose *self );
static void      on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaLedgerClose *self );
static void      on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerClose *self );
static void      on_date_changed( GtkEditable *entry, ofaLedgerClose *self );
static void      check_for_enable_dlg( ofaLedgerClose *self, GList *selected );
static gboolean  is_dialog_validable( ofaLedgerClose *self, GList *selected );
static void      check_foreach_ledger( ofaLedgerClose *self, const gchar *ledger );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_close( ofaLedgerClose *self );
static void      prepare_grid( ofaLedgerClose *self, const gchar *mnemo, GtkWidget *grid );
static gboolean  close_foreach_ledger( ofaLedgerClose *self, const gchar *mnemo, GtkWidget *grid );
static void      do_end_close( ofaLedgerClose *self );
static void      on_dossier_entry_status_count( ofoDossier *dossier, ofaEntryStatus new_status, guint count, ofaLedgerClose *self );
static void      on_dossier_entry_status_changed( ofoDossier *dossier, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaLedgerClose *self );
static void      load_settings( ofaLedgerClose *self );
static void      set_settings( ofaLedgerClose *self );

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
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	GList *it;

	g_return_if_fail( instance && OFA_IS_LEDGER_CLOSE( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_LEDGER_CLOSE( instance )->priv;

		main_window = my_window_get_main_window( MY_WINDOW( instance ));
		g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
		dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
		g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

		if( dossier && OFO_IS_DOSSIER( dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( dossier, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_close_parent_class )->dispose( instance );
}

static void
ofa_ledger_close_init( ofaLedgerClose *self )
{
	static const gchar *thisfn = "ofa_ledger_close_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_CLOSE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LEDGER_CLOSE, ofaLedgerClosePrivate );

	my_date_clear( &self->priv->closing );
}

static void
ofa_ledger_close_class_init( ofaLedgerCloseClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_close_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_close_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_close_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaLedgerClosePrivate ));
}

/**
 * ofa_ledger_close_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected ledgers
 */
gboolean
ofa_ledger_close_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_ledger_close_run";
	ofaLedgerClose *self;
	gboolean done;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_LEDGER_CLOSE,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	my_dialog_run_dialog( MY_DIALOG( self ));

	done = self->priv->done;

	g_object_unref( self );

	return( done );
}

static void
v_init_dialog( myDialog *dialog )
{
	GtkContainer *container;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	setup_ledgers_treeview( OFA_LEDGER_CLOSE( dialog ), GTK_CONTAINER( container ));
	setup_date( OFA_LEDGER_CLOSE( dialog ), GTK_CONTAINER( container ));
	setup_others( OFA_LEDGER_CLOSE( dialog ), GTK_CONTAINER( container ));
	connect_to_dossier( OFA_LEDGER_CLOSE( dialog ));
	load_settings( OFA_LEDGER_CLOSE( dialog ));
}

static void
setup_ledgers_treeview( ofaLedgerClose *self, GtkContainer *parent )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *tview_parent;
	GtkApplicationWindow *main_window;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	tview_parent = my_utils_container_get_child_by_name( parent, "treeview-parent" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));
	ofa_ledger_treeview_set_columns( priv->tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LABEL | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_main_window( priv->tview, OFA_MAIN_WINDOW( main_window ));
	ofa_ledger_treeview_set_selection_mode( priv->tview, GTK_SELECTION_MULTIPLE );

	g_signal_connect( G_OBJECT( priv->tview ), "ofa-changed", G_CALLBACK( on_rows_selected ), self );
	g_signal_connect( G_OBJECT( priv->tview ), "ofa-activated", G_CALLBACK( on_rows_activated ), self );
}

static void
setup_date( ofaLedgerClose *self, GtkContainer *parent )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *label;

	priv = self->priv;

	priv->closing_entry = my_utils_container_get_child_by_name( parent, "p1-date" );
	my_editable_date_init( GTK_EDITABLE( priv->closing_entry ));
	my_editable_date_set_format( GTK_EDITABLE( priv->closing_entry ), ofa_prefs_date_display());

	label = my_utils_container_get_child_by_name( parent, "p1-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	my_editable_date_set_label( GTK_EDITABLE( priv->closing_entry ), label, ofa_prefs_date_check());

	g_signal_connect( G_OBJECT( priv->closing_entry ), "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaLedgerClose *self, GtkContainer *parent )
{
	ofaLedgerClosePrivate *priv;
	GtkWidget *button, *label;

	priv = self->priv;

	button = my_utils_container_get_child_by_name( parent, "all-ledgers-btn" );
	g_return_if_fail( button && GTK_IS_CHECK_BUTTON( button ));
	g_signal_connect( button, "toggled", G_CALLBACK( on_all_ledgers_toggled ), self );
	priv->all_ledgers_btn = button;

	button = my_utils_container_get_child_by_name( parent, "btn-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	priv->do_close_btn = button;

	label = my_utils_container_get_child_by_name( parent, "p1-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;
	my_utils_widget_set_style( label, "labelerror" );
}

static void
connect_to_dossier( ofaLedgerClose *dialog )
{
	ofaLedgerClosePrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	gulong handler;

	priv = dialog->priv;

	main_window = my_window_get_main_window( MY_WINDOW( dialog ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	handler = g_signal_connect(
					G_OBJECT( dossier ),
					SIGNAL_DOSSIER_ENTRY_STATUS_COUNT, G_CALLBACK( on_dossier_entry_status_count ), dialog );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( dossier ),
					SIGNAL_DOSSIER_ENTRY_STATUS_CHANGED, G_CALLBACK( on_dossier_entry_status_changed ), dialog );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
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

	priv = self->priv;

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

	priv = self->priv;

	my_date_set_from_date( &priv->closing,
			my_editable_date_get_date( GTK_EDITABLE( priv->closing_entry ), NULL ));

	check_for_enable_dlg( self, NULL );
}

static void
check_for_enable_dlg( ofaLedgerClose *self, GList *selected )
{
	gboolean ok;

	ok = is_dialog_validable( self, selected );

	gtk_widget_set_sensitive( self->priv->do_close_btn, ok );

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
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	GList *it;
	gboolean ok;
	const GDate *exe_begin, *exe_end;
	gboolean selected_here;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	gtk_label_set_text( GTK_LABEL( priv->message_label ), "" );
	ok = FALSE;

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
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	ofoLedger *ledger;
	const GDate *last;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	g_return_if_fail( my_date_is_valid( &priv->closing ));
	priv->count += 1;

	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	last = ofo_ledger_get_last_close( ledger );
	if( my_date_is_valid( last ) && my_date_compare( &priv->closing, last ) < 0 ){
		priv->uncloseable += 1;
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	GtkWindow *toplevel;
	GtkWidget *button;

	if( do_close( OFA_LEDGER_CLOSE( dialog ))){

		toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
		g_return_val_if_fail( toplevel && GTK_IS_DIALOG( toplevel ), FALSE );

		button = gtk_dialog_get_widget_for_response( GTK_DIALOG( toplevel ), GTK_RESPONSE_OK );
		g_return_val_if_fail( button && GTK_IS_BUTTON( button ), FALSE );
		gtk_widget_set_sensitive( button, FALSE );

		button = gtk_dialog_get_widget_for_response( GTK_DIALOG( toplevel ), GTK_RESPONSE_CANCEL );
		g_return_val_if_fail( button && GTK_IS_BUTTON( button ), FALSE );
		gtk_button_set_label( GTK_BUTTON( button ), _( "_Close" ));
		gtk_button_set_use_underline( GTK_BUTTON( button ), TRUE );
	}

	return( FALSE );
}

static gboolean
do_close( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	GList *selected, *it;
	gboolean ok;
	GtkWidget *dialog, *content, *grid, *button;

	priv = self->priv;
	selected = ofa_ledger_treeview_get_selected( priv->tview );
	ok = is_dialog_validable( self, selected );
	g_return_val_if_fail( ok, FALSE );

	dialog = gtk_dialog_new_with_buttons(
					_( "Closing ledger" ),
					my_window_get_toplevel( MY_WINDOW( self )),
					GTK_DIALOG_MODAL,
					_( "_Close" ), GTK_RESPONSE_OK,
					NULL );

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( dialog ), GTK_RESPONSE_OK );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), FALSE );
	gtk_widget_set_sensitive( button, FALSE );

	content = gtk_dialog_get_content_area( GTK_DIALOG( dialog ));
	g_return_val_if_fail( content && GTK_IS_CONTAINER( content ), FALSE );

	grid = gtk_grid_new();
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
	gtk_widget_destroy( dialog );

	return( TRUE );
}

static void
prepare_grid( ofaLedgerClose *self, const gchar *mnemo, GtkWidget *grid )
{
	ofaLedgerClosePrivate *priv;
	gchar *str;
	GtkWidget *label, *alignment;
	myProgressBar *bar;

	priv = self->priv;

	str = g_strdup_printf( "%s :", mnemo );
	label = gtk_label_new( str );
	g_free( str );
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_grid_attach( GTK_GRID( grid ), label, 0, priv->count, 1, 1 );

	alignment = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_alignment_set_padding( GTK_ALIGNMENT( alignment ), 2, 2, 0, 10 );
	gtk_grid_attach( GTK_GRID( grid ), alignment, 1, priv->count, 1, 1 );
	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( bar ));

	priv->count += 1;
}

static gboolean
close_foreach_ledger( ofaLedgerClose *self, const gchar *mnemo, GtkWidget *grid )
{
	ofaLedgerClosePrivate *priv;
	GtkApplicationWindow *main_window;
	ofoDossier *dossier;
	GtkWidget *widget, *bar;
	ofoLedger *ledger;
	gboolean ok;

	priv = self->priv;

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	widget = gtk_grid_get_child_at( GTK_GRID( grid ), 1, priv->count );
	g_return_val_if_fail( widget && GTK_IS_BIN( widget ), FALSE );

	bar = gtk_bin_get_child( GTK_BIN( widget ));
	g_return_val_if_fail( bar && MY_IS_PROGRESS_BAR( bar ), FALSE );
	priv->bar = MY_PROGRESS_BAR( bar );

	ledger = ofo_ledger_get_by_mnemo( dossier, mnemo );
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

	ok = ofo_ledger_close( ledger, dossier, &priv->closing );

	priv->count += 1;

	return( ok );
}

static void
do_end_close( ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	gchar *str;
	GtkWindow *toplevel;
	GtkWidget *dialog;

	priv = self->priv;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	if( priv->count == 0 ){
		str = g_strdup( _( "No closed ledger" ));

	} else if( priv->count == 1 ){
		str = g_strdup( _( "Ledger has been successfully closed" ));

	} else {
		str = g_strdup_printf( _( "%u ledgers have been successfully closed" ), priv->count );
	}

	dialog = gtk_message_dialog_new(
			toplevel,
			0,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"%s", str );

	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}

static void
on_dossier_entry_status_count( ofoDossier *dossier, ofaEntryStatus new_status, guint count, ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;

	priv = self->priv;

	priv->entries_count = count;

	if( priv->entries_count == 0 ){
		g_signal_emit_by_name( priv->bar, "ofa-text", "0/0" );
	}

	priv->entries_num = 0;
}

static void
on_dossier_entry_status_changed( ofoDossier *dossier, ofoEntry *entry, ofaEntryStatus prev_status, ofaEntryStatus new_status, ofaLedgerClose *self )
{
	ofaLedgerClosePrivate *priv;
	gdouble progress;
	gchar *text;

	priv = self->priv;

	priv->entries_num += 1;
	progress = ( gdouble ) priv->entries_num / ( gdouble ) priv->entries_count;
	g_signal_emit_by_name( priv->bar, "ofa-double", progress );

	text = g_strdup_printf( "%u/%u", priv->entries_num, priv->entries_count );
	g_signal_emit_by_name( priv->bar, "ofa-text", text );
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

	priv = self->priv;
	list = ofa_settings_get_string_list( st_settings );

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

	priv = self->priv;

	str = g_strdup_printf( "%s;",
			priv->all_ledgers ? "True":"False" );

	ofa_settings_set_string( st_settings, str );

	g_free( str );
}
