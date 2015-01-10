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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "core/my-window-prot.h"

#include "ui/my-editable-date.h"
#include "ui/my-progress-bar.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-int-closing.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaIntClosingPrivate {

	gboolean            done;			/* whether we have actually done something */
	GDate               closing;
	gboolean            valid;			/* reset after each date change */

	/* UI
	 */
	ofaLedgerTreeview *tview;
	GtkButton          *do_close_btn;
	GtkLabel           *date_label;
	GtkLabel           *message_label;
	GtkWidget          *closing_entry;

	/* during the iteration on each selected ledger
	 */
	gint                count;			/* count of ledgers */
	gint                closeable;
	GList              *handlers;
	guint               entries_count;	/* count of validated entries for the ledger */
	guint               entries_num;
	myProgressBar      *bar;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-int-closing.ui";
static const gchar  *st_ui_id  = "IntClosingDlg";

G_DEFINE_TYPE( ofaIntClosing, ofa_int_closing, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      connect_to_dossier( ofaIntClosing *dialog );
static void      on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaIntClosing *self );
static void      on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaIntClosing *self );
static void      on_date_changed( GtkEditable *entry, ofaIntClosing *self );
static void      check_for_enable_dlg( ofaIntClosing *self, GList *selected );
static gboolean  is_dialog_validable( ofaIntClosing *self, GList *selected );
static void      check_foreach_ledger( ofaIntClosing *self, const gchar *ledger );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_close( ofaIntClosing *self );
static void      prepare_grid( ofaIntClosing *self, const gchar *mnemo, GtkWidget *grid );
static gboolean  close_foreach_ledger( ofaIntClosing *self, const gchar *mnemo, GtkWidget *grid );
static void      do_end_close( ofaIntClosing *self );
static void      on_dossier_pre_valid_entry( ofoDossier *dossier, const gchar *ledger, guint count, ofaIntClosing *self );
static void      on_dossier_validated_entry( ofoDossier *dossier, void *entry, ofaIntClosing *self );

static void
int_closing_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_int_closing_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_INT_CLOSING( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_int_closing_parent_class )->finalize( instance );
}

static void
int_closing_dispose( GObject *instance )
{
	ofaIntClosingPrivate *priv;
	ofoDossier *dossier;
	GList *it;

	g_return_if_fail( instance && OFA_IS_INT_CLOSING( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_INT_CLOSING( instance )->priv;
		dossier = MY_WINDOW( instance )->prot->dossier;

		if( dossier && OFO_IS_DOSSIER( dossier )){
			for( it=priv->handlers ; it ; it=it->next ){
				g_signal_handler_disconnect( dossier, ( gulong ) it->data );
			}
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_int_closing_parent_class )->dispose( instance );
}

static void
ofa_int_closing_init( ofaIntClosing *self )
{
	static const gchar *thisfn = "ofa_int_closing_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_INT_CLOSING( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_INT_CLOSING, ofaIntClosingPrivate );

	my_date_clear( &self->priv->closing );
}

static void
ofa_int_closing_class_init( ofaIntClosingClass *klass )
{
	static const gchar *thisfn = "ofa_int_closing_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = int_closing_dispose;
	G_OBJECT_CLASS( klass )->finalize = int_closing_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaIntClosingPrivate ));
}

/**
 * ofa_int_closing_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected ledgers
 */
gboolean
ofa_int_closing_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_int_closing_run";
	ofaIntClosing *self;
	gboolean done;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_INT_CLOSING,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
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
	ofaIntClosingPrivate *priv;
	GtkContainer *container;
	GtkButton *button;
	GtkLabel *label;
	GtkWidget *parent;

	priv = OFA_INT_CLOSING( dialog )->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	button = ( GtkButton * )
					my_utils_container_get_child_by_name( container, "btn-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	priv->do_close_btn = button;

	parent = my_utils_container_get_child_by_name( container, "treeview-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));
	priv->tview = ofa_ledger_treeview_new();
	ofa_ledger_treeview_attach_to( priv->tview, GTK_CONTAINER( parent ));
	ofa_ledger_treeview_set_columns( priv->tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LABEL | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_main_window( priv->tview, MY_WINDOW( dialog )->prot->main_window );
	ofa_ledger_treeview_set_selection_mode( priv->tview, GTK_SELECTION_MULTIPLE );

	g_signal_connect( G_OBJECT( priv->tview ), "changed", G_CALLBACK( on_rows_selected ), dialog );
	g_signal_connect( G_OBJECT( priv->tview ), "activated", G_CALLBACK( on_rows_activated ), dialog );

	label = ( GtkLabel * ) my_utils_container_get_child_by_name( container, "p1-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;

	priv->closing_entry = my_utils_container_get_child_by_name( container, "p1-date" );
	my_editable_date_init( GTK_EDITABLE( priv->closing_entry ));
	my_editable_date_set_format( GTK_EDITABLE( priv->closing_entry ), MY_DATE_DMYY );
	my_editable_date_set_date( GTK_EDITABLE( priv->closing_entry ), &priv->closing );
	priv->date_label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p1-label" ));
	my_editable_date_set_label( GTK_EDITABLE( priv->closing_entry ), GTK_WIDGET( priv->date_label ), MY_DATE_DMMM );

	g_signal_connect( G_OBJECT( priv->closing_entry ), "changed", G_CALLBACK( on_date_changed ), dialog );

	connect_to_dossier( OFA_INT_CLOSING( dialog ));

	check_for_enable_dlg( OFA_INT_CLOSING( dialog ), NULL );
}

static void
connect_to_dossier( ofaIntClosing *dialog )
{
	ofaIntClosingPrivate *priv;
	ofoDossier *dossier;
	gulong handler;

	priv = dialog->priv;
	dossier = MY_WINDOW( dialog )->prot->dossier;

	handler = g_signal_connect(
					G_OBJECT( dossier ),
					SIGNAL_DOSSIER_PRE_VALID_ENTRY, G_CALLBACK( on_dossier_pre_valid_entry ), dialog );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );

	handler = g_signal_connect(
					G_OBJECT( dossier ),
					SIGNAL_DOSSIER_VALIDATED_ENTRY, G_CALLBACK( on_dossier_validated_entry ), dialog );
	priv->handlers = g_list_prepend( priv->handlers, ( gpointer ) handler );
}

/*
 * LedgerTreeview callback
 */
static void
on_rows_activated( ofaLedgerTreeview *view, GList *selected, ofaIntClosing *self )
{
	if( is_dialog_validable( self, selected )){
		do_close( self );
	}
}

/*
 * LedgerTreeview callback
 */
static void
on_rows_selected( ofaLedgerTreeview *view, GList *selected, ofaIntClosing *self )
{
	check_for_enable_dlg( self, selected );
}

static void
on_date_changed( GtkEditable *entry, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	const GDate *exe_end;

	priv = self->priv;
	priv->valid = FALSE;
	my_date_set_from_date( &priv->closing,
			my_editable_date_get_date( GTK_EDITABLE( priv->closing_entry ), NULL ));

	if( !my_date_is_valid( &priv->closing )){
		gtk_label_set_text( priv->message_label, _( "Invalid closing date" ));

	} else {
		/* the date must be less or equal that the end of exercice */
		exe_end = ofo_dossier_get_exe_end( MY_WINDOW( self )->prot->dossier );
		if( !my_date_is_valid( exe_end ) || my_date_compare( &priv->closing, exe_end ) <= 0 ){
			priv->valid = TRUE;
			gtk_label_set_text( priv->message_label, "" );

		} else {
			gtk_label_set_text( priv->message_label, _( "Closing date is after the end of exercice" ));
		}
	}

	check_for_enable_dlg( self, NULL );
}

static void
check_for_enable_dlg( ofaIntClosing *self, GList *selected )
{
	gtk_widget_set_sensitive(
				GTK_WIDGET( self->priv->do_close_btn ),
				is_dialog_validable( self, selected ));
}

/*
 * an intermediate cloture is allowed if the proposed closing date is
 * valid, and greater that at least one of the previous closing dates
 */
static gboolean
is_dialog_validable( ofaIntClosing *self, GList *selected )
{
	ofaIntClosingPrivate *priv;
	GList *selection, *it;
	gboolean ok;

	priv = self->priv;

	/* do we have a intrinsically valid proposed closing date ? */
	ok = priv->valid;

	/* check that each ledger is not yet closed for this date */
	if( ok ){
		priv->count = 0;
		priv->closeable = 0;

		selection = NULL;
		if( !selected ){
			selection = ofa_ledger_treeview_get_selected( priv->tview );
			selected = selection;
		}

		for( it=selected ; it ; it=it->next ){
			check_foreach_ledger( self, ( const gchar * ) it->data );
		}

		if( !priv->closeable ){
			gtk_label_set_text( priv->message_label, _( "None of the selected ledgers is closeable at the proposed date" ));
			ok = FALSE;
		}

		if( selection ){
			ofa_ledger_treeview_free_selected( selection );
		}
	}
	if( ok ){
		gtk_label_set_text( priv->message_label, "" );
	}

	return( ok );
}

static void
check_foreach_ledger( ofaIntClosing *self, const gchar *mnemo )
{
	ofaIntClosingPrivate *priv;
	ofoLedger *ledger;
	const GDate *last;

	priv = self->priv;
	priv->count += 1;

	ledger = ofo_ledger_get_by_mnemo( MY_WINDOW( self )->prot->dossier, mnemo );
	g_return_if_fail( ledger && OFO_IS_LEDGER( ledger ));

	last = ofo_ledger_get_last_close( ledger );

	g_return_if_fail( my_date_is_valid( &priv->closing ));

	if( !my_date_is_valid( last ) || my_date_compare( &priv->closing, last ) > 0 ){
		priv->closeable += 1;
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	GtkWindow *toplevel;
	GtkWidget *button;

	if( do_close( OFA_INT_CLOSING( dialog ))){

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
do_close( ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
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
	gtk_widget_destroy( dialog );

	return( TRUE );
}

static void
prepare_grid( ofaIntClosing *self, const gchar *mnemo, GtkWidget *grid )
{
	ofaIntClosingPrivate *priv;
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
	my_progress_bar_attach_to( bar, GTK_CONTAINER( alignment ));

	priv->count += 1;
}

static gboolean
close_foreach_ledger( ofaIntClosing *self, const gchar *mnemo, GtkWidget *grid )
{
	ofaIntClosingPrivate *priv;
	GtkWidget *widget, *bar;
	ofoLedger *ledger;
	gboolean ok;

	priv = self->priv;

	widget = gtk_grid_get_child_at( GTK_GRID( grid ), 1, priv->count );
	g_return_val_if_fail( widget && GTK_IS_BIN( widget ), FALSE );

	bar = gtk_bin_get_child( GTK_BIN( widget ));
	g_return_val_if_fail( bar && MY_IS_PROGRESS_BAR( bar ), FALSE );
	priv->bar = MY_PROGRESS_BAR( bar );

	ledger = ofo_ledger_get_by_mnemo( MY_WINDOW( self )->prot->dossier, mnemo );
	g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

	ok = ofo_ledger_close( ledger, MY_WINDOW( self )->prot->dossier, &priv->closing );

	priv->count += 1;

	return( ok );
}

static void
do_end_close( ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
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
on_dossier_pre_valid_entry( ofoDossier *dossier, const gchar *ledger, guint count, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;

	priv = self->priv;

	priv->entries_count = count;
	if( priv->entries_count == 0 ){
		g_signal_emit_by_name( priv->bar, "text", "0/0" );
	}

	priv->entries_num = 0;
}

static void
on_dossier_validated_entry( ofoDossier *dossier, void *entry, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	gdouble progress;
	gchar *text;

	priv = self->priv;

	priv->entries_num += 1;
	progress = ( gdouble ) priv->entries_num / ( gdouble ) priv->entries_count;
	g_signal_emit_by_name( priv->bar, "double", progress );

	text = g_strdup_printf( "%u/%u", priv->entries_num, priv->entries_count );
	g_signal_emit_by_name( priv->bar, "text", text );
	g_free( text );
}
