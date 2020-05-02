/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-date-editable.h"
#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-prefs.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-reconcil-args.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;
	gchar       *settings_prefix;

	/* runtime
	 */
	myISettings *settings;
	ofoAccount  *account;
	GDate        date;

	/* UI
	 */
	GtkWidget   *account_entry;
	GtkWidget   *account_label;
	GtkWidget   *date_entry;
}
	ofaReconcilArgsPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-reconcil-args.ui";

static void setup_runtime( ofaReconcilArgs *self );
static void setup_bin( ofaReconcilArgs *self );
static void setup_account_selection( ofaReconcilArgs *self );
static void setup_date_selection( ofaReconcilArgs *self );
static void setup_others( ofaReconcilArgs *self );
static void on_account_changed( GtkEntry *entry, ofaReconcilArgs *self );
static void on_date_changed( GtkEntry *entry, ofaReconcilArgs *self );
static void read_settings( ofaReconcilArgs *self );
static void write_settings( ofaReconcilArgs *self );

G_DEFINE_TYPE_EXTENDED( ofaReconcilArgs, ofa_reconcil_args, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaReconcilArgs ))

static void
reconcil_args_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_reconcil_args_finalize";
	ofaReconcilArgsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECONCIL_ARGS( instance ));

	/* free data members here */
	priv = ofa_reconcil_args_get_instance_private( OFA_RECONCIL_ARGS( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_args_parent_class )->finalize( instance );
}

static void
reconcil_args_dispose( GObject *instance )
{
	ofaReconcilArgsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RECONCIL_ARGS( instance ));

	priv = ofa_reconcil_args_get_instance_private( OFA_RECONCIL_ARGS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_reconcil_args_parent_class )->dispose( instance );
}

static void
ofa_reconcil_args_init( ofaReconcilArgs *self )
{
	static const gchar *thisfn = "ofa_reconcil_args_init";
	ofaReconcilArgsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECONCIL_ARGS( self ));

	priv = ofa_reconcil_args_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_reconcil_args_class_init( ofaReconcilArgsClass *klass )
{
	static const gchar *thisfn = "ofa_reconcil_args_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = reconcil_args_dispose;
	G_OBJECT_CLASS( klass )->finalize = reconcil_args_finalize;

	/**
	 * ofaReconcilArgs::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaReconcilArgs *self,
	 * 						gpointer      user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_RECONCIL_ARGS,
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
 * ofa_reconcil_args_new:
 * @getter: a #ofaIGetter instance.
 * @settings_prefix: the prefix of the key in user settings.
 *
 * Returns: a newly allocated #ofaReconcilArgs object.
 */
ofaReconcilArgs *
ofa_reconcil_args_new( ofaIGetter *getter, const gchar *settings_prefix )
{
	ofaReconcilArgs *self;
	ofaReconcilArgsPrivate *priv;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( settings_prefix ), NULL );

	self = g_object_new( OFA_TYPE_RECONCIL_ARGS, NULL );

	priv = ofa_reconcil_args_get_instance_private( self );

	priv->getter = getter;

	g_free( priv->settings_prefix );
	priv->settings_prefix = g_strdup( settings_prefix );

	setup_runtime( self );
	setup_bin( self );
	setup_account_selection( self );
	setup_date_selection( self );
	setup_others( self );

	read_settings( self );

	return( self );
}

static void
setup_runtime( ofaReconcilArgs *self )
{
	ofaReconcilArgsPrivate *priv;

	priv = ofa_reconcil_args_get_instance_private( self );

	priv->settings = ofa_igetter_get_user_settings( priv->getter );
}

static void
setup_bin( ofaReconcilArgs *self )
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
setup_account_selection( ofaReconcilArgs *self )
{
	ofaReconcilArgsPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_reconcil_args_get_instance_private( self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );
	priv->account_entry = entry;
	ofa_account_editable_init( GTK_EDITABLE( entry ), priv->getter, ACCOUNT_ALLOW_RECONCILIABLE );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->account_label = label;
}

static void
setup_date_selection( ofaReconcilArgs *self )
{
	ofaReconcilArgsPrivate *priv;
	GtkWidget *entry, *label;

	priv = ofa_reconcil_args_get_instance_private( self );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	priv->date_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_entry_format( GTK_EDITABLE( entry ), ofa_prefs_date_get_display_format( priv->getter ));
	my_date_editable_set_label_format( GTK_EDITABLE( entry ), label, ofa_prefs_date_get_check_format( priv->getter ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), TRUE );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_get_overwrite( priv->getter ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_date_changed ), self );
}

static void
setup_others( ofaReconcilArgs *self )
{
}

static void
on_account_changed( GtkEntry *entry, ofaReconcilArgs *self )
{
	ofaReconcilArgsPrivate *priv;
	const gchar *cstr;

	priv = ofa_reconcil_args_get_instance_private( self );

	gtk_label_set_text( GTK_LABEL( priv->account_label ), "" );
	priv->account = NULL;

	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		priv->account = ofo_account_get_by_number( priv->getter, cstr );
		if( priv->account ){
			gtk_label_set_text(
					GTK_LABEL( priv->account_label ), ofo_account_get_label( priv->account ));
		}
	}

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_changed( GtkEntry *entry, ofaReconcilArgs *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_reconcil_args_is_valid:
 * @bin:
 * @msgerr: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_reconcil_args_is_valid( ofaReconcilArgs *bin, gchar **msgerr )
{
	ofaReconcilArgsPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_RECONCIL_ARGS( bin ), FALSE );

	priv = ofa_reconcil_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	if( msgerr ){
		*msgerr = NULL;
	}

	valid = TRUE;

	if( valid ){
		if( !priv->account ){
			valid = FALSE;
			if( msgerr ){
				*msgerr = g_strdup( _( "Account number is unknown or invalid" ));
			}
		}
	}
	if( valid ){
		my_date_set_from_date(
				&priv->date,
				my_date_editable_get_date( GTK_EDITABLE( priv->date_entry ), &valid ));
		if( !valid  && msgerr ){
			*msgerr = g_strdup( _( "Reconciliation date is invalid" ));
		}
	}
	if( valid ){
		write_settings( bin );
	}

	return( valid );
}

/**
 * ofa_reconcil_args_get_account:
 * @bin:
 *
 * Returns: the current account number, or %NULL.
 */
const gchar *
ofa_reconcil_args_get_account( ofaReconcilArgs *bin )
{
	ofaReconcilArgsPrivate *priv;
	const gchar *label;

	g_return_val_if_fail( bin && OFA_IS_RECONCIL_ARGS( bin ), NULL );

	priv = ofa_reconcil_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	label = NULL;

	if( priv->account ){
		label = ofo_account_get_number( priv->account );
	}

	return( label );
}

/**
 * ofa_reconcil_args_set_account:
 * @bin:
 * @number:
 */
void
ofa_reconcil_args_set_account( ofaReconcilArgs *bin, const gchar *number )
{
	ofaReconcilArgsPrivate *priv;

	g_return_if_fail( bin && OFA_IS_RECONCIL_ARGS( bin ));

	priv = ofa_reconcil_args_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	if( priv->account ){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), number );
	}
}

/**
 * ofa_reconcil_args_get_date:
 */
const GDate *
ofa_reconcil_args_get_date( ofaReconcilArgs *bin )
{
	ofaReconcilArgsPrivate *priv;
	GDate *date;

	g_return_val_if_fail( bin && OFA_IS_RECONCIL_ARGS( bin ), NULL );

	priv = ofa_reconcil_args_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	date = &priv->date;

	return(( const GDate * ) date );
}

/*
 * setttings:
 * account;date_sql;
 */
static void
read_settings( ofaReconcilArgs *self )
{
	ofaReconcilArgsPrivate *priv;
	GList *strlist, *it;
	const gchar *cstr;
	GDate date;
	gchar *key;

	priv = ofa_reconcil_args_get_instance_private( self );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	strlist = my_isettings_get_string_list( priv->settings, HUB_USER_SETTINGS_GROUP, key );

	it = strlist;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->account_entry ), cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_str( &date, cstr, MY_DATE_SQL );
		my_date_editable_set_date( GTK_EDITABLE( priv->date_entry ), &date );
	}

	my_isettings_free_string_list( priv->settings, strlist );
	g_free( key );
}

static void
write_settings( ofaReconcilArgs *self )
{
	ofaReconcilArgsPrivate *priv;
	const gchar *cstr;
	gchar *str, *sdate, *key;

	priv = ofa_reconcil_args_get_instance_private( self );

	cstr = gtk_entry_get_text( GTK_ENTRY( priv->account_entry ));

	sdate = my_date_to_str( &priv->date, MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;",
			cstr ? cstr : "",
			sdate ? sdate : "" );

	key = g_strdup_printf( "%s-args", priv->settings_prefix );
	my_isettings_set_string( priv->settings, HUB_USER_SETTINGS_GROUP, key, str );

	g_free( key );
	g_free( sdate );
	g_free( str );
}
