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

#include "api/my-utils.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-account-selector-bin.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAccountSelectorBinPrivate {
	gboolean       dispose_has_run;

	/* initialization
	 */
	gchar         *pref_name;			/* settings key name */
	gint           allowed;
	gchar         *def_account;
	ofaMainWindow *main_window;

	/* UI
	 */
	GtkWidget     *acc_entry;
	GtkWidget     *acc_select;
	GtkWidget     *acc_label;

	/* data
	 */
	gchar         *acc_number;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_bin_xml          = PKGUIDIR "/ofa-account-selector-bin.ui";

G_DEFINE_TYPE( ofaAccountSelectorBin, ofa_account_selector_bin, GTK_TYPE_BIN )

static void setup_bin( ofaAccountSelectorBin *bin );
static void on_entry_changed( GtkEntry *entry, ofaAccountSelectorBin *bin );
static void on_select_clicked( GtkButton *button, ofaAccountSelectorBin *bin );
static void get_settings( ofaAccountSelectorBin *bin );
static void set_settings( ofaAccountSelectorBin *bin );

static void
account_selector_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_selector_bin_finalize";
	ofaAccountSelectorBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECTOR_BIN( instance ));

	/* free data members here */
	priv = OFA_ACCOUNT_SELECTOR_BIN( instance )->priv;

	g_free( priv->pref_name );
	g_free( priv->def_account );
	g_free( priv->acc_number );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_selector_bin_parent_class )->finalize( instance );
}

static void
account_selector_bin_dispose( GObject *instance )
{
	ofaAccountSelectorBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ACCOUNT_SELECTOR_BIN( instance ));

	priv = OFA_ACCOUNT_SELECTOR_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_selector_bin_parent_class )->dispose( instance );
}

static void
ofa_account_selector_bin_init( ofaAccountSelectorBin *self )
{
	static const gchar *thisfn = "ofa_account_selector_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACCOUNT_SELECTOR_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_ACCOUNT_SELECTOR_BIN, ofaAccountSelectorBinPrivate );
}

static void
ofa_account_selector_bin_class_init( ofaAccountSelectorBinClass *klass )
{
	static const gchar *thisfn = "ofa_account_selector_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_selector_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_selector_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaAccountSelectorBinPrivate ));

	/**
	 * ofaAccountSelectorBin::changed:
	 *
	 * This signal is sent when the account number is changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaAccountSelectorBin *bin,
	 * 						const gchar         *acc_number,
	 * 						gpointer             user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_ACCOUNT_SELECTOR_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_STRING );
}

/**
 * ofa_account_selector_bin_new:
 * @pref_name: the name of the preference key
 * @allowed: the type of accepted account (cf. #ofaAccountSelect)
 *
 * Returns: a newly allocated #ofaAccountSelectorBin object.
 */
ofaAccountSelectorBin *
ofa_account_selector_bin_new( const gchar *pref_name, gint allowed )
{
	ofaAccountSelectorBin *self;

	self = g_object_new( OFA_TYPE_ACCOUNT_SELECTOR_BIN, NULL );

	self->priv->pref_name = g_strdup( pref_name );
	self->priv->allowed = allowed;

	setup_bin( self );
	get_settings( self );

	return( self );
}

static void
setup_bin( ofaAccountSelectorBin *bin )
{
	ofaAccountSelectorBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *button, *label;

	priv = bin->priv;
	builder = gtk_builder_new_from_file( st_bin_xml );

	object = gtk_builder_get_object( builder, "asb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top" );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "account-number" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_entry_changed ), bin );
	priv->acc_entry = entry;

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "account-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_select_clicked ), bin );
	/* the button will stay insensitive until the selection parms are set */
	gtk_widget_set_sensitive( button, FALSE );
	priv->acc_select = button;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->acc_label = label;

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_account_selector_bin_set_select_args:
 * @bin:
 * @default_account: when opening the ofaAccountSelect dialog while the
 *  entry is empty
 * @main_window:
 */
void
ofa_account_selector_bin_set_select_args(
		ofaAccountSelectorBin *bin, const gchar *default_account, ofaMainWindow *main_window )
{
	ofaAccountSelectorBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_SELECTOR_BIN( bin ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		priv->def_account = g_strdup( default_account );
		priv->main_window = main_window;

		gtk_widget_set_sensitive( priv->acc_select, TRUE );
	}
}

static void
on_entry_changed( GtkEntry *entry, ofaAccountSelectorBin *bin )
{
	ofaAccountSelectorBinPrivate *priv;
	ofoAccount *account;
	const gchar *label;
	gchar *str;
	gboolean ok;

	priv = bin->priv;

	g_free( priv->acc_number );
	priv->acc_number = g_strdup( gtk_entry_get_text( GTK_ENTRY( priv->acc_entry )));

	account = ofo_account_get_by_number(
						ofa_main_window_get_dossier( priv->main_window ), priv->acc_number );

	if( account ){
		g_return_if_fail( OFO_IS_ACCOUNT( account ));
		label = ofo_account_get_label( account );

		ok = ofo_account_is_allowed( account, priv->allowed );

		str = ok ? g_strdup( label ) : g_strdup_printf( "<i>%s</i>", label );
		gtk_label_set_markup( GTK_LABEL( priv->acc_label ), str );
		g_free( str );

		my_utils_widget_set_style( priv->acc_label, ok ? "labelnormal" : "labelinvalid" );

	} else {
		gtk_label_set_text( GTK_LABEL( priv->acc_label ), "" );
	}

	g_signal_emit_by_name( bin, "ofa-changed", priv->acc_number );

	set_settings( bin );
}

static void
on_select_clicked( GtkButton *button, ofaAccountSelectorBin *bin )
{
	ofaAccountSelectorBinPrivate *priv;
	gchar *number;

	priv = bin->priv;

	g_free( priv->acc_number );
	priv->acc_number = g_strdup( gtk_entry_get_text( GTK_ENTRY( priv->acc_entry )));

	number = ofa_account_select_run(
					priv->main_window,
					my_strlen( priv->acc_number ) ? priv->acc_number : priv->def_account,
					priv->allowed );

	if( my_strlen( number )){
		gtk_entry_set_text( GTK_ENTRY( priv->acc_entry ), number );
	}

	g_free( number );
}

/**
 * ofa_account_selector_bin_get_account:
 * @bin:
 */
const gchar *
ofa_account_selector_bin_get_account( const ofaAccountSelectorBin *bin )
{
	ofaAccountSelectorBinPrivate *priv;
	const gchar *number;

	g_return_val_if_fail( bin && OFA_IS_ACCOUNT_SELECTOR_BIN( bin ), NULL );

	priv = bin->priv;
	number = NULL;

	if( !priv->dispose_has_run ){

		number = ( const gchar * ) priv->acc_number;
	}

	return( number );
}

/**
 * ofa_account_selector_bin_set_account:
 * @bin:
 * @account: the account number
 */
void
ofa_account_selector_bin_set_account( ofaAccountSelectorBin *bin, const gchar *account )
{
	ofaAccountSelectorBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_ACCOUNT_SELECTOR_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		gtk_entry_set_text( GTK_ENTRY( priv->acc_number ), account );
	}
}

/*
 * settings are: acc_number;
 */
static void
get_settings( ofaAccountSelectorBin *bin )
{
	ofaAccountSelectorBinPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = bin->priv;

	slist = ofa_settings_get_string_list( priv->pref_name );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			gtk_entry_set_text( GTK_ENTRY( priv->acc_entry ), cstr );
		}

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaAccountSelectorBin *bin )
{
	ofaAccountSelectorBinPrivate *priv;
	gchar *str;

	priv = bin->priv;

	str = g_strdup_printf( "%s;", priv->acc_number ? priv->acc_number : "" );

	ofa_settings_set_string( priv->pref_name, str );

	g_free( str );
}
