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
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-ledger-treeview.h"
#include "ui/ofa-ledger-book-args.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	gchar               *settings_prefix;

	/* runtime
	 */
	myISettings         *settings;
	gboolean             all_ledgers;
	gboolean             new_page;
	gboolean             with_summary;
	gboolean             only_summary;

	/* UI
	 */
	GtkWidget           *vpane;
	GtkWidget           *ledgers_parent;
	ofaLedgerTreeview   *tview;
	GtkWidget           *all_ledgers_btn;
	ofaDateFilterHVBin  *date_filter;
	GtkWidget           *new_page_btn;
	GtkWidget           *with_summary_btn;
	GtkWidget           *only_summary_btn;
}
	ofaLedgerBookArgsPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ledger-book-args.ui";

static void setup_runtime( ofaLedgerBookArgs *self );
static void setup_bin( ofaLedgerBookArgs *self );
static void setup_ledger_selection( ofaLedgerBookArgs *self );
static void setup_date_selection( ofaLedgerBookArgs *self );
static void setup_others( ofaLedgerBookArgs *self );
static void setup_actions( ofaLedgerBookArgs *self );
static void on_tview_selection_changed( ofaLedgerTreeview *tview, void *unused, ofaLedgerBookArgs *self );
static void on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerBookArgs *self );
static void on_new_page_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self );
static void on_with_summary_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self );
static void on_only_summary_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self );
static void read_settings( ofaLedgerBookArgs *bin );
static void write_settings( ofaLedgerBookArgs *bin );

G_DEFINE_TYPE_EXTENDED( ofaLedgerBookArgs, ofa_ledger_book_args, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaLedgerBookArgs ))

static void
ledgers_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_book_args_finalize";
	ofaLedgerBookArgsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_BOOK_ARGS( instance ));

	/* free data members here */
	priv = ofa_ledger_book_args_get_instance_private( OFA_LEDGER_BOOK_ARGS( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_book_args_parent_class )->finalize( instance );
}

static void
ledgers_book_bin_dispose( GObject *instance )
{
	ofaLedgerBookArgsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_BOOK_ARGS( instance ));

	priv = ofa_ledger_book_args_get_instance_private( OFA_LEDGER_BOOK_ARGS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_book_args_parent_class )->dispose( instance );
}

static void
ofa_ledger_book_args_init( ofaLedgerBookArgs *self )
{
	static const gchar *thisfn = "ofa_ledger_book_args_init";
	ofaLedgerBookArgsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_BOOK_ARGS( self ));

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_ledger_book_args_class_init( ofaLedgerBookArgsClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_book_args_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_book_bin_finalize;

	/**
	 * ofaLedgerBookArgs::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerBookArgs *bin,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGER_BOOK_ARGS,
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
 * ofa_ledger_book_args_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the key in user settings.
 *
 * Returns: a newly allocated #ofaLedgerBookArgs object.
 */
ofaLedgerBookArgs *
ofa_ledger_book_args_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaLedgerBookArgs *self;
	ofaLedgerBookArgsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	self = g_object_new( OFA_TYPE_LEDGER_BOOK_ARGS, NULL );

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->getter = getter;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_runtime( self );
	setup_bin( self );
	setup_ledger_selection( self );
	setup_date_selection( self );
	setup_others( self );
	setup_actions( self );

	read_settings( self );

	return( self );
}

static void
setup_runtime( ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->settings = ofa_igetter_get_user_settings( priv->getter );
}

static void
setup_bin( ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *pane;

	priv = ofa_ledger_book_args_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "lbb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	pane = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "top" );
	g_return_if_fail( pane && GTK_IS_PANED( pane ));
	priv->vpane = pane;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_ledger_selection( ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;
	GtkWidget *widget, *toggle, *label;

	priv = ofa_ledger_book_args_get_instance_private( self );

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-ledger" );
	g_return_if_fail( widget && GTK_IS_CONTAINER( widget ));
	priv->ledgers_parent = widget;

	priv->tview = ofa_ledger_treeview_new( priv->getter, priv->settings_prefix );
	gtk_container_add( GTK_CONTAINER( widget ), GTK_WIDGET( priv->tview ));
	ofa_tvbin_set_hexpand( OFA_TVBIN( priv->tview ), FALSE );
	ofa_ledger_treeview_setup_store( priv->tview );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-frame-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), ofa_tvbin_get_tree_view( OFA_TVBIN( priv->tview )));

	g_signal_connect( priv->tview, "ofa-selchanged", G_CALLBACK( on_tview_selection_changed ), self );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-all-ledgers" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_all_ledgers_toggled ), self );
	priv->all_ledgers_btn = toggle;
}

static void
setup_date_selection( ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_ledger_book_args_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( filter, "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );

	priv->date_filter = filter;
}

static void
setup_others( ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;
	GtkWidget *toggle;

	priv = ofa_ledger_book_args_get_instance_private( self );

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-new-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = toggle;

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-with-summary" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_with_summary_toggled ), self );
	priv->with_summary_btn = toggle;

	toggle = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-only-summary" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( toggle, "toggled", G_CALLBACK( on_only_summary_toggled ), self );
	priv->only_summary_btn = toggle;
}

static void
setup_actions( ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;
	GMenu *menu;

	priv = ofa_ledger_book_args_get_instance_private( self );

	menu = ofa_itvcolumnable_get_menu( OFA_ITVCOLUMNABLE( priv->tview ));
	ofa_icontext_set_menu(
			OFA_ICONTEXT( priv->tview ), OFA_IACTIONABLE( priv->tview ),
			menu );
}

static void
on_tview_selection_changed( ofaLedgerTreeview *tview, void *unused, ofaLedgerBookArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_all_ledgers_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->all_ledgers = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->ledgers_parent, !priv->all_ledgers );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerBookArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_with_summary_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->with_summary = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->only_summary_btn, priv->with_summary );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_only_summary_toggled( GtkToggleButton *button, ofaLedgerBookArgs *self )
{
	ofaLedgerBookArgsPrivate *priv;

	priv = ofa_ledger_book_args_get_instance_private( self );

	priv->only_summary = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_ledger_book_args_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_ledger_book_args_is_valid( ofaLedgerBookArgs *bin, gchar **message )
{
	ofaLedgerBookArgsPrivate *priv;
	gboolean valid;
	GList *selected;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), FALSE );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( message ){
		*message = NULL;
	}

	if( priv->all_ledgers ){
		valid = TRUE;

	} else {
		selected = ofa_ledger_treeview_get_selected( priv->tview );
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
		write_settings( bin );
	}

	return( valid );
}

/**
 * ofa_ledger_book_args_get_treeview:
 * @bin: this #ofaLedgerBookBin widget.
 *
 * Returns: the #ofaLedgerTreeview widget.
 */
ofaLedgerTreeview *
ofa_ledger_book_args_get_treeview( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;
	ofaLedgerTreeview *tview;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), NULL );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	tview = priv->tview;

	return( tview );
}

/**
 * ofa_ledger_book_args_get_all_ledgers:
 * @bin: this #ofaLedgerBookBin widget.
 *
 * Returns: whether the user wants all ledgers.
 */
gboolean
ofa_ledger_book_args_get_all_ledgers( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;
	gboolean all_ledgers;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), FALSE );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	all_ledgers = priv->all_ledgers;

	return( all_ledgers );
}

/**
 * ofa_ledger_book_args_get_date_filter:
 * @bin: this #ofaLedgerBookBin widget.
 *
 * Returns: the #ofaIDateFilter widget.
 */
ofaIDateFilter *
ofa_ledger_book_args_get_date_filter( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), NULL );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	date_filter = OFA_IDATE_FILTER( priv->date_filter );

	return( date_filter );
}

/**
 * ofa_ledger_book_args_get_new_page_per_ledger:
 * @bin: this #ofaLedgerBookBin widget.
 *
 * Returns: whether the user wants a new page per ledger.
 */
gboolean
ofa_ledger_book_args_get_new_page_per_ledger( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), FALSE );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->new_page );
}

/**
 * ofa_ledger_book_args_get_with_summary:
 * @bin: this #ofaLedgerBookBin widget.
 *
 * Returns: whether the user wants a summary.
 */
gboolean
ofa_ledger_book_args_get_with_summary( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), FALSE );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->with_summary );
}

/**
 * ofa_ledger_book_args_get_only_summary:
 * @bin: this #ofaLedgerBookBin widget.
 *
 * Returns: whether the user wants only the summary.
 *
 * Note: we only return the value if the user has also requested the
 * summary. Else, we return %FALSE.
 */
gboolean
ofa_ledger_book_args_get_only_summary( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_BOOK_ARGS( bin ), FALSE );

	priv = ofa_ledger_book_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->with_summary ? priv->only_summary : FALSE );
}

/*
 * setttings:
 * all_ledgers;from_date;to_date;new_page;paned_pos;with_summary;only_summary;
 */
static void
read_settings( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;
	gchar *key;

	priv = ofa_ledger_book_args_get_instance_private( bin );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	strlist = my_isettings_get_string_list( priv->settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->all_ledgers_btn ), my_utils_boolean_from_str( cstr ));
		on_all_ledgers_toggled( GTK_TOGGLE_BUTTON( priv->all_ledgers_btn ), bin );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date(
				OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		ofa_idate_filter_set_date(
				OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, &date );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->new_page_btn ), my_utils_boolean_from_str( cstr ));
		on_new_page_toggled( GTK_TOGGLE_BUTTON( priv->new_page_btn ), bin );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_paned_set_position( GTK_PANED( priv->vpane ), atoi( cstr ));
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->with_summary_btn ), my_utils_boolean_from_str( cstr ));
		on_with_summary_toggled( GTK_TOGGLE_BUTTON( priv->with_summary_btn ), bin );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->only_summary_btn ), my_utils_boolean_from_str( cstr ));
	}
	on_only_summary_toggled( GTK_TOGGLE_BUTTON( priv->only_summary_btn ), bin );

	my_isettings_free_string_list( priv->settings, strlist );
	g_free( key );
}

static void
write_settings( ofaLedgerBookArgs *bin )
{
	ofaLedgerBookArgsPrivate *priv;
	gchar *key, *str, *sdfrom, *sdto;

	priv = ofa_ledger_book_args_get_instance_private( bin );

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;%d;%s;%s;",
			priv->all_ledgers ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "",
			priv->new_page ? "True":"False",
			gtk_paned_get_position( GTK_PANED( priv->vpane )),
			priv->with_summary ? "True":"False",
			priv->only_summary ? "True":"False" );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	my_isettings_set_string( priv->settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
