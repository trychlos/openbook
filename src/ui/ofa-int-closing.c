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

#include "core/my-utils.h"
#include "ui/my-window-prot.h"
#include "ui/ofa-journal-treeview.h"
#include "ui/ofa-int-closing.h"
#include "ui/ofa-main-window.h"
#include "api/ofo-dossier.h"
#include "api/ofo-journal.h"

/* private instance data
 */
struct _ofaIntClosingPrivate {

	gboolean            done;			/* whether we have actually done something */
	GDate               closing;
	gboolean            valid;			/* reset after each date change */

	/* UI
	 */
	ofaJournalTreeview *tview;
	GtkButton          *do_close_btn;
	GtkLabel           *date_label;
	GtkLabel           *message_label;

	/* during the iteration on each selected journal
	 */
	gint                count;
	gint                closeable;
	GString            *jou_list;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-int-closing.ui";
static const gchar  *st_ui_id  = "IntClosingDlg";

G_DEFINE_TYPE( ofaIntClosing, ofa_int_closing, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_rows_activated( GList *selected, ofaIntClosing *self );
static void      on_rows_selected( GList *selected, ofaIntClosing *self );
static gboolean  date_check_cb( GDate *date, ofaIntClosing *self );
static void      on_date_changed( GtkEntry *entry, ofaIntClosing *self );
static void      check_for_enable_dlg( ofaIntClosing *self, GList *selected );
static gboolean  is_dialog_validable( ofaIntClosing *self, GList *selected );
static void      check_foreach_journal( ofaIntClosing *self, ofoJournal *journal );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_close( ofaIntClosing *self );
static gboolean  close_foreach_journal( ofaIntClosing *self, ofoJournal *journal );
static void      do_end_close( ofaIntClosing *self );

static void
int_closing_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_int_closing_finalize";
	ofaIntClosingPrivate *priv;

	g_return_if_fail( OFA_IS_INT_CLOSING( instance ));

	priv = OFA_INT_CLOSING( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_int_closing_parent_class )->finalize( instance );
}

static void
int_closing_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_INT_CLOSING( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_int_closing_parent_class )->dispose( instance );
}

static void
ofa_int_closing_init( ofaIntClosing *self )
{
	static const gchar *thisfn = "ofa_int_closing_instance_init";

	g_return_if_fail( OFA_IS_INT_CLOSING( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaIntClosingPrivate, 1 );

	g_date_clear( &self->private->closing, 1 );
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
}

/**
 * ofa_int_closing_run:
 * @main: the main window of the application.
 *
 * Run an intermediate closing on selected journals
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

	done = self->private->done;

	g_object_unref( self );

	return( done );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaIntClosingPrivate *priv;
	JournalTreeviewParms parms;
	GtkContainer *container;
	GtkButton *button;
	GtkLabel *label;
	myDateParse date_parms;

	priv = OFA_INT_CLOSING( dialog )->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	button = ( GtkButton * )
					my_utils_container_get_child_by_name( container, "btn-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	priv->do_close_btn = button;

	parms.main_window = MY_WINDOW( dialog )->protected->main_window;
	parms.parent = GTK_CONTAINER(
					my_utils_container_get_child_by_name( container, "px-treeview-alignement" ));
	parms.allow_multiple_selection = TRUE;
	parms.pfnActivated = ( JournalTreeviewCb ) on_rows_activated;
	parms.pfnSelected = ( JournalTreeviewCb ) on_rows_selected;
	parms.user_data = dialog;

	priv->tview = ofa_journal_treeview_new( &parms );
	ofa_journal_treeview_init_view( priv->tview, NULL );

	label = ( GtkLabel * ) my_utils_container_get_child_by_name( container, "p1-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;

	memset( &date_parms, '\0', sizeof( date_parms ));
	date_parms.entry = my_utils_container_get_child_by_name( container, "p1-date" );
	date_parms.entry_format = MY_DATE_DDMM;
	date_parms.label = my_utils_container_get_child_by_name( container, "p1-label" );
	date_parms.label_format = MY_DATE_DMMM;
	date_parms.date = &priv->closing;
	date_parms.pfnCheck = ( myDateCheckCb ) date_check_cb;
	date_parms.on_changed_cb = G_CALLBACK( on_date_changed );
	date_parms.user_data = dialog;
	my_utils_date_parse_from_entry( &date_parms );

	priv->date_label = label;

	g_signal_connect( G_OBJECT( date_parms.entry ), "changed", G_CALLBACK( on_date_changed ), dialog );

	check_for_enable_dlg( OFA_INT_CLOSING( dialog ), NULL );
}

/*
 * JournalTreeview callback
 */
static void
on_rows_activated( GList *selected, ofaIntClosing *self )
{
	if( is_dialog_validable( self, selected )){
		do_close( self );
	}
}

/*
 * JournalTreeview callback
 */
static void
on_rows_selected( GList *selected, ofaIntClosing *self )
{
	check_for_enable_dlg( self, selected );
}

static gboolean
date_check_cb( GDate *date, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	const GDate *exe_end;

	priv = self->private;
	priv->valid = FALSE;

	/* the date must be less or equal that the end of exercice */
	exe_end = ofo_dossier_get_current_exe_fin( MY_WINDOW( self )->protected->dossier );
	if( !exe_end || !g_date_valid( exe_end ) || g_date_compare( date, exe_end ) <= 0 ){
		priv->valid = TRUE;
		gtk_label_set_text( priv->message_label, "" );

	} else {
		gtk_label_set_text( priv->message_label, _( "Closing date is after the end of exercice" ));
	}

	return( priv->valid );
}

static void
on_date_changed( GtkEntry *entry, ofaIntClosing *self )
{
	check_for_enable_dlg( self, NULL );
}

static void
check_for_enable_dlg( ofaIntClosing *self, GList *selected )
{
	gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->do_close_btn ),
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
	GList *isel;
	gboolean ok;

	priv = self->private;

	/* do we have a intrinsically valid proposed closing date ? */
	ok = priv->valid;

	/* check that each journal is not yet closed for this date */
	if( ok ){
		priv->count = 0;
		priv->closeable = 0;

		if( !selected ){
			selected = ofa_journal_treeview_get_selected( priv->tview );
		}

		for( isel=selected ; isel ; isel=isel->next ){
			check_foreach_journal( self, OFO_JOURNAL( isel->data ));
		}

		if( !priv->closeable ){
			gtk_label_set_text( priv->message_label, _( "None of the selected journals is closeable at the proposed date" ));
			ok = FALSE;
		}
	}
	if( ok ){
		gtk_label_set_text( priv->message_label, "" );
	}

	return( ok );
}

static void
check_foreach_journal( ofaIntClosing *self, ofoJournal *journal )
{
	ofaIntClosingPrivate *priv;
	const GDate *last;

	priv = self->private;
	priv->count += 1;

	last = ofo_journal_get_last_closing( journal );
	if( !last || !g_date_valid( last ) || g_date_compare( &priv->closing, last ) > 0 ){
		priv->closeable += 1;
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_close( OFA_INT_CLOSING( dialog )));
}

static gboolean
do_close( ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	GList *selected, *isel;
	gboolean ok;

	priv = self->private;
	selected = ofa_journal_treeview_get_selected( priv->tview );
	ok = is_dialog_validable( self, selected );

	g_return_val_if_fail( ok, FALSE );

	priv->count = 0;
	priv->jou_list = g_string_new( "" );

	for( isel=selected ; isel ; isel=isel->next ){
		close_foreach_journal( self, OFO_JOURNAL( isel->data ));
	}

	do_end_close( self );

	return( TRUE );
}

static gboolean
close_foreach_journal( ofaIntClosing *self, ofoJournal *journal )
{
	ofaIntClosingPrivate *priv;
	const gchar *mnemo;
	gchar *str;

	priv = self->private;

	mnemo = ofo_journal_get_mnemo( journal );

	str = g_strdup_printf( _( "Closing journal %s" ), mnemo );
	gtk_label_set_text( priv->message_label, str );
	g_free( str );

	priv->count += 1;

	if( g_utf8_strlen( priv->jou_list->str, -1 )){
		priv->jou_list = g_string_append( priv->jou_list, ", " );
	}
	g_string_append_printf( priv->jou_list, "%s", mnemo );

	return( ofo_journal_close( journal, &priv->closing ));
}

static void
do_end_close( ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	gchar *str;
	GtkWindow *toplevel;
	GtkWidget *button;
	GtkWidget *dialog;

	priv = self->private;
	toplevel = my_window_get_toplevel( MY_WINDOW( self ));

	gtk_label_set_text( priv->message_label, "" );

	gtk_widget_hide( GTK_WIDGET( priv->do_close_btn ));
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-cancel" );
	gtk_button_set_label( GTK_BUTTON( button ), _( "Close" ));

	str = g_strdup_printf(
				"%d journals (%s) successfully closed", priv->count, priv->jou_list->str );
	g_string_free( priv->jou_list, TRUE );

	dialog = gtk_message_dialog_new(
			toplevel,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			"%s", str );

	g_free( str );

	gtk_dialog_run( GTK_DIALOG( dialog ));
	gtk_widget_destroy( dialog );
}
