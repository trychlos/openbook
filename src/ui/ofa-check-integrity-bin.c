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
#include <math.h>
#include <stdlib.h>

#include "my/my-iprogress.h"
#include "my/my-progress-bar.h"
#include "my/my-style.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-igetter.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-class.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofs-currency.h"

#include "ui/ofa-check-integrity-bin.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* initialization
	 */
	ofaIGetter    *getter;
	gchar         *settings_prefix;

	/* runtime
	 */
	gboolean       display;

	gulong         dossier_errs;
	gulong         bat_lines_errs;
	gulong         accounts_errs;
	gulong         concil_errs;
	gulong         entries_errs;
	gulong         ledgers_errs;
	gulong         ope_templates_errs;
	gulong         others_errs;

	gulong         total_errs;

	GList         *workers;

	/* UI
	 */
	GtkWidget     *paned;
	GtkWidget     *upper_viewport;
	GtkWidget     *objects_grid;
	gint           objects_row;
	GtkWidget     *text_view;
	GtkTextBuffer *text_buffer;
}
	ofaCheckIntegrityBinPrivate;

/* a data structure defined for each worker
 */
typedef struct {
	const void    *worker;
	GtkWidget     *grid;
	myProgressBar *bar;
}
	sWorker;

/* signals defined here
 */
enum {
	DONE = 0,
	N_SIGNALS
};

#define SCROLLBAR_WIDTH                   16

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-check-integrity-bin.ui";
static const gchar *st_settings_sufix   = "bin";

static void     setup_bin( ofaCheckIntegrityBin *self );
static void     read_settings( ofaCheckIntegrityBin *self );
static void     write_settings( ofaCheckIntegrityBin *self );
static gchar   *get_settings_key( ofaCheckIntegrityBin *self );
static gboolean do_run( ofaCheckIntegrityBin *self );
static void     check_dossier_run( ofaCheckIntegrityBin *self );
static void     check_bat_lines_run( ofaCheckIntegrityBin *self );
static void     check_accounts_run( ofaCheckIntegrityBin *self );
static void     check_concil_run( ofaCheckIntegrityBin *self );
static void     check_entries_run( ofaCheckIntegrityBin *self );
static void     check_ledgers_run( ofaCheckIntegrityBin *self );
static void     check_ope_templates_run( ofaCheckIntegrityBin *self );
static void     set_checks_result( ofaCheckIntegrityBin *self );
static void     on_grid_size_allocate( GtkWidget *grid, GdkRectangle *allocation, ofaCheckIntegrityBin *self );
static void     iprogress_iface_init( myIProgressInterface *iface );
static void     iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget );
static void     iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar );
static void     iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total );
static void     iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count );
static void     iprogress_set_text( myIProgress *instance, const void *worker, const gchar *text );
static sWorker *get_worker_data( ofaCheckIntegrityBin *self, const void *worker );

/* intern function
 */
typedef void ( *checkfn )( ofaCheckIntegrityBin *self );

static checkfn st_fn[] = {
		check_dossier_run,
		check_bat_lines_run,
		check_accounts_run,
		check_concil_run,
		check_entries_run,
		check_ledgers_run,
		check_ope_templates_run,
		0
};

G_DEFINE_TYPE_EXTENDED( ofaCheckIntegrityBin, ofa_check_integrity_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaCheckIntegrityBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IPROGRESS, iprogress_iface_init ))

static void
check_integrity_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_finalize";
	ofaCheckIntegrityBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY_BIN( instance ));

	/* free data members here */
	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( instance ));

	g_list_free_full( priv->workers, ( GDestroyNotify ) g_free );
	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_bin_parent_class )->finalize( instance );
}

static void
check_integrity_bin_dispose( GObject *instance )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CHECK_INTEGRITY_BIN( instance ));

	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		write_settings( OFA_CHECK_INTEGRITY_BIN( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_check_integrity_bin_parent_class )->dispose( instance );
}

static void
ofa_check_integrity_bin_init( ofaCheckIntegrityBin *self )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_init";
	ofaCheckIntegrityBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_CHECK_INTEGRITY_BIN( self ));

	priv = ofa_check_integrity_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->display = TRUE;
	priv->others_errs = 0;
}

static void
ofa_check_integrity_bin_class_init( ofaCheckIntegrityBinClass *klass )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_integrity_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_integrity_bin_finalize;

	/**
	 * ofaCheckIntegrityBin::done:
	 *
	 * This signal is sent when the controls are finished.
	 *
	 * Arguments is the total count of errors.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaCheckIntegrityBin *self,
	 * 						gulong              total_errs,
	 * 						gpointer            user_data );
	 */
	st_signals[ DONE ] = g_signal_new_class_handler(
				"ofa-done",
				OFA_TYPE_CHECK_INTEGRITY_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_ULONG );
}

/**
 * ofa_check_integrity_bin_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the name where to save the settings.
 *
 * Returns: a new #ofaCheckIntegrityBin instance.
 */
ofaCheckIntegrityBin *
ofa_check_integrity_bin_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaCheckIntegrityBin *bin;
	ofaCheckIntegrityBinPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	bin = g_object_new( OFA_TYPE_CHECK_INTEGRITY_BIN, NULL );

	priv = ofa_check_integrity_bin_get_instance_private( bin );

	priv->getter = getter;
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_bin( bin );
	read_settings( bin );

	return( bin );
}

static void
setup_bin( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "cib-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	priv->paned = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "paned" );
	g_return_if_fail( priv->paned && GTK_IS_PANED( priv->paned ));

	priv->upper_viewport = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "upper-viewport" );
	g_return_if_fail( priv->upper_viewport && GTK_IS_VIEWPORT( priv->upper_viewport ));

	priv->objects_grid = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "objects-grid" );
	g_return_if_fail( priv->objects_grid && GTK_IS_GRID( priv->objects_grid ));
	priv->objects_row = 0;
	g_signal_connect( priv->objects_grid, "size-allocate", G_CALLBACK( on_grid_size_allocate ), self );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/*
 * settings are a string list with:
 * - paned pos
 */
static void
read_settings( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	myISettings *settings;
	gchar *key;
	GList *strlist, *it;
	const gchar *cstr;
	gint pos;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = get_settings_key( self );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	pos = my_strlen( cstr ) ? atoi( cstr ) : 100;
	gtk_paned_set_position( GTK_PANED( priv->paned ), pos );

	my_isettings_free_string_list( settings, strlist );
	g_free( key );
}

static void
write_settings( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	myISettings *settings;
	gchar *key, *str;
	gint pos;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	pos = gtk_paned_get_position( GTK_PANED( priv->paned ));

	str = g_strdup_printf( "%d;", pos );

	settings = ofa_igetter_get_user_settings( priv->getter );
	key = get_settings_key( self );
	my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( str );
	g_free( key );
}

static gchar *
get_settings_key( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	gchar *key;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	key = g_strdup_printf( "%s-%s", priv->settings_prefix, st_settings_sufix );

	return( key );
}

/**
 * ofa_check_integrity_bin_set_display:
 */
void
ofa_check_integrity_bin_set_display( ofaCheckIntegrityBin *bin, gboolean display )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_INTEGRITY_BIN( bin ));

	priv = ofa_check_integrity_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->display = display;
}

/**
 * ofa_check_integrity_bin_check:
 * @bin: this #ofaCheckIntegrityBin instance.
 *
 * Runs all checks.
 */
void
ofa_check_integrity_bin_check( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_CHECK_INTEGRITY_BIN( bin ));

	priv = ofa_check_integrity_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_idle_add(( GSourceFunc ) do_run, bin );
}

static gboolean
do_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	gint i;
	ofaExtenderCollection *extenders;
	GList *plugins, *it;
	ofaIDBModel *instance;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	for( i=0 ; st_fn[i] ; ++i ){
		( *st_fn[i] )( self );
	}

	extenders = ofa_igetter_get_extender_collection( priv->getter );
	plugins = ofa_extender_collection_get_for_type( extenders, OFA_TYPE_IDBMODEL );
	for( it=plugins ; it ; it=it->next ){
		instance = OFA_IDBMODEL( it->data );
		if( OFA_IDBMODEL_GET_INTERFACE( instance )->check_dbms_integrity ){
			priv->others_errs += OFA_IDBMODEL_GET_INTERFACE( instance )->check_dbms_integrity( instance, priv->getter, priv->display ? MY_IPROGRESS( self ) : NULL );
		}
	}
	g_list_free( plugins );

	set_checks_result( self );

	g_signal_emit_by_name( self, "ofa-done", priv->total_errs );

	/* do not continue and remove from idle callbacks list */
	return( G_SOURCE_REMOVE );
}

/*
 * Check that all references from dossier exist.
 */
static void
check_dossier_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	ofoDossier *dossier;
	gulong count, i;
	const gchar *cur_code, *for_ope, *sld_ope, *acc_number;
	GSList *currencies, *it;
	ofoCurrency *cur_obj;
	ofoOpeTemplate *ope_obj;
	ofoAccount *acc_obj;
	gchar *str;
	ofaHub *hub;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_DOSSIER );

	if( priv->display ){
		label = gtk_label_new( _( " Check for dossier integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	/* progress */
	hub = ofa_igetter_get_hub( priv->getter );
	dossier = ofa_hub_get_dossier( hub );
	priv->dossier_errs = 0;
	currencies = ofo_dossier_get_currencies( dossier );
	count = 3+g_slist_length( currencies );
	i = 0;

	/* check for default currency */
	cur_code = ofo_dossier_get_default_currency( dossier );
	if( !my_strlen( cur_code )){
		my_iprogress_set_text( MY_IPROGRESS( self ), worker, _( "Dossier has no default currency" ));
		priv->dossier_errs += 1;
	} else {
		cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
		if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
			str = g_strdup_printf( _( "Dossier default currency '%s' doesn't exist" ), cur_code );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->dossier_errs += 1;
		}
	}

	my_iprogress_pulse( MY_IPROGRESS( self ), worker, ++i, count );

	/* check for forward and solde operation templates */
	for_ope = ofo_dossier_get_forward_ope( dossier );
	if( !my_strlen( for_ope )){
		my_iprogress_set_text( MY_IPROGRESS( self ), worker, _( "Dossier has no forward operation template" ));
		priv->dossier_errs += 1;
	} else {
		ope_obj = ofo_ope_template_get_by_mnemo( priv->getter, for_ope );
		if( !ope_obj || !OFO_IS_OPE_TEMPLATE( ope_obj )){
			str = g_strdup_printf( _( "Dossier forward operation template '%s' doesn't exist" ), for_ope );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->dossier_errs += 1;
		}
	}

	my_iprogress_pulse( MY_IPROGRESS( self ), worker, ++i, count );

	sld_ope = ofo_dossier_get_sld_ope( dossier );
	if( !my_strlen( sld_ope )){
		my_iprogress_set_text( MY_IPROGRESS( self ), worker, _( "Dossier has no solde operation template" ));
		priv->dossier_errs += 1;
	} else {
		ope_obj = ofo_ope_template_get_by_mnemo( priv->getter, sld_ope );
		if( !ope_obj || !OFO_IS_OPE_TEMPLATE( ope_obj )){
			str = g_strdup_printf( _( "Dossier solde operation template '%s' doesn't exist" ), sld_ope );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->dossier_errs += 1;
		}
	}

	my_iprogress_pulse( MY_IPROGRESS( self ), worker, ++i, count );

	/* check solde accounts per currency */

	for( it=currencies ; it ; it=it->next ){
		cur_code = ( const gchar * ) it->data;
		if( !my_strlen( cur_code )){
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, _( "Dossier solde account has no currency" ));
			priv->dossier_errs += 1;
		} else {
			cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
			if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
				str = g_strdup_printf( _( "Dossier solde account currency '%s' doesn't exist" ), cur_code );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->dossier_errs += 1;
			}
			acc_number = ofo_dossier_get_sld_account( dossier, cur_code );
			if( !my_strlen( acc_number )){
				str = g_strdup_printf( _( "Dossier solde account for currency '%s' is empty" ), cur_code );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->dossier_errs += 1;
			} else {
				acc_obj = ofo_account_get_by_number( priv->getter, acc_number );
				if( !acc_obj || !OFO_IS_ACCOUNT( acc_obj )){
					str = g_strdup_printf( _( "Dossier solde account '%s' for currency '%s' doesn't exist" ),
							acc_number, cur_code );
					my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
					g_free( str );
					priv->dossier_errs += 1;
				}
			}
		}
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, ++i, count );
	}

	ofo_dossier_free_currencies( currencies );

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->dossier_errs );
}

/*
 * Check that BAT and BAT lines are OK
 */
static void
check_bat_lines_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	gulong count, i;
	GList *bats, *it, *lines, *itl, *orphans;
	ofoBat *bat;
	ofoBatLine *line;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	gchar *str;
	ofxCounter id, idline;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_BAT );

	if( priv->display ){
		label = gtk_label_new( _( " Check for BAT files and lines integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	priv->bat_lines_errs = 0;
	bats = ofo_bat_get_dataset( priv->getter );
	count = g_list_length( bats );

	if( count == 0 ){
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, 0, 0 );

	} else {
		for( i=1, it=bats ; it ; ++i, it=it->next ){
			bat = OFO_BAT( it->data );
			id = ofo_bat_get_id( bat );

			/* it is ok for a BAT file to not have a currency set */
			cur_code = ofo_bat_get_currency( bat );
			if( my_strlen( cur_code )){
				cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
				if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
					str = g_strdup_printf( _( "BAT file %lu currency '%s' doesn't exist" ), id, cur_code );
					my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
					g_free( str );
					priv->bat_lines_errs += 1;
				}
			}

			lines = ofo_bat_line_get_dataset( priv->getter, id );

			for( itl=lines ; itl ; itl=itl->next ){
				line = OFO_BAT_LINE( itl->data );
				idline = ofo_bat_line_get_line_id( line );

				/* it is ok for a BAT line to not have a currency */
				cur_code = ofo_bat_line_get_currency( line );
				if( my_strlen( cur_code )){
					cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
					if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
						str = g_strdup_printf(
								_( "BAT line %lu (from BAT file %lu) currency '%s' doesn't exist" ),
								idline, id, cur_code );
						my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
						g_free( str );
						priv->bat_lines_errs += 1;
					}
				}

				/* do not check the entry number of the line */
			}

			ofo_bat_line_free_dataset( lines );
			my_iprogress_pulse( MY_IPROGRESS( self ), worker, i, count );
		}

		/* check that all details have a parent */
		orphans = ofo_bat_line_get_orphans( priv->getter );
		for( it=orphans ; it ; it=it->next ){
			str = g_strdup_printf( _( "Found orphan BAT line: %s" ), ( const gchar * ) it->data );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->bat_lines_errs += 1;
			count += 1;
			my_iprogress_pulse( MY_IPROGRESS( self ), worker, count, count );
		}
		ofo_bat_line_free_orphans( orphans );
	}

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->bat_lines_errs );
}

/*
 * 3/ check that accounts are balanced per currency
 */
static void
check_accounts_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	GList *accounts, *it;
	gulong count, i;
	ofoAccount *account;
	gint cla_num;
	const gchar *acc_num, *cur_code;
	ofoClass *cla_obj;
	gchar *str;
	ofoCurrency *cur_obj;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_ACCOUNT );

	if( priv->display ){
		label = gtk_label_new( _( " Check for accounts integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	priv->accounts_errs = 0;
	accounts = ofo_account_get_dataset( priv->getter );
	count = g_list_length( accounts );

	if( count == 0 ){
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, 0, 0 );
	}

	for( i=1, it=accounts ; it && count ; ++i, it=it->next ){
		account = OFO_ACCOUNT( it->data );
		acc_num = ofo_account_get_number( account );

		cla_num = ofo_account_get_class( account );
		cla_obj = ofo_class_get_by_number( priv->getter, cla_num );
		if( !cla_obj || !OFO_IS_CLASS( cla_obj )){
			str = g_strdup_printf(
					_( "Class %d doesn't exist for account %s" ), cla_num, acc_num );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->accounts_errs += 1;
		}

		if( !ofo_account_is_root( account )){
			cur_code = ofo_account_get_currency( account );
			if( !my_strlen( cur_code )){
				str = g_strdup_printf( _( "Account %s doesn't have a currency" ), acc_num );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->accounts_errs += 1;
			} else {
				cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
				if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
					str = g_strdup_printf(
							_( "Account %s currency '%s' doesn't exist" ), acc_num, cur_code );
					my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
					g_free( str );
					priv->accounts_errs += 1;
				}
			}
		}

		my_iprogress_pulse( MY_IPROGRESS( self ), worker, i, count );
	}

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->accounts_errs );
}

/*
 * Check that ofoConcil
 */
static void
check_concil_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	gulong count;
	GList *it, *orphans;
	gchar *str;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_BAT );

	if( priv->display ){
		label = gtk_label_new( _( " Check for conciliation groups integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	count = 0;
	priv->concil_errs = 0;

	/* check that all details have a parent */
	orphans = ofo_concil_get_orphans( priv->getter );
	for( it=orphans ; it ; it=it->next ){
		str = g_strdup_printf( _( "Found orphan conciliation member: %s" ), ( const gchar * ) it->data );
		my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
		g_free( str );
		priv->concil_errs += 1;
		count += 1;
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, count, count );
	}
	ofo_concil_free_orphans( orphans );

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->concil_errs );
}

/*
 * check for entries integrity
 */
static void
check_entries_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	GList *entries, *it;
	gulong count, i;
	ofoEntry *entry;
	gchar *str;
	ofxCounter number;
	const gchar *acc_number, *cur_code, *led_mnemo, *ope_mnemo;
	ofoAccount *acc_obj;
	ofoCurrency *cur_obj;
	ofoLedger *led_obj;
	ofoOpeTemplate *ope_obj;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_ENTRY );

	if( priv->display ){
		label = gtk_label_new( _( " Check for entries integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	priv->entries_errs = 0;
	entries = ofo_entry_get_dataset_for_store( priv->getter, NULL, NULL );
	count = g_list_length( entries );

	if( count == 0 ){
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, 0, 0 );
	}

	for( i=1, it=entries ; it && count ; ++i, it=it->next ){
		entry = OFO_ENTRY( it->data );
		number = ofo_entry_get_number( entry );

		acc_number = ofo_entry_get_account( entry );
		if( !my_strlen( acc_number )){
			str = g_strdup_printf(
					_( "Entry %lu doesn't have account" ), number );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->entries_errs += 1;
		} else {
			acc_obj = ofo_account_get_by_number( priv->getter, acc_number );
			if( !acc_obj || !OFO_IS_ACCOUNT( acc_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has account %s which doesn't exist" ), number, acc_number );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		cur_code = ofo_entry_get_currency( entry );
		if( !my_strlen( cur_code )){
			str = g_strdup_printf( _( "Entry %lu doesn't have a currency" ), number );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->entries_errs += 1;
		} else {
			cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
			if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has currency '%s' which doesn't exist" ), number, cur_code );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		led_mnemo = ofo_entry_get_ledger( entry );
		if( !my_strlen( led_mnemo )){
			str = g_strdup_printf( _( "Entry %lu doesn't have a ledger" ), number );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->entries_errs += 1;
		} else {
			led_obj = ofo_ledger_get_by_mnemo( priv->getter, led_mnemo );
			if( !led_obj || !OFO_IS_LEDGER( led_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has ledger '%s' which doesn't exist" ), number, led_mnemo );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		/* ope template is not mandatory */
		ope_mnemo = ofo_entry_get_ope_template( entry );
		if( my_strlen( ope_mnemo )){
			ope_obj = ofo_ope_template_get_by_mnemo( priv->getter, ope_mnemo );
			if( !ope_obj || !OFO_IS_OPE_TEMPLATE( ope_obj )){
				str = g_strdup_printf(
						_( "Entry %lu has operation template '%s' which doesn't exist" ), number, ope_mnemo );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->entries_errs += 1;
			}
		}

		my_iprogress_pulse( MY_IPROGRESS( self ), worker, i, count );
	}

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->entries_errs );
}

/*
 * check for ledgers integrity
 */
static void
check_ledgers_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	GList *ledgers, *it;
	GList *currencies, *itc;
	gulong count, i;
	ofoLedger *ledger;
	gchar *str;
	const gchar *mnemo, *cur_code;
	ofoCurrency *cur_obj;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_LEDGER );

	if( priv->display ){
		label = gtk_label_new( _( " Check for ledgers integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	priv->ledgers_errs = 0;
	ledgers = ofo_ledger_get_dataset( priv->getter );
	count = g_list_length( ledgers );

	if( count == 0 ){
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, 0, 0 );
	}

	for( i=1, it=ledgers ; it && count ; ++i, it=it->next ){
		ledger = OFO_LEDGER( it->data );
		mnemo = ofo_ledger_get_mnemo( ledger );
		currencies = ofo_ledger_get_currencies( ledger );
		for( itc=currencies ; itc ; itc=itc->next ){
			cur_code = ( const gchar * ) itc->data;
			if( !my_strlen( cur_code )){
				str = g_strdup_printf( _( "Ledger %s has an empty currency" ), mnemo );
				my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
				g_free( str );
				priv->ledgers_errs += 1;
			} else {
				cur_obj = ofo_currency_get_by_code( priv->getter, cur_code );
				if( !cur_obj || !OFO_IS_CURRENCY( cur_obj )){
					str = g_strdup_printf(
							_( "Ledger %s has currency '%s' which doesn't exist" ), mnemo, cur_code );
					my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
					g_free( str );
					priv->ledgers_errs += 1;
				}
			}
		}
		g_list_free( currencies );
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, i, count );
	}

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->ledgers_errs );
}

/*
 * check for ope_templates integrity
 */
static void
check_ope_templates_run( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	const void *worker;
	GtkWidget *label;
	GList *ope_templates, *it, *orphans;
	gulong count, i;
	ofoOpeTemplate *ope_template;
	gchar *str;
	const gchar *mnemo, *led_mnemo, *acc_number;
	ofoLedger *led_obj;
	gint nbdets, idet;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	worker = GUINT_TO_POINTER( OFO_TYPE_OPE_TEMPLATE );

	if( priv->display ){
		label = gtk_label_new( _( " Check for operation templates integrity " ));
		my_iprogress_start_work( MY_IPROGRESS( self ), worker, label );
		my_iprogress_start_progress( MY_IPROGRESS( self ), worker, NULL, TRUE );
	}

	priv->ope_templates_errs = 0;
	ope_templates = ofo_ope_template_get_dataset( priv->getter );
	count = g_list_length( ope_templates );

	if( count == 0 ){
		my_iprogress_pulse( MY_IPROGRESS( self ), worker, 0, 0 );

	} else {
		for( i=1, it=ope_templates ; it && count ; ++i, it=it->next ){
			ope_template = OFO_OPE_TEMPLATE( it->data );
			mnemo = ofo_ope_template_get_mnemo( ope_template );

			/* ledger is optional here */
			led_mnemo = ofo_ope_template_get_ledger( ope_template );
			if( my_strlen( led_mnemo )){
				led_obj = ofo_ledger_get_by_mnemo( priv->getter, led_mnemo );
				if( !led_obj || !OFO_IS_LEDGER( led_obj )){
					str = g_strdup_printf(
							_( "Operation template %s has ledger '%s' which doesn't exist" ), mnemo, led_mnemo );
					my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
					g_free( str );
					priv->ope_templates_errs += 1;
				}
			}

			nbdets = ofo_ope_template_get_detail_count( ope_template );
			for( idet=0 ; idet<nbdets ; ++idet ){
				/* cannot check for account without first identifying formulas */
				acc_number = ofo_ope_template_get_detail_account( ope_template, idet );
				if( !my_strlen( acc_number )){

				}
			}

			my_iprogress_pulse( MY_IPROGRESS( self ), worker, i, count );
		}

		/* check that all details have a parent */
		orphans = ofo_ope_template_get_orphans( priv->getter );
		for( it=orphans ; it ; it=it->next ){
			str = g_strdup_printf( _( "Found orphan operation template: %s" ), ( const gchar * ) it->data );
			my_iprogress_set_text( MY_IPROGRESS( self ), worker, str );
			g_free( str );
			priv->ope_templates_errs += 1;
			count += 1;
			my_iprogress_pulse( MY_IPROGRESS( self ), worker, count, count );
		}
		ofo_ope_template_free_orphans( orphans );
	}

	/* progress end */
	my_iprogress_set_ok( MY_IPROGRESS( self ), worker, NULL, priv->ope_templates_errs );
}

/*
 * after the end of individual checks (entries, ledgers, accounts)
 * check that the balances are the sames
 */
static void
set_checks_result( ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkWidget *label;
	gchar *str;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	priv->total_errs =
			priv->dossier_errs
			+ priv->bat_lines_errs
			+ priv->accounts_errs
			+ priv->entries_errs
			+ priv->ledgers_errs
			+ priv->ope_templates_errs
			+ priv->others_errs;

	if( priv->display ){
		if( priv->total_errs > 0 ){
			str = g_strdup_printf(
					_( "We have detected %lu integrity errors in the DBMS." ), priv->total_errs );
			my_utils_msg_dialog( NULL, GTK_MESSAGE_WARNING, str );
			g_free( str );

		} else {
			label = my_utils_container_get_child_by_name(
							GTK_CONTAINER( self ), "p4-label-end" );
			g_return_if_fail( label && GTK_IS_LABEL( label ));

			if( priv->total_errs == 0 ){
				gtk_label_set_text( GTK_LABEL( label ),
						_( "Your DBMS is right. Good !" ));
				my_style_add( label, "labelinfo" );

			} else {
				gtk_label_set_text( GTK_LABEL( label ),
						_( "Detected integrity errors have to be fixed." ));
				my_style_add( label, "labelerror" );
			}
		}
	}
}

/*
 * thanks to http://stackoverflow.com/questions/2683531/stuck-on-scrolling-gtkviewport-to-end
 */
static void
on_grid_size_allocate( GtkWidget *grid, GdkRectangle *allocation, ofaCheckIntegrityBin *self )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkAdjustment* adjustment;

	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( self ));

	adjustment = gtk_scrollable_get_vadjustment( GTK_SCROLLABLE( priv->upper_viewport ));
	gtk_adjustment_set_value( adjustment, gtk_adjustment_get_upper( adjustment ));
}

/**
 * ofa_check_integrity_bin_get_status:
 */
gboolean
ofa_check_integrity_bin_get_status( ofaCheckIntegrityBin *bin )
{
	ofaCheckIntegrityBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_CHECK_INTEGRITY_BIN( bin ), FALSE );

	priv = ofa_check_integrity_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->total_errs == 0 );
}

/*
 * myIProgress interface management
 */
static void
iprogress_iface_init( myIProgressInterface *iface )
{
	static const gchar *thisfn = "ofa_check_integrity_bin_iprogress_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->start_work = iprogress_start_work;
	iface->start_progress = iprogress_start_progress;
	iface->pulse = iprogress_pulse;
	iface->set_ok = iprogress_set_ok;
	iface->set_text = iprogress_set_text;
}

/*
 * expects a GtkLabel
 */
static void
iprogress_start_work( myIProgress *instance, const void *worker, GtkWidget *widget )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkWidget *frame;
	sWorker *sdata;

	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( instance ));

	if( priv->display ){
		sdata = get_worker_data( OFA_CHECK_INTEGRITY_BIN( instance ), worker );

		frame = gtk_frame_new( NULL );
		gtk_widget_set_hexpand( frame, TRUE );
		my_utils_widget_set_margin_right( frame, SCROLLBAR_WIDTH );
		gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

		if( widget ){
			gtk_frame_set_label_widget( GTK_FRAME( frame ), widget );
		}

		sdata->grid = gtk_grid_new();
		my_utils_widget_set_margins( sdata->grid, 2, 2, 8, 4 );
		gtk_container_add( GTK_CONTAINER( frame ), sdata->grid );
		gtk_grid_set_column_spacing( GTK_GRID( sdata->grid ), 4 );

		gtk_grid_attach( GTK_GRID( priv->objects_grid ), frame, 0, priv->objects_row++, 1, 1 );

		gtk_widget_show_all( priv->objects_grid );
	}
}

static void
iprogress_start_progress( myIProgress *instance, const void *worker, GtkWidget *widget, gboolean with_bar )
{
	ofaCheckIntegrityBinPrivate *priv;
	sWorker *sdata;

	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( instance ));

	if( priv->display ){
		sdata = get_worker_data( OFA_CHECK_INTEGRITY_BIN( instance ), worker );

		if( widget ){
			gtk_grid_attach( GTK_GRID( sdata->grid ), widget, 0, 0, 1, 1 );
		}

		if( with_bar ){
			sdata->bar = my_progress_bar_new();
			gtk_grid_attach( GTK_GRID( sdata->grid ), GTK_WIDGET( sdata->bar ), 1, 0, 1, 1 );
		}

		gtk_widget_show_all( sdata->grid );
	}
}

static void
iprogress_pulse( myIProgress *instance, const void *worker, gulong count, gulong total )
{
	sWorker *sdata;
	gdouble progress;
	gchar *str;

	sdata = get_worker_data( OFA_CHECK_INTEGRITY_BIN( instance ), worker );

	if( sdata->bar ){
		progress = total ? ( gdouble ) count / ( gdouble ) total : 0;
		g_signal_emit_by_name( sdata->bar, "my-double", progress );

		str = g_strdup_printf( "%lu/%lu", count, total );
		g_signal_emit_by_name( sdata->bar, "my-text", str );
		g_free( str );
	}
}

static void
iprogress_set_ok( myIProgress *instance, const void *worker, GtkWidget *widget, gulong errs_count )
{
	ofaCheckIntegrityBinPrivate *priv;
	sWorker *sdata;
	GtkWidget *label;
	gchar *str;

	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( instance ));

	if( priv->display ){
		sdata = get_worker_data( OFA_CHECK_INTEGRITY_BIN( instance ), worker );

		label = gtk_label_new( "" );

		if( errs_count == 0 ){
			gtk_label_set_text( GTK_LABEL( label ), _( "OK" ));

		} else {
			str = g_strdup_printf( _( "%lu error(s)" ), errs_count );
			gtk_label_set_text( GTK_LABEL( label ), str );
			g_free( str );
		}

		gtk_widget_set_valign( label, GTK_ALIGN_END );
		my_style_add( label, errs_count == 0 ? "labelinfo" : "labelerror" );

		gtk_grid_attach( GTK_GRID( sdata->grid ), label, 2, 0, 1, 1 );

		gtk_widget_show_all( sdata->grid );
	}
}

static void
iprogress_set_text( myIProgress *instance, const void *worker, const gchar *text )
{
	ofaCheckIntegrityBinPrivate *priv;
	GtkTextIter iter;
	gchar *str;
	GtkAdjustment* adjustment;

	priv = ofa_check_integrity_bin_get_instance_private( OFA_CHECK_INTEGRITY_BIN( instance ));

	if( priv->display ){
		if( !priv->text_buffer ){
			priv->text_view = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "textview" );
			g_return_if_fail( priv->text_view && GTK_IS_TEXT_VIEW( priv->text_view ));
			priv->text_buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( priv->text_view ));
		}
		g_return_if_fail( priv->text_buffer && GTK_IS_TEXT_BUFFER( priv->text_buffer ));

		gtk_text_buffer_get_end_iter( priv->text_buffer, &iter );

		str = g_strdup_printf( "%s\n", text );
		gtk_text_buffer_insert( priv->text_buffer, &iter, str, -1 );
		g_free( str );

		adjustment = gtk_scrollable_get_vadjustment( GTK_SCROLLABLE( priv->text_view ));
		gtk_adjustment_set_value( adjustment, gtk_adjustment_get_upper( adjustment ));
	}
}

static sWorker *
get_worker_data( ofaCheckIntegrityBin *self, const void *worker )
{
	ofaCheckIntegrityBinPrivate *priv;
	GList *it;
	sWorker *sdata;

	priv = ofa_check_integrity_bin_get_instance_private( self );

	for( it=priv->workers ; it ; it=it->next ){
		sdata = ( sWorker * ) it->data;
		if( sdata->worker == worker ){
			return( sdata );
		}
	}

	sdata = g_new0( sWorker, 1 );
	sdata->worker = worker;
	priv->workers = g_list_prepend( priv->workers, sdata );

	return( sdata );
}
