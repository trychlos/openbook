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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-ledger-summary-args.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;
	gchar              *settings_prefix;

	/* runtime
	 */
	myISettings        *settings;

	/* UI
	 */
	ofaDateFilterHVBin *date_filter;
}
	ofaLedgerSummaryArgsPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ledger-summary-args.ui";

static void setup_runtime( ofaLedgerSummaryArgs *self );
static void setup_bin( ofaLedgerSummaryArgs *self );
static void setup_date_selection( ofaLedgerSummaryArgs *self );
static void on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerSummaryArgs *self );
static void read_settings( ofaLedgerSummaryArgs *self );
static void write_settings( ofaLedgerSummaryArgs *self );

G_DEFINE_TYPE_EXTENDED( ofaLedgerSummaryArgs, ofa_ledger_summary_args, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaLedgerSummaryArgs ))

static void
ledgers_book_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_summary_args_finalize";
	ofaLedgerSummaryArgsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_ARGS( instance ));

	/* free data members here */
	priv = ofa_ledger_summary_args_get_instance_private( OFA_LEDGER_SUMMARY_ARGS( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_args_parent_class )->finalize( instance );
}

static void
ledgers_book_bin_dispose( GObject *instance )
{
	ofaLedgerSummaryArgsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_LEDGER_SUMMARY_ARGS( instance ));

	priv = ofa_ledger_summary_args_get_instance_private( OFA_LEDGER_SUMMARY_ARGS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_summary_args_parent_class )->dispose( instance );
}

static void
ofa_ledger_summary_args_init( ofaLedgerSummaryArgs *self )
{
	static const gchar *thisfn = "ofa_ledger_summary_args_init";
	ofaLedgerSummaryArgsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_SUMMARY_ARGS( self ));

	priv = ofa_ledger_summary_args_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_ledger_summary_args_class_init( ofaLedgerSummaryArgsClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_summary_args_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledgers_book_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledgers_book_bin_finalize;

	/**
	 * ofaLedgerSummaryArgs::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaLedgerSummaryArgs *bin,
	 * 						gpointer           user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_LEDGER_SUMMARY_ARGS,
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
 * ofa_ledger_summary_args_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the key in user settings.
 *
 * Returns: a newly allocated #ofaLedgerSummaryArgs object.
 */
ofaLedgerSummaryArgs *
ofa_ledger_summary_args_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaLedgerSummaryArgs *bin;
	ofaLedgerSummaryArgsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	bin = g_object_new( OFA_TYPE_LEDGER_SUMMARY_ARGS, NULL );

	priv = ofa_ledger_summary_args_get_instance_private( bin );

	priv->getter = getter;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_runtime( bin );
	setup_bin( bin );
	setup_date_selection( bin );
	read_settings( bin );

	return( bin );
}

static void
setup_runtime( ofaLedgerSummaryArgs *self )
{
	ofaLedgerSummaryArgsPrivate *priv;

	priv = ofa_ledger_summary_args_get_instance_private( self );

	priv->settings = ofa_igetter_get_user_settings( priv->getter );
}

static void
setup_bin( ofaLedgerSummaryArgs *self )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "lbb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_date_selection( ofaLedgerSummaryArgs *self )
{
	ofaLedgerSummaryArgsPrivate *priv;
	GtkWidget *parent, *label;
	ofaDateFilterHVBin *filter;

	priv = ofa_ledger_summary_args_get_instance_private( self );

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-filter" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	filter = ofa_date_filter_hv_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( filter ));

	/* instead of "effect dates filter" */
	label = ofa_idate_filter_get_frame_label( OFA_IDATE_FILTER( filter ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( G_OBJECT( filter ), "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );

	priv->date_filter = filter;
}

static void
on_date_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, gboolean valid, ofaLedgerSummaryArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_ledger_summary_args_is_valid:
 * @bin:
 * @msgerr: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_ledger_summary_args_is_valid( ofaLedgerSummaryArgs *bin, gchar **msgerr )
{
	ofaLedgerSummaryArgsPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_SUMMARY_ARGS( bin ), FALSE );

	priv = ofa_ledger_summary_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	valid = ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM, msgerr ) &&
			ofa_idate_filter_is_valid(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO, msgerr );

	if( valid ){
		write_settings( bin );
	}

	return( valid );
}

/**
 * ofa_ledger_summary_args_get_date_filter:
 */
ofaIDateFilter *
ofa_ledger_summary_args_get_date_filter( ofaLedgerSummaryArgs *bin )
{
	ofaLedgerSummaryArgsPrivate *priv;
	ofaIDateFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_LEDGER_SUMMARY_ARGS( bin ), NULL );

	priv = ofa_ledger_summary_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date_filter = OFA_IDATE_FILTER( priv->date_filter );

	return( date_filter );
}

/*
 * setttings:
 * from_date;to_date;
 */
static void
read_settings( ofaLedgerSummaryArgs *self )
{
	ofaLedgerSummaryArgsPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;
	gchar *key;

	priv = ofa_ledger_summary_args_get_instance_private( self );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	strlist = my_isettings_get_string_list( priv->settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
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

	my_isettings_free_string_list( priv->settings, strlist );
	g_free( key );
}

static void
write_settings( ofaLedgerSummaryArgs *self )
{
	ofaLedgerSummaryArgsPrivate *priv;
	gchar *str, *sdfrom, *sdto, *key;

	priv = ofa_ledger_summary_args_get_instance_private( self );

	sdfrom = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idate_filter_get_date(
					OFA_IDATE_FILTER( priv->date_filter ), IDATE_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdto : "" );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	my_isettings_set_string( priv->settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
