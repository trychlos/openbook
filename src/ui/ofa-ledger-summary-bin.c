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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-ledger-summary-bin.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	const ofaMainWindow *main_window;

	/* UI
	 */
	ofaDateFilterHVBin  *date_filter;

	/* internals
	 */
}
	ofaLedgerSummaryBinPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ledger-summary-bin.ui";
static const gchar *st_settings         = "RenderLedgersSummary";

static void setup_bin( ofaLedgerSummaryBin *bin );
static void setup_date_selection( ofaLedgerSummaryBin *bin );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerSummaryBin *self );
static void load_settings( ofaLedgerSummaryBin *bin );
static void set_settings( ofaLedgerSummaryBin *bin );

G_DEFINE_TYPE_EXTENDED( ofaLedgerSummaryBin, ofa_ledger_summary_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaLedgerSummaryBin ))

static void
ledgers_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_summary_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_bin_parent_class )->finalize( instance );
}

static void
ledgers_book_bin_dispose( GObject *instance )
{
	ofaLedgerSummaryBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_BIN( instance ));

	priv = ofa_ledger_summary_bin_get_instance_private( OFA_LEDGER_SUMMARY_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_bin_parent_class )->dispose( instance );
}

static void
ofa_ledger_summary_bin_init( ofaLedgerSummaryBin *self )
{
	static const gchar *thisfn = "ofa_ledger_summary_bin_init";
	ofaLedgerSummaryBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_SUMMARY_BIN( self ));

	priv = ofa_ledger_summary_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ledger_summary_bin_class_init( ofaLedgerSummaryBinClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_summary_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_book_bin_finalize;

	/**
	 * ofaLedgerSummaryBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerSummaryBin *bin,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGER_SUMMARY_BIN,
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
 * ofa_ledger_summary_bin_new:
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Returns: a newly allocated #ofaLedgerSummaryBin object.
 */
ofaLedgerSummaryBin *
ofa_ledger_summary_bin_new( const ofaMainWindow *main_window )
{
	ofaLedgerSummaryBin *self;
	ofaLedgerSummaryBinPrivate *priv;

	self = g_object_new( OFA_TYPE_LEDGER_SUMMARY_BIN, NULL );

	priv = ofa_ledger_summary_bin_get_instance_private( self );
	priv->main_window = main_window;

	setup_bin( self );
	setup_date_selection( self );
	load_settings( self );

	return( self );
}

static void
setup_bin( ofaLedgerSummaryBin *bin )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "lbb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_date_selection( ofaLedgerSummaryBin *bin )
{
	ofaLedgerSummaryBinPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_ledger_summary_bin_get_instance_private( bin );

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
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerSummaryBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_ledger_summary_bin_is_valid:
 * @bin:
 * @msgerr: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_ledger_summary_bin_is_valid( ofaLedgerSummaryBin *bin, gchar **msgerr )
{
	ofaLedgerSummaryBinPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_SUMMARY_BIN( bin ), FALSE );

	priv = ofa_ledger_summary_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	valid = ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, msgerr ) &&
			ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, msgerr );

	if( valid ){
		set_settings( bin );
	}

	return( valid );
}

/**
 * ofa_ledger_summary_bin_get_date_filter:
 */
ofaIDateFilter *
ofa_ledger_summary_bin_get_date_filter( const ofaLedgerSummaryBin *bin )
{
	ofaLedgerSummaryBinPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_SUMMARY_BIN( bin ), NULL );

	priv = ofa_ledger_summary_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date_filter = OFA_IDATE_FILTER( priv->date_filter );

	return( date_filter );
}

/*
 * setttings:
 * from_date;to_date;
 */
static void
load_settings( ofaLedgerSummaryBin *bin )
{
	ofaLedgerSummaryBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = ofa_ledger_summary_bin_get_instance_private( bin );

	list = ofa_settings_user_get_string_list( st_settings );

	it = list;
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

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaLedgerSummaryBin *bin )
{
	ofaLedgerSummaryBinPrivate *priv;
	gchar *str, *sdfrom, *sdto;

	priv = ofa_ledger_summary_bin_get_instance_private( bin );

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "" );

	ofa_settings_user_set_string( st_settings, str );

	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
