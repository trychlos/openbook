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

#include "my/my-ibin.h"
#include "my/my-period-bin.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean      dispose_has_run;

	/* initialization
	 */
	myISettings  *settings;

	/* UI
	 */
	GtkWidget    *key_combo;
	GtkWidget    *every_prompt;
	GtkWidget    *every_entry;
	GtkWidget    *every_label;
	GtkWidget    *det_parent;
	GtkWidget    *det_prompt;
	GtkWidget    *det_details;
	GtkSizeGroup *group0;
	GtkWidget    *weekly_menu;
	GtkWidget    *monthly_menu;
	GtkWidget    *yearly_menu;
	GtkWidget    *det_button;

	/* runtime data
	 */
	myPeriod     *period;
	myPeriodKey   prev_key;
	GtkListStore *key_store;
	guint         popup_attach;				/* when setting up the popup menus */
	GtkWidget    *popup_menu;
	gulong        every_handler;
	gulong        det_handler;
}
	myPeriodBinPrivate;

/* columns displayed in the periodicity combobox
 */
enum {
	COL_PER_LABEL = 0,
	COL_PER_ID_S,
	COL_PER_N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/my/my-period-bin.ui";
static const gchar *st_item_data        = "my-period-bin-item";

static void          setup_bin( myPeriodBin *bin );
static void          setup_key_combo( myPeriodBin *self );
static void          setup_key_cb( myPeriodKey type, myPeriodBin *self );
static void          setup_popup_menu( myPeriodBin *self, GtkWidget **popup, myPeriodKey key );
static void          setup_popup_cb( guint idn, const gchar *ids, const gchar *abr, const gchar *label, myPeriodBin *self );
static void          key_combo_on_changed( GtkComboBox *box, myPeriodBin *self );
static void          popup_menu_init( myPeriodBin *self, GtkWidget *popup );
static void          popup_on_each_clicked( GtkButton *button, GtkWidget *popup );
static void          every_init( myPeriodBin *self, const gchar *label );
static void          every_on_changed( GtkEntry *entry, myPeriodBin *self );
static void          popup_set_from_period( myPeriodBin *self );
static void          popup_on_item_activated( GtkMenuItem *item, myPeriodBin *self );
static void          details_set_list( myPeriodBin *self );
static void          details_on_changed( GtkEntry *entry, myPeriodBin *self );
static void          on_bin_changed( myPeriodBin *self );
static void          read_settings( myPeriodBin *self );
static void          write_settings( myPeriodBin *self );
static void          ibin_iface_init( myIBinInterface *iface );
static guint         ibin_get_interface_version( void );
static GtkSizeGroup *ibin_get_size_group( const myIBin *instance, guint column );
static gboolean      ibin_is_valid( const myIBin *instance, gchar **msgerr );

G_DEFINE_TYPE_EXTENDED( myPeriodBin, my_period_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( myPeriodBin )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IBIN, ibin_iface_init ))

static void
period_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_period_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_PERIOD_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_period_bin_parent_class )->finalize( instance );
}

static void
period_bin_dispose( GObject *instance )
{
	myPeriodBinPrivate *priv;

	g_return_if_fail( instance && MY_IS_PERIOD_BIN( instance ));

	priv = my_period_bin_get_instance_private( MY_PERIOD_BIN( instance ));

	if( !priv->dispose_has_run ){

		write_settings( MY_PERIOD_BIN( instance ));

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->period );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_period_bin_parent_class )->dispose( instance );
}

static void
my_period_bin_init( myPeriodBin *self )
{
	static const gchar *thisfn = "my_period_bin_init";
	myPeriodBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_PERIOD_BIN( self ));

	priv = my_period_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->prev_key = 0;
}

static void
my_period_bin_class_init( myPeriodBinClass *klass )
{
	static const gchar *thisfn = "my_period_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = period_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = period_bin_finalize;
}

/**
 * my_period_bin_new:
 * @settings: a #myISettings instance.
 *
 * Returns: a new #myPeriodBin widget.
 */
myPeriodBin *
my_period_bin_new( myISettings *settings )
{
	myPeriodBin *bin;
	myPeriodBinPrivate *priv;

	g_return_val_if_fail( settings && MY_IS_ISETTINGS( settings ), NULL );

	bin = g_object_new( MY_TYPE_PERIOD_BIN, NULL );

	priv = my_period_bin_get_instance_private( bin );

	priv->settings = settings;

	setup_bin( bin );
	setup_popup_menu( bin, &priv->weekly_menu, MY_PERIOD_WEEKLY );
	setup_popup_menu( bin, &priv->monthly_menu, MY_PERIOD_MONTHLY );
	setup_popup_menu( bin, &priv->yearly_menu, MY_PERIOD_YEARLY );
	read_settings( bin );

	my_period_bin_set_period( bin, NULL );

	return( bin );
}

static void
setup_bin( myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *prompt;

	priv = my_period_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "mpb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "mpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* identifier combo box */
	priv->key_combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "key-combo" );
	g_return_if_fail( priv->key_combo && GTK_IS_COMBO_BOX( priv->key_combo ));
	setup_key_combo( self );

	prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "key-prompt" );
	g_return_if_fail( prompt && GTK_IS_LABEL( prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( prompt ), priv->key_combo );

	g_signal_connect( priv->key_combo, "changed", G_CALLBACK( key_combo_on_changed ), self );

	/* every entry */
	priv->every_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "every-entry" );
	g_return_if_fail( priv->every_entry && GTK_IS_ENTRY( priv->every_entry ));

	priv->every_handler = g_signal_connect( priv->every_entry, "changed", G_CALLBACK( every_on_changed ), self );

	priv->every_prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "every-prompt" );
	g_return_if_fail( priv->every_prompt && GTK_IS_LABEL( priv->every_prompt ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( priv->every_prompt ), priv->every_entry );

	priv->every_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "every-label" );
	g_return_if_fail( priv->every_label && GTK_IS_LABEL( priv->every_label ));

	/* details parent */
	priv->det_parent = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "det-parent" );
	g_return_if_fail( priv->det_parent && GTK_IS_BOX( priv->det_parent ));

	priv->det_prompt = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "det-prompt" );
	g_return_if_fail( priv->det_prompt && GTK_IS_LABEL( priv->det_prompt ));

	priv->det_details = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "det-details" );
	g_return_if_fail( priv->det_details && GTK_IS_ENTRY( priv->det_details ));

	priv->det_handler = g_signal_connect( priv->det_details, "changed", G_CALLBACK( details_on_changed ), self );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_key_combo( myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	GtkCellRenderer *cell;

	priv = my_period_bin_get_instance_private( self );

	priv->key_store = gtk_list_store_new( COL_PER_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING );
	gtk_combo_box_set_model( GTK_COMBO_BOX( priv->key_combo ), GTK_TREE_MODEL( priv->key_store ));
	g_object_unref( priv->key_store );

	my_period_enum_key(( myPeriodEnumKeyCb ) setup_key_cb, self );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( priv->key_combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( priv->key_combo ), cell, "text", COL_PER_LABEL );

	g_signal_connect( priv->key_combo, "changed", G_CALLBACK( key_combo_on_changed ), self );

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( priv->key_combo ), COL_PER_ID_S );
}

static void
setup_key_cb( myPeriodKey type, myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	GtkTreeIter iter;

	priv = my_period_bin_get_instance_private( self );

	gtk_list_store_insert_with_values(
			priv->key_store,
			&iter,
			-1,
			COL_PER_LABEL, my_period_key_get_label( type ),
			COL_PER_ID_S,  my_period_key_get_dbms( type ),
			-1 );
}

/*
 * prepare (once) the popup menu
 * to let the user select which day of the week/month/year the period
 * must be generated
 */
static void
setup_popup_menu( myPeriodBin *self, GtkWidget **menu, myPeriodKey key )
{
	myPeriodBinPrivate *priv;

	priv = my_period_bin_get_instance_private( self );

	*menu = gtk_menu_new();
	priv->popup_menu = *menu;
	priv->popup_attach = 0;
	my_period_enum_details( key, ( myPeriodEnumDetailsCb ) setup_popup_cb, self );
}

static void
setup_popup_cb( guint idn, const gchar *ids, const gchar *abr, const gchar *label, myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	GtkWidget *item;

	priv = my_period_bin_get_instance_private( self );

	item = gtk_check_menu_item_new_with_label( label );
	g_object_set_data( G_OBJECT( item ), st_item_data, GUINT_TO_POINTER( idn ));
	g_signal_connect( item, "activate", G_CALLBACK( popup_on_item_activated ), self );
	gtk_menu_attach( GTK_MENU( priv->popup_menu ), item, 0, 1, priv->popup_attach, priv->popup_attach+1 );
	priv->popup_attach += 1;
}

/**
 * my_period_bin_set_period:
 * @bin: this #myPeriodBin instance.
 * @period: [allow-none]: a #myPeriod object to be edited here.
 *
 * Set the @period.
 */
void
my_period_bin_set_period( myPeriodBin *bin, myPeriod *period )
{
	static const gchar *thisfn = "my_period_bin_set_period";
	myPeriodBinPrivate *priv;
	myPeriodKey key;

	g_debug( "%s: bin=%p, period=%p", thisfn, ( void * ) bin, ( void * ) period );

	g_return_if_fail( bin && MY_IS_PERIOD_BIN( bin ));
	g_return_if_fail( !period || MY_IS_PERIOD( period ));

	priv = my_period_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_clear_object( &priv->period );
	if( period ){
		priv->period = g_object_ref( period );
	} else {
		priv->period = my_period_new();
	}

	key = my_period_get_key( priv->period );
	gtk_combo_box_set_active_id( GTK_COMBO_BOX( priv->key_combo ), my_period_key_get_dbms( key ));
}

static void
key_combo_on_changed( GtkComboBox *box, myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	const gchar *dbms;
	myPeriodKey new_key;

	priv = my_period_bin_get_instance_private( self );

	dbms = gtk_combo_box_get_active_id( GTK_COMBO_BOX( priv->key_combo ));
	new_key = my_period_key_from_dbms( dbms );

	if( new_key != priv->prev_key ){
		my_period_set_key( priv->period, new_key );
		priv->prev_key = new_key;

		every_init( self, NULL );

		switch( new_key ){
			case MY_PERIOD_DAILY:
				every_init( self, _( "day(s)" ));
				break;

			case MY_PERIOD_WEEKLY:
				every_init( self, _( "week(s)" ));
				popup_menu_init( self, priv->weekly_menu );
				break;

			case MY_PERIOD_MONTHLY:
				every_init( self, _( "month(s)" ));
				popup_menu_init( self, priv->monthly_menu );
				break;

			case MY_PERIOD_YEARLY:
				every_init( self, _( "year(s)" ));
				popup_menu_init( self, priv->yearly_menu );
				break;

			default:
				break;
		}

		on_bin_changed( self );
	}
}

static void
popup_menu_init( myPeriodBin *self, GtkWidget *popup )
{
	myPeriodBinPrivate *priv;

	//g_debug( "popup_init" );

	priv = my_period_bin_get_instance_private( self );

	priv->popup_menu = popup;
	priv->det_button = gtk_button_new_with_mnemonic( _( "On _each" ));
	gtk_container_add( GTK_CONTAINER( priv->det_parent ), priv->det_button );
	g_signal_connect( priv->det_button, "clicked", G_CALLBACK( popup_on_each_clicked ), popup );

	gtk_widget_show_all( popup );
	gtk_widget_show_all( priv->det_parent );
	gtk_widget_show_all( priv->det_prompt );
	gtk_widget_show_all( priv->det_details );

	details_set_list( self );
	popup_set_from_period( self );
}

static void
popup_on_each_clicked( GtkButton *button, GtkWidget *popup )
{
	gtk_menu_popup_at_widget(
			GTK_MENU( popup ), GTK_WIDGET( button ), GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL );
}

static void
every_init( myPeriodBin *self, const gchar *label )
{
	myPeriodBinPrivate *priv;
	gchar *str;

	//g_debug( "every_init: label=%s", label );

	priv = my_period_bin_get_instance_private( self );

	if( my_strlen( label )){
		gtk_widget_show( priv->every_prompt );

		str = g_strdup_printf( "%u", my_period_get_every( priv->period ));
		gtk_entry_set_text( GTK_ENTRY( priv->every_entry ), str );
		g_free( str );
		gtk_widget_show( priv->every_entry );

		gtk_label_set_text( GTK_LABEL( priv->every_label ), label );
		gtk_widget_show( priv->every_label );

	} else {
		/* reset and hide every and details widgets
		 * but do not trigger handlers so that myPeriod is not updated */
		gtk_widget_hide( priv->every_prompt );
		g_signal_handler_block( priv->every_entry, priv->every_handler );
		gtk_entry_set_text( GTK_ENTRY( priv->every_entry ), "" );
		g_signal_handler_unblock( priv->every_entry, priv->every_handler );
		gtk_widget_hide( priv->every_entry );
		gtk_widget_hide( priv->every_label );

		gtk_widget_hide( priv->det_prompt );
		g_signal_handler_block( priv->det_details, priv->det_handler );
		gtk_entry_set_text( GTK_ENTRY( priv->det_details ), "" );
		g_signal_handler_unblock( priv->det_details, priv->det_handler );
		gtk_widget_hide( priv->det_details );

		gtk_container_foreach( GTK_CONTAINER( priv->det_parent ), ( GtkCallback ) gtk_widget_destroy, NULL );
	}
}

static void
every_on_changed( GtkEntry *entry, myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	const gchar *cstr;

	priv = my_period_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		my_period_set_every( priv->period, atoi( cstr ));
	} else {
		my_period_set_every( priv->period, 0 );
	}

	on_bin_changed( self );
}

static void
popup_set_from_period( myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	GList *details, *items_list, *it;
	GtkWidget *item;
	guint idn;

	priv = my_period_bin_get_instance_private( self );

	details = my_period_get_details( priv->period );

	items_list = gtk_container_get_children( GTK_CONTAINER( priv->popup_menu ));
	for( it=items_list ; it ; it=it->next ){
		item = ( GtkWidget * ) it->data;
		idn = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( item ), st_item_data ));
		g_return_if_fail( item && GTK_IS_CHECK_MENU_ITEM( item ));
		gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( item ), g_list_find( details, GUINT_TO_POINTER( idn )) != NULL );
	}
}

static void
popup_on_item_activated( GtkMenuItem *item, myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	guint idn;
	gboolean active;

	priv = my_period_bin_get_instance_private( self );

	idn = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( item ), st_item_data ));
	active = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( item ));

	if( active ){
		my_period_details_add( priv->period, idn );
	} else {
		my_period_details_remove( priv->period, idn );
	}

	details_set_list( self );

	on_bin_changed( self );
}

static void
details_set_list( myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	gchar *str;

	priv = my_period_bin_get_instance_private( self );

	g_signal_handler_block( priv->det_details, priv->det_handler );

	str = my_period_get_details_str_i( priv->period );

	if( my_strlen( str )){
		gtk_entry_set_text( GTK_ENTRY( priv->det_details ), str );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->det_details ), "" );
	}

	g_free( str );

	g_signal_handler_unblock( priv->det_details, priv->det_handler );
}

static void
details_on_changed( GtkEntry *entry, myPeriodBin *self )
{
	myPeriodBinPrivate *priv;
	const gchar *cstr;

	//g_debug( "my_period_bin_details_on_changed" );

	priv = my_period_bin_get_instance_private( self );

	cstr = gtk_entry_get_text( entry );
	if( my_strlen( cstr )){
		my_period_set_details( priv->period, cstr );
	} else {
		my_period_set_details( priv->period, NULL );
	}

	popup_set_from_period( self );

	on_bin_changed( self );
}

/*
 * If something has changed, advise the parent container
 */
static void
on_bin_changed( myPeriodBin *self )
{
	//g_debug( "my_period_bin_on_bin_changed" );

	g_signal_emit_by_name( self, "my-ibin-changed" );
}

/**
 * my_period_bin_get_period:
 * @bin: this #myPeriodBin instance.
 *
 * Returns: the #myPeriod object.
 */
myPeriod *
my_period_bin_get_period( myPeriodBin *bin )
{
	myPeriodBinPrivate *priv;

	g_return_val_if_fail( bin && MY_IS_PERIOD_BIN( bin ), NULL );

	priv = my_period_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->period );
}

/*
 * settings are: month:enter/select; year:enter/select
 */
static void
read_settings( myPeriodBin *self )
{
}

static void
write_settings( myPeriodBin *self )
{
}

/*
 * myIBin interface management
 */
static void
ibin_iface_init( myIBinInterface *iface )
{
	static const gchar *thisfn = "my_period_bin_ibin_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ibin_get_interface_version;
	iface->get_size_group = ibin_get_size_group;
	iface->is_valid = ibin_is_valid;
}

static guint
ibin_get_interface_version( void )
{
	return( 1 );
}

static GtkSizeGroup *
ibin_get_size_group( const myIBin *instance, guint column )
{
	static const gchar *thisfn = "my_period_bin_ibin_get_size_group";
	myPeriodBinPrivate *priv;

	g_debug( "%s: instance=%p, column=%u", thisfn, ( void * ) instance, column );

	g_return_val_if_fail( instance && MY_IS_PERIOD_BIN( instance ), NULL );

	priv = my_period_bin_get_instance_private( MY_PERIOD_BIN( instance ));

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: invalid column=%u", thisfn, column );

	return( NULL );
}

/*
 * ibin_is_valid:
 * @bin: this #myPeriodBin instance.
 * @msgerr: [allow-none]: set to the error message as a newly
 *  allocated string which should be g_free() by the caller.
 *
 * Returns: %TRUE if the current datas are able to generate something:
 * - it is not unset
 * - it repeats at least once
 * - at least one detail is set (for periods other than daily)
 */
gboolean
ibin_is_valid( const myIBin *instance, gchar **msgerr )
{
	myPeriod *period;
	gboolean valid;

	//g_debug( "my_period_bin_ibin_is_valid" );

	valid = FALSE;
	if( msgerr ){
		*msgerr = NULL;
	}

	period = my_period_bin_get_period( MY_PERIOD_BIN( instance ));

	if( period ){
		valid = my_period_is_valid( period, msgerr );

	} else if( msgerr ){
		*msgerr = g_strdup( _( "Period is not set" ));
	}

	return( valid );
}
