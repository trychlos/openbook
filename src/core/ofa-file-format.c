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
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-file-format.h"
#include "api/ofa-settings.h"

/* private instance data
 */
struct _ofaFileFormatPrivate {
	gboolean     dispose_has_run;

	/* if serialized in user preferences
	 */
	gchar       *prefs_name;

	/* runtime data
	 */
	gchar       *name;					/* may be %NULL if defaults */
	ofaFFtype    type;
	ofaFFmode    mode;
	gchar       *charmap;
	myDateFormat date_format;
	gchar        decimal_sep;
	gchar        field_sep;				/* csv only */
	union {
		gboolean with_headers;
		gint     count_headers;
	} h;
};

typedef struct {
	ofaFFtype      format;
	const gchar *label;
}
	sFormat;

static const sFormat st_file_format[] = {
		{ OFA_FFTYPE_CSV,   N_( "CSV-like file format" )},
		{ OFA_FFTYPE_FIXED, N_( "Fixed file format" )},
		{ OFA_FFTYPE_OTHER, N_( "Other (plugin-managed) format" )},
		{ 0 }
};

static gint         st_def_format       = OFA_FFTYPE_CSV;
static gint         st_def_mode         = OFA_FFMODE_EXPORT;
static const gchar *st_def_charmap      = "UTF-8";
static const gint   st_def_date         = MY_DATE_SQL;
static const gchar *st_def_decimal      = ".";
static const gchar *st_def_field_sep    = ";";
static const gchar *st_def_headers      = "True";

G_DEFINE_TYPE( ofaFileFormat, ofa_file_format, G_TYPE_OBJECT )

static void  do_init( ofaFileFormat *self, const gchar *prefs_name );

static void
file_format_finalize( GObject *instance )
{
	ofaFileFormatPrivate *priv;

	static const gchar *thisfn = "ofa_file_format_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_FILE_FORMAT( instance ));

	/* free data members here */
	priv = OFA_FILE_FORMAT( instance )->priv;

	g_free( priv->prefs_name );
	g_free( priv->charmap );
	g_free( priv->name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_format_parent_class )->finalize( instance );
}

static void
file_format_dispose( GObject *instance )
{
	ofaFileFormatPrivate *priv;

	g_return_if_fail( instance && OFA_IS_FILE_FORMAT( instance ));

	priv = OFA_FILE_FORMAT( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_file_format_parent_class )->dispose( instance );
}

static void
ofa_file_format_init( ofaFileFormat *self )
{
	static const gchar *thisfn = "ofa_file_format_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_FILE_FORMAT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_FILE_FORMAT, ofaFileFormatPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_file_format_class_init( ofaFileFormatClass *klass )
{
	static const gchar *thisfn = "ofa_file_format_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = file_format_dispose;
	G_OBJECT_CLASS( klass )->finalize = file_format_finalize;

	g_type_class_add_private( klass, sizeof( ofaFileFormatPrivate ));
}

/**
 * ofa_file_format_new:
 * @prefs_name: [allow-none]: the name of the settings key which is
 *  used to serialized this file format. If set, the object will be
 *  initialized from settings. If %NULL, the general defaults will
 *  be set.
 * @mode: whether the file format targets export or import.
 *
 * Returns: a newly allocated #ofaFileFormat object.
 */
ofaFileFormat *
ofa_file_format_new( const gchar *prefs_name )
{
	ofaFileFormat *self;

	self = g_object_new( OFA_TYPE_FILE_FORMAT, NULL );

	do_init( self, prefs_name );

	return( self );
}

/*
 * read file format from user settings:
 *
 * export: name;type;mode;charmap;date_format;decimal_sep;field_sep;with_headers
 * import: name;type;mode;charmap;date_format;decimal_sep;field_sep;count_headers
 */
static void
do_init( ofaFileFormat *self, const gchar *prefs_name )
{
	static const gchar *thisfn = "ofa_file_format_do_init";
	ofaFileFormatPrivate *priv;
	GList *prefs_list, *it;
	const gchar *cstr;
	gchar *text;

	priv = self->priv;

	priv->prefs_name = g_strdup( prefs_name );
	prefs_list = ofa_settings_user_get_string_list( prefs_name );

	/* name of this file format */
	it = prefs_list;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: name='%s'", cstr );*/
	priv->name = ( gchar * )( cstr ? g_strdup( cstr ) : cstr );

	/* file format type */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: ffmt='%s'", cstr );*/
	text = cstr ? g_strdup( cstr ) : g_strdup_printf( "%d", st_def_format );
	priv->type = atoi( text );
	g_free( text );

	/* target mode */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	text = cstr ? g_strdup( cstr ) : g_strdup_printf( "%d", st_def_mode );
	priv->mode = atoi( text );
	g_debug( "%s: prefs=%s, type=%d, mode=%d", thisfn, prefs_name, priv->type, priv->mode );
	g_free( text );
	if( priv->mode != OFA_FFMODE_EXPORT && priv->mode != OFA_FFMODE_IMPORT ){
		priv->mode = OFA_FFMODE_EXPORT;
	}

	/* charmap */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : st_def_charmap;
	/*g_debug( "do_init: charmap='%s'", cstr );*/
	priv->charmap = g_strdup( cstr );

	/* date format */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: date='%s'", cstr );*/
	text = cstr ? g_strdup( cstr ) : g_strdup_printf( "%d", st_def_date );
	priv->date_format = atoi( text );
	g_free( text );

	/* decimal separator */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: decimal_sep='%s'", cstr );*/
	text = g_strdup( cstr ? cstr : st_def_decimal);
	priv->decimal_sep = atoi( text );
	g_free( text );

	/* field separator */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: field_sep='%s'", cstr );*/
	text = g_strdup( cstr ? cstr : st_def_field_sep );
	priv->field_sep = atoi( text );
	g_free( text );

	/* with headers */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : st_def_headers;
	/*g_debug( "do_init: headers='%s'", cstr );*/
	if( priv->mode == OFA_FFMODE_EXPORT ){
		priv->h.with_headers = g_utf8_collate( cstr, "True" ) == 0;
	} else {
		priv->h.count_headers = atoi( cstr );
	}

	ofa_settings_free_string_list( prefs_list );
}

/**
 * ofa_file_format_get_ffmode:
 */
ofaFFmode
ofa_file_format_get_ffmode( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->mode );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_file_format_get_fftype:
 */
ofaFFtype
ofa_file_format_get_fftype( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->type );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_file_format_get_fftype_str:
 */
const gchar *
ofa_file_format_get_fftype_str( ofaFFtype format )
{
	static const gchar *thisfn = "ofa_file_format_get_fftype_str";
	gint i;

	for( i=0 ; st_file_format[i].format ; ++i ){
		if( st_file_format[i].format == format ){
			return( gettext( st_file_format[i].label ));
		}
	}

	/* only a debug message as the overflow of the previous loop is
	 * used when enumerating valid file formats */
	g_debug( "%s: unknown file format: %d (may be normal)", thisfn, format );
	return( NULL );
}

/**
 * ofa_file_format_get_charmap:
 */
const gchar *
ofa_file_format_get_charmap( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), NULL );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return(( const gchar * ) priv->charmap );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofa_file_format_get_date_format:
 */
myDateFormat
ofa_file_format_get_date_format( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), -1 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->date_format );
	}

	g_return_val_if_reached( -1 );
	return( -1 );
}

/**
 * ofa_file_format_get_decimal_sep:
 */
gchar
ofa_file_format_get_decimal_sep( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->decimal_sep );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_file_format_get_field_sep:
 */
gchar
ofa_file_format_get_field_sep( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->field_sep );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_file_format_get_headers_count:
 */
gint
ofa_file_format_get_headers_count( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->h.count_headers );
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

/**
 * ofa_file_format_has_headers:
 */
gboolean
ofa_file_format_has_headers( const ofaFileFormat *settings )
{
	ofaFileFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->h.with_headers );
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

/**
 * ofa_file_format_set:
 * @settings:
 * @name:
 * @type:
 * @mode
 * @charmap:
 * @date_format:
 * @decimal_sep:
 * @field_sep:
 * @with_headers:
 */
void
ofa_file_format_set( ofaFileFormat *settings,
								const gchar *name,
								ofaFFtype type,
								ofaFFmode mode,
								const gchar *charmap,
								myDateFormat date_format,
								gchar decimal_sep,
								gchar field_sep,
								gint count_headers )
{
	ofaFileFormatPrivate *priv;
	GList *prefs_list;
	gchar *sfile, *sdate, *sdecimal, *sfield, *sheaders;

	g_return_if_fail( settings && OFA_IS_FILE_FORMAT( settings ));

	priv = settings->priv;
	prefs_list = NULL;

	if( !priv->dispose_has_run ){

		/* name */
		g_free( priv->name );
		priv->name = g_strdup( name );
		prefs_list = g_list_append( prefs_list, ( gpointer )( name ? name : "" ));

		/* file format */
		priv->type = type;
		sfile = g_strdup_printf( "%d", type );
		prefs_list = g_list_append( prefs_list, sfile );

		/* mode */
		priv->mode = mode;
		sfile = g_strdup_printf( "%d", mode );
		prefs_list = g_list_append( prefs_list, sfile );

		/* charmap (may be %NULL) */
		g_free( priv->charmap );
		priv->charmap = g_strdup( charmap );
		prefs_list = g_list_append( prefs_list, ( gpointer ) charmap );

		/* date format  */
		priv->date_format = date_format;
		sdate = g_strdup_printf( "%d", date_format );
		prefs_list = g_list_append( prefs_list, sdate );

		/* decimal separator */
		priv->decimal_sep = decimal_sep;
		sdecimal = g_strdup_printf( "%d", decimal_sep );
		prefs_list = g_list_append( prefs_list, sdecimal );

		/* field separator */
		priv->field_sep = field_sep;
		sfield = g_strdup_printf( "%d", field_sep );
		prefs_list = g_list_append( prefs_list, sfield );

		/* with headers */
		if( priv->mode == OFA_FFMODE_EXPORT ){
			priv->h.with_headers = count_headers > 0;
			sheaders = g_strdup_printf( "%s", priv->h.with_headers ? "True":"False" );
		} else {
			priv->h.count_headers = count_headers;
			sheaders = g_strdup_printf( "%d", priv->h.count_headers );
		}
		prefs_list = g_list_append( prefs_list, sheaders );

		/* save in user preferences */
		g_debug( "ofa_file_format_set: prefs_name=%s", priv->prefs_name );
		if( my_strlen( priv->prefs_name )){
			ofa_settings_user_set_string_list( priv->prefs_name, prefs_list );
		}

		g_list_free( prefs_list );

		g_free( sheaders );
		g_free( sfield );
		g_free( sdecimal );
		g_free( sdate );
		g_free( sfile );
	}
}
