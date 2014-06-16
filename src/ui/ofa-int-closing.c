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
	gint                not_already_closed;
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-int-closing.ui";
static const gchar  *st_ui_id  = "IntClosingDlg";

G_DEFINE_TYPE( ofaIntClosing, ofa_int_closing, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_rows_activated( const gchar *mnemo, ofaIntClosing *self );
static void      on_rows_selected( const gchar *mnemo, ofaIntClosing *self );
static void      on_date_changed( GtkEntry *entry, ofaIntClosing *self );
static void      check_for_enable_dlg( ofaIntClosing *self );
static gboolean  is_dialog_validable( ofaIntClosing *self );
static void      check_foreach_journal( const gchar *mnemo, ofaIntClosing *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static void      do_close( ofaIntClosing *self );
static void      close_foreach_journal( const gchar *mnemo, ofaIntClosing *self );

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
	GtkButton *button;
	GtkEntry *entry;
	GtkLabel *label;

	priv = OFA_INT_CLOSING( dialog )->private;

	parms.main_window = MY_WINDOW( dialog )->protected->main_window;
	parms.parent = GTK_CONTAINER(
							my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
									"px-treeview-alignement" ));
	parms.allow_multiple_selection = TRUE;
	parms.pfnActivation = ( JournalTreeviewCb ) on_rows_activated;
	parms.pfnSelection = ( JournalTreeviewCb ) on_rows_selected;
	parms.user_data = dialog;

	priv->tview = ofa_journal_treeview_new( &parms );
	ofa_journal_treeview_init_view( priv->tview, NULL );

	button = ( GtkButton * ) my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
									"btn-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	priv->do_close_btn = button;

	label = ( GtkLabel * ) my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
									"p1-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->date_label = label;

	label = ( GtkLabel * ) my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
									"p1-message" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->message_label = label;

	entry = ( GtkEntry * ) my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog ))),
									"p1-date" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_date_changed ), dialog );

	check_for_enable_dlg( OFA_INT_CLOSING( dialog ));
}

/*
 * JournalTreeview callback
 */
static void
on_rows_activated( const gchar *mnemo, ofaIntClosing *self )
{
	g_debug( "on_rows_activated" );
}

/*
 * JournalTreeview callback
 */
static void
on_rows_selected( const gchar *mnemo, ofaIntClosing *self )
{
	g_debug( "on_rows_selected" );
}

static void
on_date_changed( GtkEntry *entry, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	gchar *str;
	const GDate *exe_end;

	priv = self->private;
	priv->valid = FALSE;

	g_date_set_parse( &priv->closing, gtk_entry_get_text( entry ));

	if( g_date_valid( &priv->closing )){
		str = my_utils_display_from_date( &priv->closing, MY_UTILS_DATE_DMMM );
		gtk_label_set_text( priv->date_label, str );
		g_free( str );

		/* the date must be less or equal that the end of exercice */
		exe_end = ofo_dossier_get_current_exe_fin( MY_WINDOW( self )->protected->dossier );
		if( !exe_end || !g_date_valid( exe_end ) || g_date_compare( &priv->closing, exe_end ) <= 0 ){
			priv->valid = TRUE;
			gtk_label_set_text( priv->message_label, "" );

		} else {
			gtk_label_set_text( priv->message_label, _( "Closing date is after the end of exercice" ));
		}

	} else {
		gtk_label_set_text( priv->date_label, "" );
		gtk_label_set_text( priv->message_label, _( "Closing date is invalid" ));
	}

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaIntClosing *self )
{
	gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->do_close_btn ),
				is_dialog_validable( self ));
}

/*
 * an intermediate cloture is allowed if the proposed closing date is
 * valid, and greater that at least one of the previous closing dates
 */
static gboolean
is_dialog_validable( ofaIntClosing *self )
{
	gboolean ok;
	ofaIntClosingPrivate *priv;

	priv = self->private;

	/* do we have a intrinsically valid proposed closing date ? */
	ok = priv->valid;

	/* check that each journal is not yet closed for this date */
	if( ok ){
		priv->count = 0;
		priv->not_already_closed = 0;

		ofa_journal_treeview_foreach_sel(
					priv->tview, ( JournalTreeviewCb ) check_foreach_journal, self );

		if( !priv->not_already_closed ){
			gtk_label_set_text( priv->message_label, _( "No journal found to be closeable at the proposed date" ));
			ok = FALSE;
		}
	}

	return( ok );
}

static void
check_foreach_journal( const gchar *mnemo, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	ofoJournal *journal;
	const GDate *last;

	priv = self->private;

	journal = ofo_journal_get_by_mnemo( MY_WINDOW( self )->protected->dossier, mnemo );
	priv->count += 1;

	if( journal ){
		last = ofo_journal_get_last_closing( journal );
		if( last && g_date_valid( last ) && g_date_compare( &priv->closing, last ) > 0 ){
			priv->not_already_closed += 1;
		}
	}
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	do_close( OFA_INT_CLOSING( dialog ));

	return( FALSE );
}

static void
do_close( ofaIntClosing *self )
{
	GtkWidget *button;

	gboolean ok;
	ok = is_dialog_validable( self );
	g_return_if_fail( ok );

	ofa_journal_treeview_foreach_sel(
				self->private->tview, ( JournalTreeviewCb ) close_foreach_journal, self );

	gtk_label_set_text( self->private->message_label, "" );

	gtk_widget_hide( GTK_WIDGET( self->private->do_close_btn ));

	button = my_utils_container_get_child_by_name(
									GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
									"btn-cancel" );
	gtk_button_set_label( GTK_BUTTON( button ), _( "Close" ));
}

static void
close_foreach_journal( const gchar *mnemo, ofaIntClosing *self )
{
	ofaIntClosingPrivate *priv;
	ofoJournal *journal;
	gchar *str;

	priv = self->private;

	journal = ofo_journal_get_by_mnemo( MY_WINDOW( self )->protected->dossier, mnemo );

	str = g_strdup_printf( _( "Closing journal %s" ), mnemo );
	gtk_label_set_text( priv->message_label, str );
	g_free( str );

	ofo_journal_close( journal, &priv->closing );
}
