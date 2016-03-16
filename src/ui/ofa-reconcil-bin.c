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
#include "api/my-editable-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ientry-account.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-reconcil-bin.h"

/* private instance data
 */
struct _ofaReconcilBinPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */
	const ofaMainWindow *main_window;
	ofaHub              *hub;

	/* UI
	 */
	GtkWidget           *account_entry;
	GtkWidget           *account_label;
	GtkWidget           *date_entry;

	/* internals
	 */
	ofoAccount          *account;
	GDate                date;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-reconcil-bin.ui";
static const gchar *st_settings         = "RenderReconciliation";

static void iaccount_entry_iface_init( ofaIEntryAccountInterface *iface );
static void setup_bin( ofaReconcilBin *self );
static void setup_account_selection( ofaReconcilBin *self );
static void setup_date_selection( ofaReconcilBin *self );
static void setup_others( ofaReconcilBin *self );
static void on_account_changed( GtkEntry *entry, ofaReconcilBin *self );
static void on_date_changed( GtkEntry *entry, ofaReconcilBin *self );
static void load_settings( ofaReconcilBin *self );
static void set_settings( ofaReconcilBin *self );

G_DEFINE_TYPE_EXTENDED( ofaReconcilBin, ofa_reconcil_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaReconcilBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IENTRY_ACCOUNT, iaccount_entry_iface_init ))

static void
reconcil_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECONCIL_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_bin_parent_class )->finalize( instance );
}

static void
reconcil_bin_dispose( GObject *instance )
{
	ofaReconcilBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECONCIL_BIN( instance ));

	priv = ofa_reconcil_bin_get_instance_private( OFA_RECONCIL_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_bin_parent_class )->dispose( instance );
}

static void
ofa_reconcil_bin_init( ofaReconcilBin *self )
{
	static const gchar *thisfn = "ofa_reconcil_bin_init";
	ofaReconcilBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECONCIL_BIN( self ));

	priv = ofa_reconcil_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_reconcil_bin_class_init( ofaReconcilBinClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_bin_finalize;

	/**
	 * ofaReconcilBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaReconcilBin *self,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_RECONCIL_BIN,
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
 * ofa_reconcil_bin_new:
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Returns: a newly allocated #ofaReconcilBin object.
 */
ofaReconcilBin *
ofa_reconcil_bin_new( const ofaMainWindow *main_window )
{
	ofaReconcilBin *self;
	ofaReconcilBinPrivate *priv;

	self = g_object_new( OFA_TYPE_RECONCIL_BIN, NULL );

	priv = ofa_reconcil_bin_get_instance_private( self );

	priv->main_window = main_window;

	priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	setup_bin( self );
	setup_account_selection( self );
	setup_date_selection( self );
	setup_others( self );

	load_settings( self );

	return( self );
}

/*
 * ofaIEntryAccount interface management
 */
static void
iaccount_entry_iface_init( ofaIEntryAccountInterface *iface )
{
	static const gchar *thisfn = "ofa_account_filter_vv_bin_iaccount_entry_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

static void
setup_bin( ofaReconcilBin *self )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "rb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );
}

static void
setup_account_selection( ofaReconcilBin *self )
{
	ofaReconcilBinPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_reconcil_bin_get_instance_private( self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );
	priv->account_entry = entry;
	ofa_ientry_account_init(
			OFA_IENTRY_ACCOUNT( self ), OFA_MAIN_WINDOW( priv->main_window ),
			GTK_ENTRY( entry ), ACCOUNT_ALLOW_RECONCILIABLE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->account_label = label;
}

static void
setup_date_selection( ofaReconcilBin *self )
{
	ofaReconcilBinPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_reconcil_bin_get_instance_private( self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->date_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), TRUE );

	g_signal_connect( entry, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaReconcilBin *self )
{
}

static void
on_account_changed( GtkEntry *entry, ofaReconcilBin *self )
{
	ofaReconcilBinPrivate *priv;
	const gchar *cstr;

	priv = ofa_reconcil_bin_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	priv->account = NULL;

	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		priv->account = ofo_account_get_by_number( priv->hub, cstr );
		if( priv->account ){
			gtk_label_set_text(
					GTK_LABEL( priv->account_label ), ofo_account_get_label( priv->account ));
		}
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_changed( GtkEntry *entry, ofaReconcilBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_reconcil_bin_is_valid:
 * @bin:
 * @msgerr: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_reconcil_bin_is_valid( ofaReconcilBin *bin, gchar **msgerr )
{
	ofaReconcilBinPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_RECONCIL_BIN( bin ), FALSE );

	priv = ofa_reconcil_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	valid = TRUE;

	if( valid ){
		if( !priv->account ){
			valid = FALSE;
			if( msgerr ){
				*msgerr = g_strdup( _( "Invalid account" ));
			}
		}
	}
	if( valid ){
		my_date_set_from_date(
				&priv->date,
				my_editable_date_get_date( GTK_EDITABLE( priv->date_entry ), &valid ));
		if( !valid  && msgerr ){
			*msgerr = g_strdup( _( "Invalid reconciliation date" ));
		}
	}
	if( valid ){
		set_settings( bin );
	}

	return( valid );
}

/**
 * ofa_reconcil_bin_get_account:
 * @bin:
 *
 * Returns: the current account number, or %NULL.
 */
const gchar *
ofa_reconcil_bin_get_account( const ofaReconcilBin *bin )
{
	ofaReconcilBinPrivate *priv;
	const gchar *label;

	g_return_val_if_fail( bin && OFA_IS_RECONCIL_BIN( bin ), NULL );

	priv = ofa_reconcil_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	label = NULL;

	if( priv->account ){
		label = ofo_account_get_number( priv->account );
	}

	return( label );
}

/**
 * ofa_reconcil_bin_set_account:
 * @bin:
 * @number:
 */
void
ofa_reconcil_bin_set_account( ofaReconcilBin *bin, const gchar *number )
{
	ofaReconcilBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECONCIL_BIN( bin ));

	priv = ofa_reconcil_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( priv->account ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), number );
	}
}

/**
 * ofa_reconcil_bin_get_date:
 */
const GDate *
ofa_reconcil_bin_get_date( const ofaReconcilBin *bin )
{
	ofaReconcilBinPrivate *priv;
	GDate *date;

	g_return_val_if_fail( bin && OFA_IS_RECONCIL_BIN( bin ), NULL );

	priv = ofa_reconcil_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date = &priv->date;

	return(( const GDate * ) date );
}

/*
 * setttings:
 * account;date_sql;
 */
static void
load_settings( ofaReconcilBin *self )
{
	ofaReconcilBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = ofa_reconcil_bin_get_instance_private( self );

	list = ofa_settings_user_get_string_list( st_settings );

	it = list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		my_editable_date_set_date( GTK_EDITABLE( priv->date_entry ), &date );
	}

	ofa_settings_free_string_list( list );
}

static void
set_settings( ofaReconcilBin *self )
{
	ofaReconcilBinPrivate *priv;
	const gchar *cstr;
	gchar *str, *sdate;

	priv = ofa_reconcil_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->account_entry ));

	sdate = my_date_to_str( &priv->date, MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;",
			cstr ? cstr : "",
			sdate ? sdate : "" );

	ofa_settings_user_set_string( st_settings, str );

	g_free( sdate );
	g_free( str );
}
