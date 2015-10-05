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
#include <gtk/gtk.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-idate-filter.h"

/* data associated to each implementor object
 */
typedef struct {
	gchar        *xml_name;
	gboolean      mandatory;
	gchar        *prefs_key;
	GtkSizeGroup *group0;

	GtkWidget    *from_entry;
	GDate         from_date;

	GtkWidget    *to_entry;
	GDate         to_date;
}
	sIDateFilter;

#define IDATE_FILTER_LAST_VERSION      1
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

static GType          register_type( void );
static void           interface_base_init( ofaIDateFilterInterface *klass );
static void           interface_base_finalize( ofaIDateFilterInterface *klass );
static sIDateFilter *get_idate_filter_data( ofaIDateFilter *filter );
static void           on_widget_finalized( ofaIDateFilter *idate_filter, void *finalized_widget );
static void           setup_composite( ofaIDateFilter *filter, sIDateFilter *sdata );
static void           on_from_changed( GtkEntry *entry, ofaIDateFilter *filter );
static gboolean       on_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaIDateFilter *filter );
static void           on_to_changed( GtkEntry *entry, ofaIDateFilter *filter );
static gboolean       on_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaIDateFilter *filter );
static void           on_date_changed( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date );
static gboolean       on_date_focus_out( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date, sIDateFilter *sdata );
static void           load_settings( ofaIDateFilter *filter, sIDateFilter *sdata );
static void           set_settings( ofaIDateFilter *filter, sIDateFilter *sdata );

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
ofa_idate_filter_get_interface_last_version( const ofaIDateFilter *instance )
{
	return( IDATE_FILTER_LAST_VERSION );
}

/**
 * ofa_idate_filter_setup_bin:
 * @filter: this #ofaIDateFilter instance.
 *
 * Initialize the composite widget which implements this interface.
 */
void
ofa_idate_filter_setup_bin( ofaIDateFilter *filter, const gchar *xml_name )
{
	static const gchar *thisfn = "ofa_idate_filter_setup_bin";
	sIDateFilter *sdata;

	g_debug( "%s: filter=%p, xml_name=%s", thisfn, ( void * ) filter, xml_name );

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));
	g_return_if_fail( G_IS_OBJECT( filter ));

	sdata = get_idate_filter_data( filter );
	sdata->xml_name = g_strdup( xml_name );
	sdata->mandatory = DEFAULT_MANDATORY;

	setup_composite( filter, sdata );
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
	g_free( sdata->xml_name );
	g_free( sdata->prefs_key );
	g_free( sdata );

	g_object_set_data( G_OBJECT( filter ), IDATE_FILTER_DATA, NULL );
}

static void
setup_composite( ofaIDateFilter *filter, sIDateFilter *sdata )
{
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel, *entry, *label;

	builder = gtk_builder_new_from_file( sdata->xml_name );

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

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), sdata->mandatory );

	g_signal_connect( entry, "changed", G_CALLBACK( on_from_changed ), filter );
	g_signal_connect( entry, "focus-out-event", G_CALLBACK( on_from_focus_out ), filter );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "from-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

	/* To: block */
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	sdata->to_entry = entry;

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), ofa_prefs_date_display());
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, ofa_prefs_date_check());
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), sdata->mandatory );

	g_signal_connect( entry, "changed", G_CALLBACK( on_to_changed ), filter );
	g_signal_connect( entry, "focus-out-event", G_CALLBACK( on_to_focus_out ), filter );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( filter ), "to-prompt" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), entry );

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
	}
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

	my_date_set_from_date( date, my_editable_date_get_date( GTK_EDITABLE( entry ), NULL ));
	empty = my_editable_date_is_empty( GTK_EDITABLE( entry ));
	valid = my_date_is_valid( date );

	g_signal_emit_by_name( filter, "ofa-changed", who, empty, valid );
}

/*
 * only record the date in settings if it is valid
 */
static gboolean
on_date_focus_out( ofaIDateFilter *filter, gint who, GtkEntry *entry, GDate *date, sIDateFilter *sdata )
{
	my_date_set_from_date( date, my_editable_date_get_date( GTK_EDITABLE( entry ), NULL ));

	if( my_date_is_valid( date ) ||
			( my_editable_date_is_empty( GTK_EDITABLE( entry )) && !sdata->mandatory )){

		set_settings( filter, sdata );
	}

	g_signal_emit_by_name( filter, "ofa-focus-out", who, date );

	return( FALSE );
}

/**
 * ofa_idate_filter_set_prefs:
 * @filter:
 * @prefs_key: the settings key where date are stored as a string list
 *  of SQL-formatted dates.
 *
 * Load the settings from user preferences.
 */
void
ofa_idate_filter_set_prefs( ofaIDateFilter *filter, const gchar *prefs_key )
{
	sIDateFilter *sdata;

	g_return_if_fail( filter && OFA_IS_IDATE_FILTER( filter ));

	sdata = get_idate_filter_data( filter );

	g_free( sdata->prefs_key );
	sdata->prefs_key = g_strdup( prefs_key );

	load_settings( filter, sdata );
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
			my_editable_date_set_date( GTK_EDITABLE( sdata->from_entry ), date );
			break;
		case IDATE_FILTER_TO:
			my_editable_date_set_date( GTK_EDITABLE( sdata->to_entry ), date );
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
				( !sdata->mandatory && my_editable_date_is_empty( GTK_EDITABLE( entry )));
	}

	if( !valid && message ){
		switch( who ){
			case IDATE_FILTER_FROM:
				*message = g_strdup( _( "From date is not valid" ));
				break;
			case IDATE_FILTER_TO:
				*message = g_strdup( _( "To date is not valid" ));
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
 * settings are: from;to; as SQL-formatted dates
 */
static void
load_settings( ofaIDateFilter *filter, sIDateFilter *sdata )
{
	GList *slist, *it;
	const gchar *cstr;

	slist = ofa_settings_get_string_list( sdata->prefs_key );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			my_editable_date_set_date( GTK_EDITABLE( sdata->from_entry ), &sdata->from_date );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			my_date_set_from_sql( &sdata->to_date, cstr );
			my_editable_date_set_date( GTK_EDITABLE( sdata->to_entry ), &sdata->to_date );
		}

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaIDateFilter *filter, sIDateFilter *sdata )
{
	gchar *sfrom, *sto, *str;

	if( my_strlen( sdata->prefs_key )){

		sfrom = my_date_to_str( &sdata->from_date, MY_DATE_SQL );
		sto = my_date_to_str( &sdata->to_date, MY_DATE_SQL );

		str = g_strdup_printf( "%s;%s;", sfrom, sto );

		ofa_settings_set_string( sdata->prefs_key, str );

		g_free( str );
		g_free( sfrom );
		g_free( sto );
	}
}