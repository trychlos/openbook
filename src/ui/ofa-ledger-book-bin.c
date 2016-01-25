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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-ledger-book-bin.h"

/* private instance data
 */
struct _ofaLedgerBookBinPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */
	const ofaMainWindow *main_window;

	/* UI
	 */
	GtkWidget           *ledgers_parent;
	ofaLedgerTreeview   *ledgers_tview;
	GtkWidget           *all_ledgers_btn;
	GtkWidget           *new_page_btn;
	ofaDateFilterHVBin  *date_filter;

	/* internals
	 */
	gboolean             all_ledgers;
	gboolean             new_page;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-ledger-book-bin.ui";
static const gchar *st_settings         = "RenderLedgersBook";

G_DEFINE_TYPE( ofaLedgerBookBin, ofa_ledger_book_bin, GTK_TYPE_BIN )

static void setup_bin( ofaLedgerBookBin *bin );
static void setup_ledger_selection( ofaLedgerBookBin *bin );
static void setup_date_selection( ofaLedgerBookBin *bin );
static void setup_others( ofaLedgerBookBin *bin );
static void on_tview_selection_changed( ofaLedgerTreeview *tview, GList *selected_mnemos, ofaLedgerBookBin *self );
static void on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerBookBin *self );
static void on_new_page_toggled( GtkToggleButton *button, ofaLedgerBookBin *self );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerBookBin *self );
static void load_settings( ofaLedgerBookBin *bin );
static void set_settings( ofaLedgerBookBin *bin );

static void
ledgers_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_book_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_BOOK_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_book_bin_parent_class )->finalize( instance );
}

static void
ledgers_book_bin_dispose( GObject *instance )
{
	ofaLedgerBookBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_BOOK_BIN( instance ));

	priv = OFA_LEDGER_BOOK_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_book_bin_parent_class )->dispose( instance );
}

static void
ofa_ledger_book_bin_init( ofaLedgerBookBin *self )
{
	static const gchar *thisfn = "ofa_ledger_book_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_BOOK_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_LEDGER_BOOK_BIN, ofaLedgerBookBinPrivate );
}

static void
ofa_ledger_book_bin_class_init( ofaLedgerBookBinClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_book_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_book_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaLedgerBookBinPrivate ));

	/**
	 * ofaLedgerBookBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerBookBin *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGER_BOOK_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );
}

/**
 * ofa_ledger_book_bin_new:
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Returns: a newly allocated #ofaLedgerBookBin object.
 */
ofaLedgerBookBin *
ofa_ledger_book_bin_new( const ofaMainWindow *main_window )
{
	ofaLedgerBookBin *self;

	self = g_object_new( OFA_TYPE_LEDGER_BOOK_BIN, NULL );

	self->priv->main_window = main_window;

	setup_bin( self );
	setup_ledger_selection( self );
	setup_date_selection( self );
	setup_others( self );

	load_settings( self );

	return( self );
}

static void
setup_bin( ofaLedgerBookBin *bin )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "lbb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_ledger_selection( ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	GtkWidget *widget, *toggle, *label;
	GtkApplication *application;
	ofaHub *hub;

	priv = bin->priv;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-ledger" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));
	priv->ledgers_parent = widget;

	application = gtk_window_get_application( GTK_WINDOW( priv->main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv->ledgers_tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->ledgers_tview ));
	ofa_ledger_treeview_set_hexpand( priv->ledgers_tview, FALSE );
	ofa_ledger_treeview_set_columns( priv->ledgers_tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_hub( priv->ledgers_tview, hub );
	ofa_ledger_treeview_set_selection_mode( priv->ledgers_tview, GTK_SELECTION_MULTIPLE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-frame-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), ofa_ledger_treeview_get_treeview( priv->ledgers_tview ));

	g_signal_connect( priv->ledgers_tview, "ofa-changed", G_CALLBACK( on_tview_selection_changed ), bin );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p1-all-ledgers" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_all_ledgers_toggled ), bin );
	priv->all_ledgers_btn = toggle;
}

static void
setup_date_selection( ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = bin->priv;

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( G_OBJECT( filter ), "ofa-changed", G_CALLBACK( on_date_filter_changed ), bin );

	priv->date_filter = filter;
}

static void
setup_others( ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	GtkWidget *toggle;

	priv = bin->priv;

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "p3-new-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_new_page_toggled ), bin );
	priv->new_page_btn = toggle;
}

static void
on_tview_selection_changed( ofaLedgerTreeview *tview, GList *selected_mnemos, ofaLedgerBookBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerBookBin *self )
{
	ofaLedgerBookBinPrivate *priv;

	priv = self->priv;

	priv->all_ledgers = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->ledgers_parent, !priv->all_ledgers );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaLedgerBookBin *self )
{
	ofaLedgerBookBinPrivate *priv;

	priv = self->priv;

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerBookBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_ledger_book_bin_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_ledger_book_bin_is_valid( ofaLedgerBookBin *bin, gchar **message )
{
	ofaLedgerBookBinPrivate *priv;
	gboolean valid;
	GList *selected;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_BIN( bin ), FALSE );

	priv = bin->priv;
	valid = FALSE;
	if( message ){
		*message = NULL;
	}

	if( !priv->dispose_has_run ){

		if( priv->all_ledgers ){
			valid = TRUE;
		} else {
			selected = ofa_ledger_treeview_get_selected( priv->ledgers_tview );
			valid = ( g_list_length( selected ) > 0 );
			ofa_ledger_treeview_free_selected( selected );
			if( !valid && message ){
				*message = g_strdup( _( "No ledger selected" ));
			}
		}

		if( valid ){
			valid = ofa_idate_filter_is_valid(
							OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, message ) &&
					ofa_idate_filter_is_valid(
							OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, message );
		}

		if( valid ){
			set_settings( bin );
		}
	}

	return( valid );
}

/**
 * ofa_ledger_book_bin_get_treeview:
 * @bin:
 */
ofaLedgerTreeview *
ofa_ledger_book_bin_get_treeview( const ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	ofaLedgerTreeview *tview;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_BIN( bin ), NULL );

	priv = bin->priv;
	tview = NULL;

	if( !priv->dispose_has_run ){

		tview = priv->ledgers_tview;
	}

	return( tview );
}

/**
 * ofa_ledger_book_bin_get_all_ledgers:
 * @bin:
 */
gboolean
ofa_ledger_book_bin_get_all_ledgers( const ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	gboolean all_ledgers;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_BIN( bin ), FALSE );

	priv = bin->priv;
	all_ledgers = FALSE;

	if( !priv->dispose_has_run ){

		all_ledgers = priv->all_ledgers;
	}

	return( all_ledgers );
}

/**
 * ofa_ledger_book_bin_get_new_page_per_ledger:
 * @bin:
 */
gboolean
ofa_ledger_book_bin_get_new_page_per_ledger( const ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_BIN( bin ), FALSE );

	priv = bin->priv;
	new_page = FALSE;

	if( !priv->dispose_has_run ){

		new_page = priv->new_page;
	}

	return( new_page );
}

/**
 * ofa_ledger_book_bin_get_date_filter:
 */
ofaIDateFilter *
ofa_ledger_book_bin_get_date_filter( const ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_BIN( bin ), NULL );

	priv = bin->priv;
	date_filter = NULL;

	if( !priv->dispose_has_run ){

		date_filter = OFA_IDATE_FILTER( priv->date_filter );
	}

	return( date_filter );
}

/*
 * setttings:
 * all_ledgers;from_date;to_date;new_page;
 */
static void
load_settings( ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = bin->priv;
	list = ofa_settings_user_get_string_list( st_settings );

	it = list;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->all_ledgers_btn ), my_utils_boolean_from_str( cstr ));
		on_all_ledgers_toggled( GTK_TOGGLE_BUTTON( priv->all_ledgers_btn ), bin );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date(
				OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date(
				OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->new_page_btn ), my_utils_boolean_from_str( cstr ));
		on_new_page_toggled( GTK_TOGGLE_BUTTON( priv->new_page_btn ), bin );
	}

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaLedgerBookBin *bin )
{
	ofaLedgerBookBinPrivate *priv;
	gchar *str, *sdfrom, *sdto;

	priv = bin->priv;

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;",
			priv->all_ledgers ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "",
			priv->new_page ? "True":"False" );

	ofa_settings_user_set_string( st_settings, str );

	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
