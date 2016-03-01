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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-plugin.h"
#include "api/ofo-base.h"

/* data set against the imported object
 */
typedef struct {

	/* initialization
	 */
	void                *caller;

	/* runtime data
	 */
	guint                count;			/* total count of lines to be imported */
	guint                progress;		/* import progression */
	guint                insert;		/* insert progression */

	/* when importing via a plugin
	 */
	gchar               *uri;
	const ofaFileFormat *settings;
	void                *ref;
}
	sIImportable;

#define IIMPORTABLE_LAST_VERSION        1
#define IIMPORTABLE_DATA                "ofa-iimportable-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType         register_type( void );
static void          interface_base_init( ofaIImportableInterface *klass );
static void          interface_base_finalize( ofaIImportableInterface *klass );
static gboolean      iimportable_is_willing_to( ofaIImportable *importable, const gchar *uri, const ofaFileFormat *settings );
static void          render_progress( ofaIImportable *importable, sIImportable *sdata, guint number, guint phase );
static sIImportable *get_iimportable_data( ofaIImportable *importable );
static void          on_importable_finalized( sIImportable *sdata, GObject *finalized_object );

/**
 * ofa_iimportable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iimportable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iimportable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iimportable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIImportableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIImportable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIImportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iimportable_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;

	if( st_initializations == 1 ){

		/* declare here the default implementations */
	}
}

static void
interface_base_finalize( ofaIImportableInterface *klass )
{
	static const gchar *thisfn = "ofa_iimportable_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iimportable_get_interface_last_version:
 * @instance: this #ofaIImportable instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iimportable_get_interface_last_version( void )
{
	return( IIMPORTABLE_LAST_VERSION );
}

/**
 * ofa_iimportable_find_willing_to:
 * @uri:
 * @settings:
 *
 * Returns: a new reference on a module willing to import the specified
 * @uri. The returned reference should be g_object_unref() by the caller
 * after import.
 */
ofaIImportable *
ofa_iimportable_find_willing_to( const gchar *uri, const ofaFileFormat *settings )
{
	static const gchar *thisfn = "ofa_iimportable_find_willing_to";
	GList *modules, *it;
	ofaIImportable *found;

	found = NULL;
	modules = ofa_plugin_get_extensions_for_type( OFA_TYPE_IIMPORTABLE );
	g_debug( "%s: uri=%s, settings=%p, modules=%p, count=%d",
			thisfn, uri, ( void * ) settings, ( void * ) modules, g_list_length( modules ));

	for( it=modules ; it ; it=it->next ){
		if( iimportable_is_willing_to( OFA_IIMPORTABLE( it->data ), uri, settings )){
			found = g_object_ref( OFA_IIMPORTABLE( it->data ));
			break;
		}
	}

	ofa_plugin_free_extensions( modules );

	return( found );
}

/*
 * ofa_iimportable_is_willing_to:
 * @importable: this #ofaIImportable instance.
 * @uri: the URI to be imported.
 * @settings: an #ofaFileFormat object.
 *
 * Returns: %TRUE if the provider is willing to import the file.
 */
static gboolean
iimportable_is_willing_to( ofaIImportable *importable, const gchar *uri, const ofaFileFormat *settings )
{
	static const gchar *thisfn = "ofa_iimportable_is_willing_to";
	sIImportable *sdata;
	gboolean ok;

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), FALSE );
	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), FALSE );

	sdata = get_iimportable_data( importable );
	g_return_val_if_fail( sdata, FALSE );

	ok = FALSE;
	sdata->uri = g_strdup( uri );
	sdata->settings = settings;

	if( OFA_IIMPORTABLE_GET_INTERFACE( importable )->is_willing_to ){
		ok = OFA_IIMPORTABLE_GET_INTERFACE( importable )->is_willing_to(
					importable, sdata->uri, sdata->settings, &sdata->ref, &sdata->count );
	}

	g_debug( "%s: importable=%p (%s), ok=%s, count=%u",
			thisfn,
			( void * ) importable, G_OBJECT_TYPE_NAME( importable ), ok ? "True":"False", sdata->count );

	return( ok );
}

/**
 * ofa_iimportable_import:
 * @importable: this #ofaIImportable instance.
 * @lines: the content of the imported file as a #GSList list of
 *  #GSList fields.
 * @settings: an #ofaFileFormat object.
 * @dossier: the current dossier.
 * @caller: the caller instance.
 *
 * Import the dataset from the named file.
 *
 * Returns: the count of found errors.
 */
gint
ofa_iimportable_import( ofaIImportable *importable,
									GSList *lines, const ofaFileFormat *settings,
									ofaHub *hub, void *caller )
{
	static const gchar *thisfn = "ofa_iimportable_import";
	sIImportable *sdata;
	gint errors;
	GTimeVal stamp_start, stamp_end;
	gchar *sstart, *send;
	guint count;
	gulong udelay;
	GSList *content;

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), 0 );
	g_return_val_if_fail( OFO_IS_BASE( importable ), 0 );
	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	my_utils_stamp_set_now( &stamp_start );
	sdata = get_iimportable_data( importable );
	g_return_val_if_fail( sdata, 0 );

	count = ofa_file_format_get_headers_count( settings );
	content = g_slist_nth( lines, count );
	g_debug( "%s: count=%d, lines=%p (%d), content=%p (%d)",
			thisfn, count, ( void * ) lines, g_slist_length( lines ), ( void * ) content, g_slist_length( content ));

	errors = 0;
	sdata->caller = caller;
	sdata->progress = 0;
	sdata->insert = 0;
	sdata->count = g_slist_length( content );

	if( OFA_IIMPORTABLE_GET_INTERFACE( importable )->import ){
		errors = OFA_IIMPORTABLE_GET_INTERFACE( importable )->import( importable, content, settings, hub );
	}

	my_utils_stamp_set_now( &stamp_end );

	sstart = my_utils_stamp_to_str( &stamp_start, MY_STAMP_YYMDHMS );
	send = my_utils_stamp_to_str( &stamp_end, MY_STAMP_YYMDHMS );
	count = g_slist_length( lines );
	udelay = 1000000*(stamp_end.tv_sec-stamp_start.tv_sec)+stamp_end.tv_usec-stamp_start.tv_usec;

	g_debug( "%s: stamp_start=%s, stamp_end=%s, count=%u: average is %'.5lf s",
			thisfn, sstart, send, count, ( gdouble ) udelay / 1000000.0 / ( gdouble ) count );

	g_free( sstart );
	g_free( send );

	return( errors );
}

/**
 * ofa_iimportable_get_string:
 *
 * Returns a newly allocated string pointed to by the @it.
 * If not null, the string is stripped from leading and tailing spaces and double quotes.
 * The returned string should be g_free() by the caller.
 *
 * Returns NULL if empty.
 */
gchar *
ofa_iimportable_get_string( GSList **it, const ofaFileFormat *settings )
{
	const gchar *cstr;
	gchar *str1, *str2, *regexp;
	glong len;

	str2 = NULL;
	cstr = *it ? ( const gchar * )(( *it )->data ) : NULL;
	if( cstr ){
		str1 = NULL;
		len = my_strlen( cstr );
		if( len ){
			if( g_str_has_prefix( cstr, "\"" ) && g_str_has_suffix( cstr, "\"" )){
				str1 = g_utf8_substring( cstr, 1, len-1 );
			} else {
				str1 = g_strstrip( g_strdup( cstr ));
			}
			regexp = g_strdup_printf( "(\\\")|(\\\n)|(\\\r)|(\\%c)", ofa_file_format_get_field_sep( settings ));
			str2 = my_utils_unquote_regexp( str1, regexp );
			if( !my_strlen( str2 )){
				g_free( str2 );
				str2 = NULL;
			}
		}
		if( 1 ){
			g_debug( "src='%s', temp='%s', out='%s'", cstr, str1, str2 );
		}
		g_free( str1 );
	}
	*it = *it ? ( *it )->next : NULL;

	return( str2 );
}

/**
 * ofa_iimportable_pulse:
 * @importable: this #ofaIImportable instance.
 * @phase: whether this is the import or the insert phase
 *
 * Make the progress bar pulsing.
 */
void
ofa_iimportable_pulse( ofaIImportable *importable,
										ofeImportablePhase phase )
{
	sIImportable *sdata;

	g_return_if_fail( OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	if( sdata->caller ){
		g_signal_emit_by_name( sdata->caller, "pulse", phase );
	}
}

/**
 * ofa_iimportable_increment_progress:
 * @importable: this #ofaIImportable instance.
 * @phase: whether this is the import or the insert phase
 * @count: the count of lines to be added to total of imported lines.
 *
 * Increment the import/insert progress count.
 */
void
ofa_iimportable_increment_progress( ofaIImportable *importable,
										ofeImportablePhase phase, guint count )
{
	sIImportable *sdata;

	g_return_if_fail( OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	if( phase == IMPORTABLE_PHASE_IMPORT ){
		sdata->progress += count;
		render_progress( importable, sdata, sdata->progress, phase );

	} else {
		g_return_if_fail( phase == IMPORTABLE_PHASE_INSERT );
		sdata->insert += count;
		render_progress( importable, sdata, sdata->insert, phase );
	}
}

static void
render_progress( ofaIImportable *importable, sIImportable *sdata, guint number, guint phase )
{
	gdouble progress;
	gchar *text;

	if( OFA_IS_IIMPORTER( sdata->caller )){
		progress = ( gdouble ) number / ( gdouble ) sdata->count;
		text = g_strdup_printf( "%u/%u", number, sdata->count );
		g_signal_emit_by_name( sdata->caller, "progress", phase, progress, text );
		g_free( text );
	}
}

/**
 * ofa_iimportable_set_message:
 * @importable: this #ofaIImportable instance.
 * @line: the line count, counted from 1.
 * @status: whether this is a standard, warning or error message
 * @msg: the message.
 *
 * Send the message to the caller.
 *
 * This function should be called by the implementation each time a
 * message is to be displayed.
 */
void
ofa_iimportable_set_message( ofaIImportable *importable,
										guint line_number, ofeImportableMsg status, const gchar *msg )
{
	sIImportable *sdata;

	g_return_if_fail( OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	if( OFA_IS_IIMPORTER( sdata->caller )){
		g_signal_emit_by_name( sdata->caller, "message", line_number, status, msg );
	}
}

static sIImportable *
get_iimportable_data( ofaIImportable *importable )
{
	sIImportable *sdata;

	sdata = ( sIImportable * ) g_object_get_data( G_OBJECT( importable ), IIMPORTABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sIImportable, 1 );
		g_object_set_data( G_OBJECT( importable ), IIMPORTABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( importable ), ( GWeakNotify ) on_importable_finalized, sdata );
	}

	return( sdata );
}

static void
on_importable_finalized( sIImportable *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_iimportable_on_importable_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata->uri );
	g_free( sdata );
}

/**
 * ofa_iimportable_import_uri:
 * @importable: this #ofaIImportable instance.
 * @dossier: the current dossier.
 * @caller: [allow-none]: the caller instance.
 * @imported_id: [allow-none][out]: if non %NULL, then set to a newly
 *  allocated identifier of the imported object which should be g_free()
 *  by the caller.
 *
 * Import the specified @uri.
 *
 * Returns: the count of errors.
 */
guint
ofa_iimportable_import_uri( ofaIImportable *importable,
								ofaHub *hub, void *caller, ofxCounter *imported_id )
{
	static const gchar *thisfn = "ofa_iimportable_import_uri";
	sIImportable *sdata;
	gint errors;

	g_debug( "%s: importable=%p, hub=%p, caller=%p",
			thisfn, ( void * ) importable, ( void * ) hub, ( void * ) caller );

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), 0 );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );

	sdata = get_iimportable_data( importable );
	g_return_val_if_fail( sdata, 0 );

	errors = 0;
	sdata->caller = caller;
	sdata->progress = 0;
	sdata->insert = 0;

	if( OFA_IIMPORTABLE_GET_INTERFACE( importable )->import_uri ){
		errors = OFA_IIMPORTABLE_GET_INTERFACE( importable )->import_uri(
						importable, sdata->ref, sdata->uri, sdata->settings, hub, imported_id );
	}

	return( errors );
}

/**
 * ofa_iimportable_get_count:
 * @importable: this #ofaIImportable instance.
 *
 * Returns: the count of imported lines.
 */
guint
ofa_iimportable_get_count( ofaIImportable *importable )
{
	static const gchar *thisfn = "ofa_iimportable_get_count";
	sIImportable *sdata;

	g_debug( "%s: importable=%p", thisfn, ( void * ) importable );

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), 0 );

	sdata = get_iimportable_data( importable );
	g_return_val_if_fail( sdata, 0 );

	return( sdata->insert );
}

/**
 * ofa_iimportable_set_count:
 * @importable: this #ofaIImportable instance.
 *
 * Set the new count of lines.
 */
void
ofa_iimportable_set_count( ofaIImportable *importable, guint count )
{
	sIImportable *sdata;

	g_return_if_fail( importable && OFA_IS_IIMPORTABLE( importable ));

	sdata = get_iimportable_data( importable );
	g_return_if_fail( sdata );

	sdata->count = count;
}
