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
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-dates-filter-hv-bin.h"
#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-ledgers-book-bin.h"

/* private instance data
 */
struct _ofaLedgersBookBinPrivate {
	gboolean             dispose_has_run;
	ofaMainWindow       *main_window;

	/* UI
	 */
	GtkWidget           *ledgers_parent;
	ofaLedgerTreeview   *ledgers_tview;
	GtkWidget           *all_ledgers_btn;
	GtkWidget           *new_page_btn;
	ofaDatesFilterHVBin *dates_filter;

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

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-ledgers-book-bin.ui";
static const gchar *st_ui_id            = "LedgersBookBin";
static const gchar *st_settings         = "RenderLedgersBook";

G_DEFINE_TYPE( ofaLedgersBookBin, ofa_ledgers_book_bin, GTK_TYPE_BIN )

static GtkWidget *load_dialog( ofaLedgersBookBin *bin );
static void       setup_ledger_selection( ofaLedgersBookBin *self, GtkContainer *parent );
static void       setup_date_selection( ofaLedgersBookBin *self, GtkContainer *parent );
static void       setup_others( ofaLedgersBookBin *self, GtkContainer *parent );
static void       on_tview_selection_changed( ofaLedgerTreeview *tview, GList *selected_mnemos, ofaLedgersBookBin *self );
static void       on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgersBookBin *self );
static void       on_new_page_toggled( GtkToggleButton *button, ofaLedgersBookBin *self );
static void       on_dates_filter_changed( ofaIDatesFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgersBookBin *self );
static void       load_settings( ofaLedgersBookBin *bin );
static void       set_settings( ofaLedgersBookBin *bin );

static void
ledgers_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledgers_book_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGERS_BOOK_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledgers_book_bin_parent_class )->finalize( instance );
}

static void
ledgers_book_bin_dispose( GObject *instance )
{
	ofaLedgersBookBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGERS_BOOK_BIN( instance ));

	priv = OFA_LEDGERS_BOOK_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledgers_book_bin_parent_class )->dispose( instance );
}

static void
ofa_ledgers_book_bin_init( ofaLedgersBookBin *self )
{
	static const gchar *thisfn = "ofa_ledgers_book_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGERS_BOOK_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_LEDGERS_BOOK_BIN, ofaLedgersBookBinPrivate );
}

static void
ofa_ledgers_book_bin_class_init( ofaLedgersBookBinClass *klass )
{
	static const gchar *thisfn = "ofa_ledgers_book_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_book_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaLedgersBookBinPrivate ));

	/**
	 * ofaLedgersBookBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgersBookBin *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGERS_BOOK_BIN,
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
 * ofa_ledgers_book_bin_new:
 * @main_window:
 *
 * Returns: a newly allocated #ofaLedgersBookBin object.
 */
ofaLedgersBookBin *
ofa_ledgers_book_bin_new( ofaMainWindow *main_window )
{
	ofaLedgersBookBin *self;
	GtkWidget *parent;

	self = g_object_new( OFA_TYPE_LEDGERS_BOOK_BIN, NULL );

	self->priv->main_window = main_window;

	parent = load_dialog( self );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), NULL );

	setup_ledger_selection( self, GTK_CONTAINER( parent ));
	setup_date_selection( self, GTK_CONTAINER( parent ));
	setup_others( self, GTK_CONTAINER( parent ));

	load_settings( self );

	return( self );
}

/*
 * returns: the GtkContainer parent
 */
static GtkWidget *
load_dialog( ofaLedgersBookBin *bin )
{
	GtkWidget *top_widget;

	top_widget = my_utils_container_attach_from_ui( GTK_CONTAINER( bin ), st_ui_xml, st_ui_id, "top" );
	g_return_val_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ), NULL );

	return( top_widget );
}

static void
setup_ledger_selection( ofaLedgersBookBin *self, GtkContainer *parent )
{
	ofaLedgersBookBinPrivate *priv;
	GtkWidget *widget, *toggle;

	priv = self->priv;

	widget = my_utils_container_get_child_by_name( parent, "p1-ledger" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));
	priv->ledgers_parent = widget;

	priv->ledgers_tview = ofa_ledger_treeview_new();
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->ledgers_tview ));
	ofa_ledger_treeview_set_hexpand( priv->ledgers_tview, FALSE );
	ofa_ledger_treeview_set_columns( priv->ledgers_tview,
			LEDGER_DISP_MNEMO | LEDGER_DISP_LAST_ENTRY | LEDGER_DISP_LAST_CLOSE );
	ofa_ledger_treeview_set_main_window( priv->ledgers_tview, priv->main_window );
	ofa_ledger_treeview_set_selection_mode( priv->ledgers_tview, GTK_SELECTION_MULTIPLE );

	g_signal_connect( priv->ledgers_tview, "ofa-changed", G_CALLBACK( on_tview_selection_changed ), self );

	toggle = my_utils_container_get_child_by_name( parent, "p1-all-ledgers" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_all_ledgers_toggled ), self );
	priv->all_ledgers_btn = toggle;
}

static void
setup_date_selection( ofaLedgersBookBin *self, GtkContainer *parent )
{
	ofaLedgersBookBinPrivate *priv;
	GtkWidget *alignment, *label;
	ofaDatesFilterHVBin *bin;

	priv = self->priv;

	alignment = my_utils_container_get_child_by_name( parent, "dates-filter" );
	g_return_if_fail( alignment && GTK_IS_CONTAINER( alignment ));

	bin = ofa_dates_filter_hv_bin_new();
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( bin ));

	/* instead of "effect dates filter" */
	label = ofa_idates_filter_get_frame_label( OFA_IDATES_FILTER( bin ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_dates_filter_changed ), self );

	priv->dates_filter = bin;
}

static void
setup_others( ofaLedgersBookBin *self, GtkContainer *parent )
{
	ofaLedgersBookBinPrivate *priv;
	GtkWidget *toggle;

	priv = self->priv;

	toggle = my_utils_container_get_child_by_name( parent, "p3-new-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = toggle;
}

static void
on_tview_selection_changed( ofaLedgerTreeview *tview, GList *selected_mnemos, ofaLedgersBookBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgersBookBin *self )
{
	ofaLedgersBookBinPrivate *priv;

	priv = self->priv;

	priv->all_ledgers = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->ledgers_parent, !priv->all_ledgers );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaLedgersBookBin *self )
{
	ofaLedgersBookBinPrivate *priv;

	priv = self->priv;

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_dates_filter_changed( ofaIDatesFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgersBookBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_ledgers_book_bin_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_ledgers_book_bin_is_valid( ofaLedgersBookBin *bin, gchar **message )
{
	ofaLedgersBookBinPrivate *priv;
	gboolean valid;
	GList *selected;

	g_return_val_if_fail( bin && OFA_IS_LEDGERS_BOOK_BIN( bin ), FALSE );

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
			valid = ofa_idates_filter_is_valid(
							OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM, message ) &&
					ofa_idates_filter_is_valid(
							OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO, message );
		}

		if( valid ){
			set_settings( bin );
		}
	}

	return( valid );
}

/**
 * ofa_ledgers_book_bin_get_treeview:
 * @bin:
 */
ofaLedgerTreeview *
ofa_ledgers_book_bin_get_treeview( const ofaLedgersBookBin *bin )
{
	ofaLedgersBookBinPrivate *priv;
	ofaLedgerTreeview *tview;

	g_return_val_if_fail( bin && OFA_IS_LEDGERS_BOOK_BIN( bin ), NULL );

	priv = bin->priv;
	tview = NULL;

	if( !priv->dispose_has_run ){

		tview = priv->ledgers_tview;
	}

	return( tview );
}

/**
 * ofa_ledgers_book_bin_get_all_ledgers:
 * @bin:
 */
gboolean
ofa_ledgers_book_bin_get_all_ledgers( const ofaLedgersBookBin *bin )
{
	ofaLedgersBookBinPrivate *priv;
	gboolean all_ledgers;

	g_return_val_if_fail( bin && OFA_IS_LEDGERS_BOOK_BIN( bin ), FALSE );

	priv = bin->priv;
	all_ledgers = FALSE;

	if( !priv->dispose_has_run ){

		all_ledgers = priv->all_ledgers;
	}

	return( all_ledgers );
}

/**
 * ofa_ledgers_book_bin_get_new_page_per_ledger:
 * @bin:
 */
gboolean
ofa_ledgers_book_bin_get_new_page_per_ledger( const ofaLedgersBookBin *bin )
{
	ofaLedgersBookBinPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_LEDGERS_BOOK_BIN( bin ), FALSE );

	priv = bin->priv;
	new_page = FALSE;

	if( !priv->dispose_has_run ){

		new_page = priv->new_page;
	}

	return( new_page );
}

/**
 * ofa_ledgers_book_bin_get_date_filter:
 */
ofaIDatesFilter *
ofa_ledgers_book_bin_get_dates_filter( const ofaLedgersBookBin *bin )
{
	ofaLedgersBookBinPrivate *priv;
	ofaIDatesFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_LEDGERS_BOOK_BIN( bin ), NULL );

	priv = bin->priv;
	date_filter = NULL;

	if( !priv->dispose_has_run ){

		date_filter = OFA_IDATES_FILTER( priv->dates_filter );
	}

	return( date_filter );
}

/*
 * setttings:
 * all_ledgers;from_date;to_date;new_page;
 */
static void
load_settings( ofaLedgersBookBin *bin )
{
	ofaLedgersBookBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = bin->priv;
	list = ofa_settings_get_string_list( st_settings );

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
		ofa_idates_filter_set_date(
				OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idates_filter_set_date(
				OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO, &date );
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
set_settings( ofaLedgersBookBin *bin )
{
	ofaLedgersBookBinPrivate *priv;
	gchar *str, *sdfrom, *sdto;

	priv = bin->priv;

	sdfrom = my_date_to_str(
			ofa_idates_filter_get_date(
					OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idates_filter_get_date(
					OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;",
			priv->all_ledgers ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "",
			priv->new_page ? "True":"False" );

	ofa_settings_set_string( st_settings, str );

	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
