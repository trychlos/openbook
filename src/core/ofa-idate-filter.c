/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
#include <gtk/gtk.h>

#include "my/my-date-editable.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idate-filter.h"
#include "api/ofa-preferences.h"

/* data associated to each implementor object
 */
typedef struct {

	/* initialization
	 */
	ofaHub       *hub;
	gchar        *ui_resource;

	/* runtime
	 */
	gboolean      mandatory;
	gchar        *settings_key;
	GtkSizeGroup *group0;

	GtkWidget    *from_entry;
	GDate         from_date;

	GtkWidget    *to_entry;
	GDate         to_date;
}
	sIDateFilter;

#define IDATE_FILTER_LAST_VERSION       1
#define IDATE_FILTER_DATA              "ofa-idate-filter-data"

#define DEFAULT_MANDATORY               FALSE

/* signals defined here
 */
enum {
	CHANGED = 0,
	FOCUS_OUT,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIDateFilterInterface *klass );
static void          interface_base_finalize( ofaIDateFilterInterface *klass );
static sIDateFilter *get_idate_filter_data( ofaIDateFilter *filter );
static void          on_widget_finalized( ofaIDateFilter *filter, void *finalized_widget );
static void          setup_bin( ofaIDateFilter *filter, sIDateFilter *sdata );
static void          on_from_changed( GtkEntry *entry, ofaIDateFilter *filter );
static gboolean      on_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaIDateFilter *filter );
static void          on_to_changed( GtkEntry *entry, ofaIDateFilter *filter );
static gboolean      on_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaIDateFilter *filter );
static void          on_date_changed( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date );
static gboolean      on_date_focus_out( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date, sIDateFilter *sdata );
static void          read_settings( ofaIDateFilter *filter, sIDateFilter *sdata );
static void          write_settings( ofaIDateFilter *filter, sIDateFilter *sdata );

/**
 * ofa_idate_filter_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idate_filter_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idate_filter_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idate_filter_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDateFilterInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDateFilter", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_CONTAINER );

	return( type );
}

static void
interface_base_init( ofaIDateFilterInterface *klass )
{
	static const gchar *thisfn = "ofa_idate_filter_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/**
		 * ofaIDateFilter::ofa-changed:
		 *
		 * This signal is sent when one of the from or to dates is changed.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIDateFilter *filter,
		 * 						gint           who,
		 * 						gboolean       empty,
		 * 						gboolean       valid,
		 * 						gpointer       user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-changed",
					OFA_TYPE_IDATE_FILTER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					3,
					G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN );

		/**
		 * ofaIDateFilter::ofa-focus-out:
		 *
		 * This signal is sent when one of the from or to date entries lose
		 * the focus. The date is supposed to be complete.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaIDateFilter *filter,
		 * 						gint           who,
		 * 						gboolean       empty,
		 * 						GDate         *date,
		 * 						gpointer       user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"ofa-focus-out",
					OFA_TYPE_IDATE_FILTER,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					3,
					G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_POINTER );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIDateFilterInterface *klass )
{
	static const gchar *thisfn = "ofa_idate_filter_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idate_filter_get_interface_last_version:
 * @instance: this #ofaIDateFilter instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idate_filter_get_interface_last_version( void )
{
	return( IDATE_FILTER_LAST_VERSION );
}

/**
 * ofa_idate_filter_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_idate_filter_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_IDATE_FILTER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaIDateFilterInterface * ) iface )->get_interface_version ){
		version = (( ofaIDateFilterInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaIDateFilter::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_idate_filter_setup_bin:
 * @filter: this #ofaIDateFilter instance.
 * @hub: the #ofaHub object of the application.
 * @ui_resource: the path of the binary resource which holds the user
 *  interface XML description.
 *
 * Initialize the widget which implements this interface.
 */
void
ofa_idate_filter_setup_bin( ofaIDateFilter *filter, ofaHub *hub, const gchar *ui_resource )
{
	static const gchar *thisfn = "ofa_idate_filter_setup_bin";
	sIDateFilter *sdata;

	g_debug( "%s: filter=%p, ui_resource=%s", thisfn, ( void * ) filter, ui_resource );

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));
	g_return_if_fail( G_IS_OBJECT( filter ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	sdata = get_idate_filter_data( filter );

	sdata->hub = hub;
	sdata->ui_resource = g_strdup( ui_resource );
	sdata->mandatory = DEFAULT_MANDATORY;

	setup_bin( filter, sdata );
}

static sIDateFilter *
get_idate_filter_data( ofaIDateFilter *filter )
{
	sIDateFilter *sdata;

	sdata = ( sIDateFilter * ) g_object_get_data( G_OBJECT( filter ), IDATE_FILTER_DATA );

	if( !sdata ){
		sdata = g_new0( sIDateFilter, 1 );
		g_object_set_data( G_OBJECT( filter ), IDATE_FILTER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( filter ), ( GWeakNotify ) on_widget_finalized, filter );
	}

	return( sdata );
}

/*
 * called on ofaDateFilterBin composite widget finalization
 */
static void
on_widget_finalized( ofaIDateFilter *filter, void *finalized_widget )
{
	static const gchar *thisfn = "ofa_idate_filter_on_widget_finalized";
	sIDateFilter *sdata;

	g_debug( "%s: filter=%p (%s), ref_count=%d, finalized_widget=%p",
			thisfn,
			( void * ) filter, G_OBJECT_TYPE_NAME( filter ), G_OBJECT( filter )->ref_count,
			( void * ) finalized_widget );

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));

	sdata = get_idate_filter_data( filter );

	g_clear_object( &sdata->group0 );
	g_free( sdata->ui_resource );
	g_free( sdata->settings_key );
	g_free( sdata );

	g_object_set_data( G_OBJECT( filter ), IDATE_FILTER_DATA, NULL );
}

static void
setup_bin( ofaIDateFilter *filter, sIDateFilter *sdata )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	builder = gtk_builder_new_from_resource( sdata->ui_resource );

	object = gtk_builder_get_object( builder, "dfb-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	sdata->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "dfb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( filter ), GTK_WINDOW( toplevel ), "top" );

	/* From: block */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	sdata->from_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display( sdata->hub ));
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check( sdata->hub ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), sdata->mandatory );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite( sdata->hub ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_from_changed ), filter );
	g_signal_connect( entry, "focus-out-event", G_CALLBACK( on_from_focus_out ), filter );

	/* To: block */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	sdata->to_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_date_editable_init( GTK_EDITABLE( entry ));
	my_date_editable_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display( sdata->hub ));
	my_date_editable_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check( sdata->hub ));
	my_date_editable_set_mandatory( GTK_EDITABLE( entry ), sdata->mandatory );
	my_date_editable_set_overwrite( GTK_EDITABLE( entry ), ofa_prefs_date_overwrite( sdata->hub ));

	g_signal_connect( entry, "changed", G_CALLBACK( on_to_changed ), filter );
	g_signal_connect( entry, "focus-out-event", G_CALLBACK( on_to_focus_out ), filter );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

/**
 * ofa_idate_filter_add_widget:
 * @filter: this #ofaIDateFilter instance.
 * @widget: the widget to be added.
 * @where: where the widget is to be added in the composite UI.
 *
 * Add an application-specific widget to the composite.
 *
 * Only one widget should be added to the implementation widget.
 * But, unless otherwise specified, neither the interface nor by default
 * the implementation class check than several widgets are successively
 * added. So it is up to the application to only call this method once.
 */
void
ofa_idate_filter_add_widget( ofaIDateFilter *filter, GtkWidget *widget, gint where )
{
	static const gchar *thisfn = "ofa_idate_filter_add_widget";

	g_debug( "%s: filter=%p, widget=%p, where=%d",
			thisfn, ( void * ) filter, ( void * ) widget, where );

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));
	g_return_if_fail( G_IS_OBJECT( filter ));

	if( OFA_IDATE_FILTER_GET_INTERFACE( filter )->add_widget ){
		OFA_IDATE_FILTER_GET_INTERFACE( filter )->add_widget( filter, widget, where );
		return;
	}

	g_info( "%s: ofaIDateFilter's %s implementation does not provide 'add_widget()' method",
			thisfn, G_OBJECT_TYPE_NAME( filter ));
}

static void
on_from_changed( GtkEntry *entry, ofaIDateFilter *filter )
{
	sIDateFilter *sdata;

	sdata = get_idate_filter_data( filter );
	on_date_changed( filter, IDATE_FILTER_FROM, entry, &sdata->from_date );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaIDateFilter *filter )
{
	sIDateFilter *sdata;

	sdata = get_idate_filter_data( filter );
	return( on_date_focus_out( filter, IDATE_FILTER_FROM, entry, &sdata->from_date, sdata ));
}

static void
on_to_changed( GtkEntry *entry, ofaIDateFilter *filter )
{
	sIDateFilter *sdata;

	sdata = get_idate_filter_data( filter );
	on_date_changed( filter, IDATE_FILTER_TO, entry, &sdata->to_date );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaIDateFilter *filter )
{
	sIDateFilter *sdata;

	sdata = get_idate_filter_data( filter );
	return( on_date_focus_out( filter, IDATE_FILTER_TO, entry, &sdata->to_date, sdata ));
}

static void
on_date_changed( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date )
{
	gboolean empty, valid;

	my_date_set_from_date( date, my_date_editable_get_date( GTK_EDITABLE( entry ), NULL ));
	empty = my_date_editable_is_empty( GTK_EDITABLE( entry ));
	valid = my_date_is_valid( date );

	g_signal_emit_by_name( filter, "ofa-changed", who, empty, valid );
}

/*
 * only record the date in settings if it is valid
 */
static gboolean
on_date_focus_out( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date, sIDateFilter *sdata )
{
	my_date_set_from_date( date, my_date_editable_get_date( GTK_EDITABLE( entry ), NULL ));

	if( my_date_is_valid( date ) ||
			( my_date_editable_is_empty( GTK_EDITABLE( entry )) && !sdata->mandatory )){

		write_settings( filter, sdata );
	}

	g_signal_emit_by_name( filter, "ofa-focus-out", who, date );

	return( FALSE );
}

/**
 * ofa_idate_filter_set_settings_key:
 * @filter: this #ofaIDateFilter instance.
 * @settings_key: the user settings key;
 *  dates are stored as a string list of SQL-formatted dates.
 *
 * Setup the settings key, and load the settings from user preferences.
 */
void
ofa_idate_filter_set_settings_key( ofaIDateFilter *filter, const gchar *settings_key )
{
	sIDateFilter *sdata;

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));

	sdata = get_idate_filter_data( filter );

	g_free( sdata->settings_key );
	sdata->settings_key = g_strdup( settings_key );

	read_settings( filter, sdata );
}

/**
 * ofa_idate_filter_get_date:
 * @filter:
 * @who: whether we are addressing the "From" date or the "To" one.
 *
 * Returns: The specified date.
 */
const GDate *
ofa_idate_filter_get_date( ofaIDateFilter *filter, gint who )
{
	static const gchar *thisfn = "ofa_idate_filter_get_date";
	sIDateFilter *sdata;
	GDate *date;

	g_return_val_if_fail( filter && OFA_IS_IDATE_FILTER( filter ), NULL );

	sdata = get_idate_filter_data( filter );
	date = NULL;

	switch( who ){
		case IDATE_FILTER_FROM:
			date = &sdata->from_date;
			break;
		case IDATE_FILTER_TO:
			date = &sdata->to_date;
			break;
		default:
			g_warning( "%s: invalid date identifier: %d", thisfn, who );
	}

	return(( const GDate * ) date );
}

/**
 * ofa_idate_filter_set_date:
 * @filter:
 * @who: whether we are addressing the "From" date or the "To" one.
 * @date:
 */
void
ofa_idate_filter_set_date( ofaIDateFilter *filter, gint who, const GDate *date )
{
	static const gchar *thisfn = "ofa_idate_filter_set_date";
	sIDateFilter *sdata;

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));

	sdata = get_idate_filter_data( filter );

	switch( who ){
		case IDATE_FILTER_FROM:
			my_date_editable_set_date( GTK_EDITABLE( sdata->from_entry ), date );
			break;
		case IDATE_FILTER_TO:
			my_date_editable_set_date( GTK_EDITABLE( sdata->to_entry ), date );
			break;
		default:
			g_warning( "%s: invalid date identifier: %d", thisfn, who );
	}
}

/**
 * ofa_idate_filter_is_valid:
 * @filter:
 * @who: whether we are addressing the "From" date or the "To" one.
 * @message: [out][allow-none]: will be set to an error message.
 *
 * Returns: %TRUE is the specified date is valid, taking into account
 * if it is mandatory or not.
 */
gboolean
ofa_idate_filter_is_valid( ofaIDateFilter *filter, gint who, gchar **message )
{
	static const gchar *thisfn = "ofa_idate_filter_is_valid";
	sIDateFilter *sdata;
	gboolean valid;
	GtkWidget *entry;
	GDate *date;
	gchar *str;

	g_return_val_if_fail( filter && OFA_IS_IDATE_FILTER( filter ), FALSE );

	sdata = get_idate_filter_data( filter );
	valid = FALSE;
	date = NULL;
	if( message ){
		*message = NULL;
	}

	switch( who ){
		case IDATE_FILTER_FROM:
			date = &sdata->from_date;
			entry = sdata->from_entry;
			break;
		case IDATE_FILTER_TO:
			date = &sdata->to_date;
			entry = sdata->to_entry;
			break;
		default:
			str = g_strdup_printf( "%s: invalid date identifier: %d", thisfn, who );
			g_warning( "%s", str );
			if( message ){
				*message = g_strdup( str );
			}
			g_free( str );
	}

	if( date ){
		valid = my_date_is_valid( date ) ||
				( !sdata->mandatory && my_date_editable_is_empty( GTK_EDITABLE( entry )));
	}

	if( !valid && message ){
		switch( who ){
			case IDATE_FILTER_FROM:
				*message = g_strdup( _( "'From' date is not valid" ));
				break;
			case IDATE_FILTER_TO:
				*message = g_strdup( _( "'To' date is not valid" ));
				break;
		}
	}

	return( valid );
}

/**
 * ofa_idate_filter_get_entry:
 * @filter:
 * @who:
 *
 * Returns: a pointer to the GtkWidget which is used as en antry for the date.
 */
GtkWidget *
ofa_idate_filter_get_entry( ofaIDateFilter *filter, gint who )
{
	static const gchar *thisfn = "ofa_idate_filter_get_entry";
	sIDateFilter *sdata;
	GtkWidget *entry;

	g_return_val_if_fail( filter && OFA_IS_IDATE_FILTER( filter ), NULL );

	sdata = get_idate_filter_data( filter );
	entry = NULL;

	switch( who ){
		case IDATE_FILTER_FROM:
			entry = sdata->from_entry;
			break;
		case IDATE_FILTER_TO:
			entry = sdata->to_entry;
			break;
		default:
			g_warning( "%s: invalid date identifier: %d", thisfn, who );
	}

	return( entry );
}

/**
 * ofa_idate_filter_get_frame_label:
 * @filter:
 *
 * Returns: a pointer to the GtkWidget which is used as the frame label.
 */
GtkWidget *
ofa_idate_filter_get_frame_label( ofaIDateFilter *filter )
{
	g_return_val_if_fail( filter && OFA_IS_IDATE_FILTER( filter ), NULL );

	return( my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "frame-label" ));
}

/**
 * ofa_idate_filter_get_prompt:
 * @filter:
 * @who:
 *
 * Returns: a pointer to the GtkWidget which is used as the "From" prompt.
 */
GtkWidget *
ofa_idate_filter_get_prompt( ofaIDateFilter *filter, gint who )
{
	static const gchar *thisfn = "ofa_idate_filter_get_prompt";
	const gchar *cstr;

	g_return_val_if_fail( filter && OFA_IS_IDATE_FILTER( filter ), NULL );
	cstr = NULL;

	switch( who ){
		case IDATE_FILTER_FROM:
			cstr = "from-prompt";
			break;
		case IDATE_FILTER_TO:
			cstr = "to-prompt";
			break;
		default:
			g_warning( "%s: invalid date identifier: %d", thisfn, who );
	}

	return( cstr ? my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), cstr ) : NULL );
}

/*
 * settings are: from(s); to(s); as SQL-formatted dates
 */
static void
read_settings( ofaIDateFilter *filter, sIDateFilter *sdata )
{
	myISettings *settings;
	GList *strlist, *it;
	const gchar *cstr;

	settings = ofa_hub_get_user_settings( sdata->hub );
	strlist = my_isettings_get_string_list( settings, HUB_USER_SETTINGS_GROUP, sdata->settings_key );

	it = strlist;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_sql( &sdata->from_date, cstr );
		my_date_editable_set_date( GTK_EDITABLE( sdata->from_entry ), &sdata->from_date );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		my_date_set_from_sql( &sdata->to_date, cstr );
		my_date_editable_set_date( GTK_EDITABLE( sdata->to_entry ), &sdata->to_date );
	}

	my_isettings_free_string_list( settings, strlist );
}

static void
write_settings( ofaIDateFilter *filter, sIDateFilter *sdata )
{
	myISettings *settings;
	gchar *sfrom, *sto, *str;

	if( my_strlen( sdata->settings_key )){

		settings = ofa_hub_get_user_settings( sdata->hub );

		sfrom = my_date_to_str( &sdata->from_date, MY_DATE_SQL );
		sto = my_date_to_str( &sdata->to_date, MY_DATE_SQL );

		str = g_strdup_printf( "%s;%s;", sfrom, sto );

		my_isettings_set_string( settings, HUB_USER_SETTINGS_GROUP, sdata->settings_key, str );

		g_free( str );
		g_free( sfrom );
		g_free( sto );
	}
}
