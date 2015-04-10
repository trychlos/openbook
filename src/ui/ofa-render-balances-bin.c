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
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-dates-filter-hv-bin.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-render-balances-bin.h"

/* private instance data
 */
struct _ofaRenderBalancesBinPrivate {
	gboolean             dispose_has_run;
	ofaMainWindow       *main_window;

	/* UI
	 */
	GtkWidget           *from_account_etiq;		/* account selection */
	GtkWidget           *from_account_entry;
	GtkWidget           *from_account_btn;
	GtkWidget           *from_account_label;
	GtkWidget           *to_account_etiq;
	GtkWidget           *to_account_entry;
	GtkWidget           *to_account_btn;
	GtkWidget           *to_account_label;
	GtkWidget           *all_accounts_btn;
	GtkWidget           *per_class_btn;			/* subtotal per class */
	GtkWidget           *new_page_btn;
	ofaDatesFilterHVBin *dates_filter;

	/* internals
	 */
	gchar               *from_account;
	gchar               *to_account;
	gboolean             all_accounts;
	gboolean             per_class;
	gboolean             new_page;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-render-balances-bin.ui";
static const gchar *st_ui_id            = "RenderBalancesBin";
static const gchar *st_settings         = "RenderBalances";

G_DEFINE_TYPE( ofaRenderBalancesBin, ofa_render_balances_bin, GTK_TYPE_BIN )

static GtkWidget *load_dialog( ofaRenderBalancesBin *bin );
static void       setup_account_selection( ofaRenderBalancesBin *self, GtkContainer *parent );
static void       setup_date_selection( ofaRenderBalancesBin *self, GtkContainer *parent );
static void       setup_others( ofaRenderBalancesBin *self, GtkContainer *parent );
static void       on_from_account_changed( GtkEntry *entry, ofaRenderBalancesBin *self );
static void       on_from_account_select( GtkButton *button, ofaRenderBalancesBin *self );
static void       on_to_account_changed( GtkEntry *entry, ofaRenderBalancesBin *self );
static void       on_to_account_select( GtkButton *button, ofaRenderBalancesBin *self );
static void       on_account_changed( ofaRenderBalancesBin *self, GtkEntry *entry, GtkWidget *label, gchar **dest );
static void       on_account_select( GtkButton *button, ofaRenderBalancesBin *self, GtkWidget *entry );
static void       on_all_accounts_toggled( GtkToggleButton *button, ofaRenderBalancesBin *self );
static void       on_per_class_toggled( GtkToggleButton *button, ofaRenderBalancesBin *self );
static void       on_new_page_toggled( GtkToggleButton *button, ofaRenderBalancesBin *self );
static void       on_date_filter_changed( ofaIDatesFilter *filter, gint who, gboolean empty, gboolean valid, ofaRenderBalancesBin *self );
static void       load_settings( ofaRenderBalancesBin *bin );
static void       set_settings( ofaRenderBalancesBin *bin );

static void
render_balances_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_render_balances_bin_finalize";
	ofaRenderBalancesBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RENDER_BALANCES_BIN( instance ));

	/* free data members here */
	priv = OFA_RENDER_BALANCES_BIN( instance )->priv;

	g_free( priv->from_account );
	g_free( priv->to_account );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_balances_bin_parent_class )->finalize( instance );
}

static void
render_balances_bin_dispose( GObject *instance )
{
	ofaRenderBalancesBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_RENDER_BALANCES_BIN( instance ));

	priv = OFA_RENDER_BALANCES_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_render_balances_bin_parent_class )->dispose( instance );
}

static void
ofa_render_balances_bin_init( ofaRenderBalancesBin *self )
{
	static const gchar *thisfn = "ofa_render_balances_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RENDER_BALANCES_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_RENDER_BALANCES_BIN, ofaRenderBalancesBinPrivate );
}

static void
ofa_render_balances_bin_class_init( ofaRenderBalancesBinClass *klass )
{
	static const gchar *thisfn = "ofa_render_balances_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = render_balances_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = render_balances_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaRenderBalancesBinPrivate ));

	/**
	 * ofaRenderBalancesBin::ofa-changed:
	 *
	 * This signal is sent when a widget has changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaRenderBalancesBin *bin,
	 * 						gpointer            user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-changed",
				OFA_TYPE_RENDER_BALANCES_BIN,
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
 * ofa_render_balances_bin_new:
 * @main_window:
 *
 * Returns: a newly allocated #ofaRenderBalancesBin object.
 */
ofaRenderBalancesBin *
ofa_render_balances_bin_new( ofaMainWindow *main_window )
{
	ofaRenderBalancesBin *self;
	GtkWidget *parent;

	self = g_object_new( OFA_TYPE_RENDER_BALANCES_BIN, NULL );

	self->priv->main_window = main_window;

	parent = load_dialog( self );
	g_return_val_if_fail( parent && GTK_IS_CONTAINER( parent ), NULL );

	setup_account_selection( self, GTK_CONTAINER( parent ));
	setup_date_selection( self, GTK_CONTAINER( parent ));
	setup_others( self, GTK_CONTAINER( parent ));

	load_settings( self );

	return( self );
}

/*
 * returns: the GtkContainer parent
 */
static GtkWidget *
load_dialog( ofaRenderBalancesBin *bin )
{
	GtkWidget *window;
	GtkWidget *top_widget;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_val_if_fail( window && GTK_IS_WINDOW( window ), NULL );

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top" );
	g_return_val_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ), NULL );

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));

	return( top_widget );
}

static void
setup_account_selection( ofaRenderBalancesBin *self, GtkContainer *parent )
{
	ofaRenderBalancesBinPrivate *priv;
	GtkWidget *label, *entry, *button, *toggle;

	priv = self->priv;

	label = my_utils_container_get_child_by_name( parent, "from-account-etiq" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->from_account_etiq = label;

	label = my_utils_container_get_child_by_name( parent, "from-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->from_account_label = label;

	entry = my_utils_container_get_child_by_name( parent, "from-account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_from_account_changed ), self );
	priv->from_account_entry = entry;

	button = my_utils_container_get_child_by_name( parent, "from-account-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_from_account_select ), self );
	priv->from_account_btn = button;

	label = my_utils_container_get_child_by_name( parent, "to-account-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->to_account_label = label;

	label = my_utils_container_get_child_by_name( parent, "to-account-etiq" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	priv->to_account_etiq = label;

	entry = my_utils_container_get_child_by_name( parent, "to-account-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_to_account_changed ), self );
	priv->to_account_entry = entry;

	button = my_utils_container_get_child_by_name( parent, "to-account-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_to_account_select ), self );
	priv->to_account_btn = button;

	toggle = my_utils_container_get_child_by_name( parent, "all-accounts" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_all_accounts_toggled ), self );
	priv->all_accounts_btn = toggle;
}

static void
setup_date_selection( ofaRenderBalancesBin *self, GtkContainer *parent )
{
	ofaRenderBalancesBinPrivate *priv;
	GtkWidget *alignment, *label;
	ofaDatesFilterHVBin *bin;

	priv = self->priv;

	alignment = my_utils_container_get_child_by_name( parent, "date-filter" );
	g_return_if_fail( alignment && GTK_IS_CONTAINER( alignment ));

	bin = ofa_dates_filter_hv_bin_new();
	gtk_container_add( GTK_CONTAINER( alignment ), GTK_WIDGET( bin ));

	/* instead of "effect dates filter" */
	label = ofa_idates_filter_get_frame_label( OFA_IDATES_FILTER( bin ));
	gtk_label_set_markup( GTK_LABEL( label ), _( " Effect date selection " ));

	g_signal_connect( G_OBJECT( bin ), "ofa-changed", G_CALLBACK( on_date_filter_changed ), self );

	priv->dates_filter = bin;
}

static void
setup_others( ofaRenderBalancesBin *self, GtkContainer *parent )
{
	ofaRenderBalancesBinPrivate *priv;
	GtkWidget *toggle;

	priv = self->priv;

	/* setup the new_page btn before the per_class one in order to be
	 * safely updated when setting the later preference */
	toggle = my_utils_container_get_child_by_name( parent, "p3-new-page" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_new_page_toggled ), self );
	priv->new_page_btn = toggle;

	toggle = my_utils_container_get_child_by_name( parent, "p3-per-class" );
	g_return_if_fail( toggle && GTK_IS_CHECK_BUTTON( toggle ));
	g_signal_connect( G_OBJECT( toggle ), "toggled", G_CALLBACK( on_per_class_toggled ), self );
	priv->per_class_btn = toggle;
}

static void
on_from_account_changed( GtkEntry *entry, ofaRenderBalancesBin *self )
{
	on_account_changed( self, entry, self->priv->from_account_label, &self->priv->from_account );
}

static void
on_from_account_select( GtkButton *button, ofaRenderBalancesBin *self )
{
	on_account_select( button, self, self->priv->from_account_entry );
}

static void
on_to_account_changed( GtkEntry *entry, ofaRenderBalancesBin *self )
{
	on_account_changed( self, entry, self->priv->to_account_label, &self->priv->to_account );
}

static void
on_to_account_select( GtkButton *button, ofaRenderBalancesBin *self )
{
	on_account_select( button, self, self->priv->to_account_entry );
}

static void
on_account_changed( ofaRenderBalancesBin *self, GtkEntry *entry, GtkWidget *label, gchar **dest )
{
	ofaRenderBalancesBinPrivate *priv;
	ofoDossier *dossier;
	const gchar *cstr;
	ofoAccount *account;

	priv = self->priv;

	dossier = ofa_main_window_get_dossier( priv->main_window );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	cstr = gtk_entry_get_text( entry );
	account = ofo_account_get_by_number( dossier, cstr );
	if( account ){
		gtk_label_set_text( GTK_LABEL( label ), ofo_account_get_label( account ));
	} else {
		gtk_label_set_text( GTK_LABEL( label ), "" );
	}

	g_free( *dest );
	*dest = g_strdup( cstr );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_account_select( GtkButton *button, ofaRenderBalancesBin *self, GtkWidget *entry )
{
	ofaRenderBalancesBinPrivate *priv;
	gchar *number;

	priv = self->priv;

	number = ofa_account_select_run(
						priv->main_window,
						gtk_entry_get_text( GTK_ENTRY( entry )),
						ACCOUNT_ALLOW_DETAIL );
	if( number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), number );
		g_free( number );
	}
}

static void
on_all_accounts_toggled( GtkToggleButton *button, ofaRenderBalancesBin *self )
{
	ofaRenderBalancesBinPrivate *priv;
	gboolean bvalue;

	priv = self->priv;

	bvalue = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( priv->from_account_etiq, !bvalue );
	gtk_widget_set_sensitive( priv->from_account_entry, !bvalue );
	gtk_widget_set_sensitive( priv->from_account_btn, !bvalue );
	gtk_widget_set_sensitive( priv->from_account_label, !bvalue );

	gtk_widget_set_sensitive( priv->to_account_etiq, !bvalue );
	gtk_widget_set_sensitive( priv->to_account_entry, !bvalue );
	gtk_widget_set_sensitive( priv->to_account_btn, !bvalue );
	gtk_widget_set_sensitive( priv->to_account_label, !bvalue );

	priv->all_accounts = bvalue;

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_per_class_toggled( GtkToggleButton *button, ofaRenderBalancesBin *self )
{
	ofaRenderBalancesBinPrivate *priv;
	gboolean bvalue;

	priv = self->priv;

	bvalue = gtk_toggle_button_get_active( button );
	gtk_widget_set_sensitive( priv->new_page_btn, bvalue );

	priv->per_class = bvalue;

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_new_page_toggled( GtkToggleButton *button, ofaRenderBalancesBin *self )
{
	ofaRenderBalancesBinPrivate *priv;

	priv = self->priv;

	priv->new_page = gtk_toggle_button_get_active( button );

	g_signal_emit_by_name( self, "ofa-changed" );
}

static void
on_date_filter_changed( ofaIDatesFilter *filter, gint who, gboolean empty, gboolean valid, ofaRenderBalancesBin *self )
{
	g_signal_emit_by_name( self, "ofa-changed" );
}

/**
 * ofa_render_balances_bin_is_valid:
 * @bin:
 * @message: [out][allow-none]: the error message if any.
 *
 * Returns: %TRUE if the composite widget content is valid.
 */
gboolean
ofa_render_balances_bin_is_valid( ofaRenderBalancesBin *bin, gchar **message )
{
	ofaRenderBalancesBinPrivate *priv;
	gboolean valid;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), FALSE );

	priv = bin->priv;
	valid = FALSE;
	if( message ){
		*message = NULL;
	}

	if( !priv->dispose_has_run ){

		valid = ofa_idates_filter_is_valid(
						OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM, message ) &&
				ofa_idates_filter_is_valid(
						OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO, message );

		if( valid ){
			set_settings( bin );
		}
	}

	return( valid );
}

/**
 * ofa_render_balances_bin_get_from_account:
 */
const gchar *
ofa_render_balances_bin_get_from_account( const ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	const gchar *from_account;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), NULL );

	priv = bin->priv;
	from_account = NULL;

	if( !priv->dispose_has_run ){

		from_account = ( const gchar * ) priv->from_account;
	}

	return( from_account );
}

/**
 * ofa_render_balances_bin_get_to_account:
 */
const gchar *
ofa_render_balances_bin_get_to_account( const ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	const gchar *to_account;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), NULL );

	priv = bin->priv;
	to_account = NULL;

	if( !priv->dispose_has_run ){

		to_account = ( const gchar * ) priv->to_account;
	}

	return( to_account );
}

/**
 * ofa_render_balances_bin_get_all_accounts:
 */
gboolean
ofa_render_balances_bin_get_all_accounts( const ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	gboolean all_accounts;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), FALSE );

	priv = bin->priv;
	all_accounts = FALSE;

	if( !priv->dispose_has_run ){

		all_accounts = priv->all_accounts;
	}

	return( all_accounts );
}

/**
 * ofa_render_balances_bin_get_subtotal_per_class:
 */
gboolean
ofa_render_balances_bin_get_subtotal_per_class( const ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	gboolean subtotal;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), FALSE );

	priv = bin->priv;
	subtotal = FALSE;

	if( !priv->dispose_has_run ){

		subtotal = priv->per_class;
	}

	return( subtotal );
}

/**
 * ofa_render_balances_bin_get_new_page_per_class:
 */
gboolean
ofa_render_balances_bin_get_new_page_per_class( const ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	gboolean new_page;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), FALSE );

	priv = bin->priv;
	new_page = FALSE;

	if( !priv->dispose_has_run ){

		new_page = priv->new_page;
	}

	return( new_page );
}

/**
 * ofa_render_balances_bin_get_date_filter:
 */
ofaIDatesFilter *
ofa_render_balances_bin_get_dates_filter( const ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	ofaIDatesFilter *date_filter;

	g_return_val_if_fail( bin && OFA_IS_RENDER_BALANCES_BIN( bin ), NULL );

	priv = bin->priv;
	date_filter = NULL;

	if( !priv->dispose_has_run ){

		date_filter = OFA_IDATES_FILTER( priv->dates_filter );
	}

	return( date_filter );
}

/*
 * setttings:
 * account_from;account_to;all_accounts;effect_from;effect_to;subtotal_per_class;new_page_per_class;
 */
static void
load_settings( ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	GList *list, *it;
	const gchar *cstr;
	GDate date;

	priv = bin->priv;
	list = ofa_settings_get_string_list( st_settings );

	it = list;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->from_account_entry ), cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->to_account_entry ), cstr );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON( priv->all_accounts_btn ), my_utils_boolean_from_str( cstr ));
		on_all_accounts_toggled( GTK_TOGGLE_BUTTON( priv->all_accounts_btn ), bin );
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
				GTK_TOGGLE_BUTTON( priv->per_class_btn ), my_utils_boolean_from_str( cstr ));
		on_per_class_toggled( GTK_TOGGLE_BUTTON( priv->per_class_btn ), bin );
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
set_settings( ofaRenderBalancesBin *bin )
{
	ofaRenderBalancesBinPrivate *priv;
	gchar *str, *sdfrom, *sdto;

	priv = bin->priv;

	sdfrom = my_date_to_str(
			ofa_idates_filter_get_date(
					OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_FROM ), MY_DATE_SQL );
	sdto = my_date_to_str(
			ofa_idates_filter_get_date(
					OFA_IDATES_FILTER( priv->dates_filter ), IDATES_FILTER_TO ), MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;%s;%s;%s;%s;%s;",
			priv->from_account ? priv->from_account : "",
			priv->to_account ? priv->to_account : "",
			priv->all_accounts ? "True":"False",
			my_strlen( sdfrom ) ? sdfrom : "",
			my_strlen( sdto ) ? sdfrom : "",
			priv->per_class ? "True":"False",
			priv->new_page ? "True":"False" );

	ofa_settings_set_string( st_settings, str );

	g_free( sdfrom );
	g_free( sdto );
	g_free( str );
}
