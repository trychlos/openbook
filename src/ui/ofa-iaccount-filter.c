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
#include <gtk/gtk.h>

#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-ihubber.h"
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-iaccount-filter.h"

/* data associated to each implementor object
 */
typedef struct {
	gchar               *xml_name;
	const ofaMainWindow *main_window;
	ofaHub              *hub;
	ofoDossier          *dossier;
	gchar               *prefs_key;
	GtkSizeGroup        *group0;

	GtkWidget           *from_prompt;
	GtkWidget           *from_entry;
	GtkWidget           *from_select;
	GtkWidget           *from_label;
	gchar               *from_account;

	GtkWidget           *to_prompt;
	GtkWidget           *to_entry;
	GtkWidget           *to_select;
	GtkWidget           *to_label;
	gchar               *to_account;

	GtkWidget           *all_btn;
	gboolean             all_accounts;
}
	sIAccountFilter;

#define IACCOUNT_FILTER_LAST_VERSION      1
#define IACCOUNT_FILTER_DATA              "ofa-iaccount-filter-data"

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations = 0;	/* interface initialization count */

static GType             register_type( void );
static void              interface_base_init( ofaIAccountFilterInterface *klass );
static void              interface_base_finalize( ofaIAccountFilterInterface *klass );
static sIAccountFilter *get_iaccount_filter_data( ofaIAccountFilter *filter );
static void              on_widget_finalized( ofaIAccountFilter *iaccount_filter, void *finalized_widget );
static void              setup_composite( ofaIAccountFilter *filter, sIAccountFilter *sdata );
static void              on_from_changed( GtkEntry *entry, ofaIAccountFilter *filter );
static void              on_to_changed( GtkEntry *entry, ofaIAccountFilter *filter );
static void              on_account_changed( ofaIAccountFilter *filter, gint who, GtkEntry *entry, GtkWidget *label, gchar **account, sIAccountFilter *sdata );
static void              on_from_select_clicked( GtkButton *button, ofaIAccountFilter *filter );
static void              on_to_select_clicked( GtkButton *button, ofaIAccountFilter *filter );
static void              on_select_clicked( ofaIAccountFilter *filter, gint who, GtkWidget *entry, sIAccountFilter *sdata );
static void              on_all_accounts_toggled( GtkToggleButton *button, ofaIAccountFilter *filter );
static gboolean          is_account_valid( ofaIAccountFilter *filter, gint who, GtkEntry *entry, GtkWidget *label, gchar **account, sIAccountFilter *sdata );
static void              load_settings( ofaIAccountFilter *filter, sIAccountFilter *sdata );
static void              set_settings( ofaIAccountFilter *filter, sIAccountFilter *sdata );

/**
 * ofa_iaccount_filter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iaccount_filter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iaccount_filter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iaccount_filter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIAccountFilterInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIAccountFilter", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_CONTAINER );

	return( type );
}

static void
interface_base_init( ofaIAccountFilterInterface *klass )
{
	static const gchar *thisfn = "ofa_iaccount_filter_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIAccountFilter::ofa-changed:
		 *
		 * This signal is sent when one of the from or to accounts is changed.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIAccountFilter *filter,
		 * 						gpointer       user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-changed",
					OFA_TYPE_IACCOUNT_FILTER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					0,
					G_TYPE_NONE );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIAccountFilterInterface *klass )
{
	static const gchar *thisfn = "ofa_iaccount_filter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iaccount_filter_get_interface_last_version:
 * @instance: this #ofaIAccountFilter instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iaccount_filter_get_interface_last_version( const ofaIAccountFilter *instance )
{
	return( IACCOUNT_FILTER_LAST_VERSION );
}

/**
 * ofa_iaccount_filter_setup_bin:
 * @filter: this #ofaIAccountFilter instance.
 * @xml_name: the UI .xml definition file name.
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Initialize the composite widget which implements this interface.
 */
void
ofa_iaccount_filter_setup_bin( ofaIAccountFilter *filter, const gchar *xml_name, const ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_iaccount_filter_setup_bin";
	sIAccountFilter *sdata;
	GtkApplication *application;

	g_debug( "%s: filter=%p, xml_name=%s, main_window=%p",
			thisfn, ( void * ) filter, xml_name, ( void * ) main_window );

	g_return_if_fail( filter && G_IS_OBJECT( filter ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	sdata = get_iaccount_filter_data( filter );
	sdata->xml_name = g_strdup( xml_name );
	sdata->main_window = main_window;

	application = gtk_window_get_application( GTK_WINDOW( main_window ));
	g_return_if_fail( application && OFA_IS_IHUBBER( application ));

	sdata->hub = ofa_ihubber_get_hub( OFA_IHUBBER( application ));
	g_return_if_fail( sdata->hub && OFA_IS_HUB( sdata->hub ));

	sdata->dossier = ofa_hub_get_dossier( sdata->hub );
	g_return_if_fail( sdata->dossier && OFO_IS_DOSSIER( sdata->dossier ));

	setup_composite( filter, sdata );
}

static sIAccountFilter *
get_iaccount_filter_data( ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	sdata = ( sIAccountFilter * ) g_object_get_data( G_OBJECT( filter ), IACCOUNT_FILTER_DATA );

	if( !sdata ){
		sdata = g_new0( sIAccountFilter, 1 );
		g_object_set_data( G_OBJECT( filter ), IACCOUNT_FILTER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( filter ), ( GWeakNotify ) on_widget_finalized, filter );
	}

	return( sdata );
}

/*
 * called on ofaDateFilterBin composite widget finalization
 */
static void
on_widget_finalized( ofaIAccountFilter *filter, void *finalized_widget )
{
	static const gchar *thisfn = "ofa_iaccount_filter_on_widget_finalized";
	sIAccountFilter *sdata;

	g_debug( "%s: filter=%p (%s), ref_count=%d, finalized_widget=%p",
			thisfn,
			( void * ) filter, G_OBJECT_TYPE_NAME( filter ), G_OBJECT( filter )->ref_count,
			( void * ) finalized_widget );

	g_return_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ));

	sdata = get_iaccount_filter_data( filter );

	g_clear_object( &sdata->group0 );
	g_free( sdata->xml_name );
	g_free( sdata->prefs_key );
	g_free( sdata->from_account );
	g_free( sdata->to_account );
	g_free( sdata );

	g_object_set_data( G_OBJECT( filter ), IACCOUNT_FILTER_DATA, NULL );
}

static void
setup_composite( ofaIAccountFilter *filter, sIAccountFilter *sdata )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label, *button, *check;

	builder = gtk_builder_new_from_file( sdata->xml_name );

	object = gtk_builder_get_object( builder, "afb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	sdata->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "afb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( filter ), GTK_WINDOW( toplevel ), "top" );

	/* From: block */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	sdata->from_prompt = label;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	sdata->from_entry = entry;

	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	g_signal_connect( entry, "changed", G_CALLBACK( on_from_changed ), filter );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	sdata->from_select = button;

	g_signal_connect( button, "clicked", G_CALLBACK( on_from_select_clicked ), filter );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	sdata->from_label = label;

	/* To: block */
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	sdata->to_prompt = label;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	sdata->to_entry = entry;

	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	g_signal_connect( entry, "changed", G_CALLBACK( on_to_changed ), filter );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-select" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	sdata->to_select = button;

	g_signal_connect( button, "clicked", G_CALLBACK( on_to_select_clicked ), filter );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	sdata->to_label = label;

	check = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "all-accounts" );
	g_return_if_fail( check && GTK_IS_CHECK_BUTTON( check ));
	sdata->all_btn = check;

	g_signal_connect( check, "toggled", G_CALLBACK( on_all_accounts_toggled ), filter );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
on_from_changed( GtkEntry *entry, ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	sdata = get_iaccount_filter_data( filter );
	on_account_changed( filter, IACCOUNT_FILTER_FROM, entry, sdata->from_label, &sdata->from_account, sdata );
}

static void
on_to_changed( GtkEntry *entry, ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	sdata = get_iaccount_filter_data( filter );
	on_account_changed( filter, IACCOUNT_FILTER_TO, entry, sdata->to_label, &sdata->to_account, sdata );
}

static void
on_account_changed( ofaIAccountFilter *filter, gint who, GtkEntry *entry, GtkWidget *label, gchar **account, sIAccountFilter *sdata )
{
	is_account_valid( filter, who, entry, label, account, sdata );
	set_settings( filter, sdata );
	g_signal_emit_by_name( filter, "ofa-changed" );
}

static void
on_from_select_clicked( GtkButton *button, ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	sdata = get_iaccount_filter_data( filter );
	on_select_clicked( filter, IACCOUNT_FILTER_FROM, sdata->from_entry, sdata );
}

static void
on_to_select_clicked( GtkButton *button, ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	sdata = get_iaccount_filter_data( filter );
	on_select_clicked( filter, IACCOUNT_FILTER_TO, sdata->to_entry, sdata );
}

static void
on_select_clicked( ofaIAccountFilter *filter, gint who, GtkWidget *entry, sIAccountFilter *sdata )
{
	gchar *number;

	number = ofa_account_select_run(
					sdata->main_window,
					gtk_entry_get_text( GTK_ENTRY( entry )),
					ACCOUNT_ALLOW_ALL );
	if( number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), number );
		g_free( number );
	}
}

static void
on_all_accounts_toggled( GtkToggleButton *button, ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	sdata = get_iaccount_filter_data( filter );
	sdata->all_accounts = gtk_toggle_button_get_active( button );

	gtk_widget_set_sensitive( sdata->from_prompt, !sdata->all_accounts );
	gtk_widget_set_sensitive( sdata->from_entry, !sdata->all_accounts );
	gtk_widget_set_sensitive( sdata->from_select, !sdata->all_accounts );
	gtk_widget_set_sensitive( sdata->from_label, !sdata->all_accounts );

	gtk_widget_set_sensitive( sdata->to_prompt, !sdata->all_accounts );
	gtk_widget_set_sensitive( sdata->to_entry, !sdata->all_accounts );
	gtk_widget_set_sensitive( sdata->to_select, !sdata->all_accounts );
	gtk_widget_set_sensitive( sdata->to_label, !sdata->all_accounts );

	set_settings( filter, sdata );
	g_signal_emit_by_name( filter, "ofa-changed" );
}

/**
 * ofa_iaccount_filter_set_prefs:
 * @filter:
 * @prefs_key: the settings key where accounts are stored as a string list
 *
 * Load the settings from user preferences.
 */
void
ofa_iaccount_filter_set_prefs( ofaIAccountFilter *filter, const gchar *prefs_key )
{
	sIAccountFilter *sdata;

	g_return_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ));

	sdata = get_iaccount_filter_data( filter );

	g_free( sdata->prefs_key );
	sdata->prefs_key = g_strdup( prefs_key );

	load_settings( filter, sdata );
}

/**
 * ofa_iaccount_filter_get_account:
 * @filter:
 * @who: whether we are addressing the "From" date or the "To" one.
 *
 * Returns: The specified account number.
 */
const gchar *
ofa_iaccount_filter_get_account( ofaIAccountFilter *filter, gint who )
{
	static const gchar *thisfn = "ofa_iaccount_filter_get_account";
	sIAccountFilter *sdata;
	gchar *account;

	g_return_val_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ), NULL );

	sdata = get_iaccount_filter_data( filter );
	account = NULL;

	switch( who ){
		case IACCOUNT_FILTER_FROM:
			account = sdata->from_account;
			break;
		case IACCOUNT_FILTER_TO:
			account = sdata->to_account;
			break;
		default:
			g_warning( "%s: invalid account identifier: %d", thisfn, who );
	}

	return(( const gchar * ) account );
}

/**
 * ofa_iaccount_filter_set_account:
 * @filter:
 * @who: whether we are addressing the "From" date or the "To" one.
 * @account:
 */
void
ofa_iaccount_filter_set_account( ofaIAccountFilter *filter, gint who, const gchar *account )
{
	static const gchar *thisfn = "ofa_iaccount_filter_set_account";
	sIAccountFilter *sdata;

	g_return_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ));

	sdata = get_iaccount_filter_data( filter );

	switch( who ){
		case IACCOUNT_FILTER_FROM:
			gtk_entry_set_text( GTK_ENTRY( sdata->from_entry ), account ? account : "" );
			break;
		case IACCOUNT_FILTER_TO:
			gtk_entry_set_text( GTK_ENTRY( sdata->to_entry ), account ? account : "" );
			break;
		default:
			g_warning( "%s: invalid account identifier: %d", thisfn, who );
	}
}

/**
 * ofa_iaccount_filter_get_all_accounts:
 * @filter:
 *
 * Returns: Whether the "All accounts" checkbox is selected.
 */
gboolean
ofa_iaccount_filter_get_all_accounts( ofaIAccountFilter *filter )
{
	sIAccountFilter *sdata;

	g_return_val_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ), FALSE );

	sdata = get_iaccount_filter_data( filter );
	return( sdata->all_accounts );
}

/**
 * ofa_iaccount_filter_set_all_accounts:
 * @filter:
 * @all_accounts:
 */
void
ofa_iaccount_filter_set_all_accounts( ofaIAccountFilter *filter, gboolean all_accounts )
{
	sIAccountFilter *sdata;

	g_return_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ));

	sdata = get_iaccount_filter_data( filter );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( sdata->all_btn ), all_accounts );
	on_all_accounts_toggled( GTK_TOGGLE_BUTTON( sdata->all_btn ), filter );
}

/**
 * ofa_iaccount_filter_is_valid:
 * @filter:
 * @who: whether we are addressing the "From" account or the "To" one.
 * @message: [out][allow-none]: will be set to an error message.
 *
 * Returns: %TRUE is the specified account is valid.
 */
gboolean
ofa_iaccount_filter_is_valid( ofaIAccountFilter *filter, gint who, gchar **message )
{
	static const gchar *thisfn = "ofa_iaccount_filter_is_valid";
	sIAccountFilter *sdata;
	gboolean valid;
	GtkWidget *entry, *label;
	gchar **number;
	gchar *str;

	g_return_val_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ), FALSE );

	sdata = get_iaccount_filter_data( filter );
	valid = FALSE;
	entry = NULL;
	label = NULL;
	number = NULL;
	if( message ){
		*message = NULL;
	}

	switch( who ){
		case IACCOUNT_FILTER_FROM:
			number = &sdata->from_account;
			label = sdata->from_label;
			entry = sdata->from_entry;
			break;
		case IACCOUNT_FILTER_TO:
			number = &sdata->to_account;
			label = sdata->to_label;
			entry = sdata->to_entry;
			break;
		default:
			str = g_strdup_printf( "%s: invalid account identifier: %d", thisfn, who );
			g_warning( "%s", str );
			if( message ){
				*message = g_strdup( str );
			}
			g_free( str );
	}

	valid = is_account_valid( filter, who, GTK_ENTRY( entry ), label, number, sdata );

	if( !valid && message ){
		switch( who ){
			case IACCOUNT_FILTER_FROM:
				*message = g_strdup( _( "From account is not valid" ));
				break;
			case IACCOUNT_FILTER_TO:
				*message = g_strdup( _( "To account is not valid" ));
				break;
		}
	}

	return( valid );
}

static gboolean
is_account_valid( ofaIAccountFilter *filter, gint who, GtkEntry *entry, GtkWidget *label, gchar **account, sIAccountFilter *sdata )
{
	gboolean valid;
	const gchar *cstr;
	ofoAccount *account_obj;

	valid = FALSE;
	g_free( *account );
	*account = NULL;
	gtk_label_set_text( GTK_LABEL( label ), "" );

	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		*account = g_strdup( cstr );
		account_obj = ofo_account_get_by_number( sdata->hub, cstr );
		if( account_obj && OFO_IS_ACCOUNT( account_obj )){
			gtk_label_set_text( GTK_LABEL( label ), ofo_account_get_label( account_obj ));
			valid = TRUE;
		}
	}

	return( valid );
}

/**
 * ofa_iaccount_filter_get_frame_label:
 * @filter:
 *
 * Returns: a pointer to the GtkWidget which is used as the frame label.
 */
GtkWidget *
ofa_iaccount_filter_get_frame_label( ofaIAccountFilter *filter )
{
	g_return_val_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ), NULL );

	return( my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "frame-label" ));
}

/**
 * ofa_iaccount_filter_get_from_prompt:
 * @filter:
 *
 * Returns: a pointer to the GtkWidget which is used as the "From" prompt.
 */
GtkWidget *
ofa_iaccount_filter_get_from_prompt( ofaIAccountFilter *filter )
{
	g_return_val_if_fail( filter && OFA_IS_IACCOUNT_FILTER( filter ), NULL );

	return( my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-prompt" ));
}

/*
 * settings are: from;to;all_accounts;
 */
static void
load_settings( ofaIAccountFilter *filter, sIAccountFilter *sdata )
{
	GList *slist, *it;
	const gchar *cstr;
	gboolean all_accounts;

	slist = ofa_settings_user_get_string_list( sdata->prefs_key );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			gtk_entry_set_text( GTK_ENTRY( sdata->from_entry ), cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			gtk_entry_set_text( GTK_ENTRY( sdata->to_entry ), cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			all_accounts = my_utils_boolean_from_str( cstr );
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( sdata->all_btn ), all_accounts );
			on_all_accounts_toggled( GTK_TOGGLE_BUTTON( sdata->all_btn ), filter );
		}

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaIAccountFilter *filter, sIAccountFilter *sdata )
{
	gchar *str;

	if( my_strlen( sdata->prefs_key )){

		str = g_strdup_printf( "%s;%s;%s;",
				gtk_entry_get_text( GTK_ENTRY( sdata->from_entry )),
				gtk_entry_get_text( GTK_ENTRY( sdata->to_entry )),
				sdata->all_accounts ? "True":"False" );

		ofa_settings_user_set_string( sdata->prefs_key, str );

		g_free( str );
	}
}
