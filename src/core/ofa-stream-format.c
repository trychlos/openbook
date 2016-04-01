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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-settings.h"
#include "api/ofa-stream-format.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* when serialized in user preferences
	 */
	gchar       *name;
	ofeSFMode    mode;
	gint         indicators;

	/* runtime data
	 */
	gchar       *charmap;
	myDateFormat date_format;
	gchar        thousand_sep;
	gchar        decimal_sep;
	gchar        field_sep;
	gchar        string_delim;
	union {
		gboolean with_headers;
		gint     count_headers;
	} h;
}
	ofaStreamFormatPrivate;

typedef struct {
	ofeSFMode    mode;
	const gchar *str;
	const gchar *localized;
}
	sLabels;

static sLabels st_labels[] = {
		{ OFA_SFMODE_EXPORT, "Export", N_( "Export" )},
		{ OFA_SFMODE_IMPORT, "Import", N_( "Import" )},
		{ 0 }
};

static const gchar *st_def_charmap      = "UTF-8";
static const gint   st_def_date         = MY_DATE_SQL;
static const gchar *st_def_thousand     = "";
static const gchar *st_def_decimal      = ".";
static const gchar *st_def_field_sep    = ";";
static const gchar *st_def_headers      = "True";
static const gchar *st_def_string_delim = "\"";

static void   do_init( ofaStreamFormat *self, const gchar *name, ofeSFMode mode );
static gchar *get_key_name( const gchar *name, ofeSFMode mode );

G_DEFINE_TYPE_EXTENDED( ofaStreamFormat, ofa_stream_format, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaStreamFormat ))

static void
stream_format_finalize( GObject *instance )
{
	ofaStreamFormatPrivate *priv;

	static const gchar *thisfn = "ofa_stream_format_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT( instance ));

	/* free data members here */
	priv = ofa_stream_format_get_instance_private( OFA_STREAM_FORMAT( instance ));

	g_free( priv->name );
	g_free( priv->charmap );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_stream_format_parent_class )->finalize( instance );
}

static void
stream_format_dispose( GObject *instance )
{
	ofaStreamFormatPrivate *priv;

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT( instance ));

	priv = ofa_stream_format_get_instance_private( OFA_STREAM_FORMAT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_stream_format_parent_class )->dispose( instance );
}

static void
ofa_stream_format_init( ofaStreamFormat *self )
{
	static const gchar *thisfn = "ofa_stream_format_init";
	ofaStreamFormatPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_STREAM_FORMAT( self ));

	priv = ofa_stream_format_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_stream_format_class_init( ofaStreamFormatClass *klass )
{
	static const gchar *thisfn = "ofa_stream_format_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = stream_format_dispose;
	G_OBJECT_CLASS( klass )->finalize = stream_format_finalize;
}

/**
 * ofa_stream_format_get_default_name:
 *
 * Returns: the default name.
 */
const gchar *
ofa_stream_format_get_default_name( void )
{
	return( "Default" );
}

/**
 * ofa_stream_format_get_default_mode:
 *
 * Returns: the default mode.
 */
ofeSFMode
ofa_stream_format_get_default_mode( void )
{
	return( OFA_SFMODE_EXPORT );
}

/**
 * ofa_stream_format_get_mode_str:
 *
 * Returns: the associated non-localized string.
 */
const gchar *
ofa_stream_format_get_mode_str( ofeSFMode mode )
{
	gint i;

	for( i=0 ; st_labels[i].mode ; ++i ){
		if( st_labels[i].mode == mode ){
			return( st_labels[i].str );
		}
	}

	return( ofa_stream_format_get_mode_str( ofa_stream_format_get_default_mode()));
}

/**
 * ofa_stream_format_get_mode_localestr:
 *
 * Returns: the associated ocalized string.
 */
const gchar *
ofa_stream_format_get_mode_localestr( ofeSFMode mode )
{
	gint i;

	for( i=0 ; st_labels[i].mode ; ++i ){
		if( st_labels[i].mode == mode ){
			return( gettext( st_labels[i].localized ));
		}
	}

	return( ofa_stream_format_get_mode_localestr( ofa_stream_format_get_default_mode()));
}

/**
 * ofa_stream_format_exists:
 * @name: the user-provided name for this format.
 * @mode: the target mode for this format.
 *
 * Returns: %TRUE if the @name-@mode format is already defined in user
 * settings.
 */
gboolean
ofa_stream_format_exists( const gchar *name, ofeSFMode mode )
{
	gboolean exists;
	gchar *keyname, *str;

	exists = FALSE;

	keyname = get_key_name( name, mode );
	if( my_strlen( keyname )){
		str = ofa_settings_user_get_string( keyname );
		exists = my_strlen( str ) > 0;
		g_free( str );
	}

	return( exists );
}

/**
 * ofa_stream_format_new:
 * @name: [allow-none]: the user-provided name for this format;
 *  defaults to 'Default'.
 * @mode: [allow-none]: the target mode for this format;
 *  defaults to Export
 *
 * Returns: a newly allocated #ofaStreamFormat object.
 */
ofaStreamFormat *
ofa_stream_format_new( const gchar *name, ofeSFMode mode )
{
	ofaStreamFormat *self;

	self = g_object_new( OFA_TYPE_STREAM_FORMAT, NULL );

	do_init( self, name, mode );

	return( self );
}

/*
 * read file format from user settings:
 *
 * export: indicators;charmap;date_format;thousand_sep;decimal_sep;field_sep;with_headers;string_delim;
 * import: indicators;charmap;date_format;thousand_sep;decimal_sep;field_sep;count_headers;string_delim;
 */
static void
do_init( ofaStreamFormat *self, const gchar *name, ofeSFMode mode )
{
	ofaStreamFormatPrivate *priv;
	gchar *keyname;
	GList *prefs_list, *it;
	const gchar *cstr;
	gchar *text;

	priv = ofa_stream_format_get_instance_private( self );

	priv->name = g_strdup( my_strlen( name ) ? name : ofa_stream_format_get_default_name());

	switch( mode ){
		case OFA_SFMODE_EXPORT:
		case OFA_SFMODE_IMPORT:
			priv->mode = mode;
			break;
		default:
			priv->mode = ofa_stream_format_get_default_mode();
			break;
	}

	keyname = get_key_name( priv->name, priv->mode );
	g_return_if_fail( my_strlen( keyname ));

	prefs_list = ofa_settings_user_get_string_list( keyname );

	/* indicators */
	it = prefs_list;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	priv->indicators = cstr ? atoi( cstr ) : OFA_SFHAS_ALL;

	/* charmap */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : st_def_charmap;
	/*g_debug( "do_init: charmap='%s'", cstr );*/
	g_free( priv->charmap );
	priv->charmap = g_strdup( cstr );

	/* date format */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: date='%s'", cstr );*/
	text = cstr ? g_strdup( cstr ) : g_strdup_printf( "%d", st_def_date );
	priv->date_format = atoi( text );
	g_free( text );

	/* thousand separator */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	text = g_strdup( cstr ? cstr : st_def_thousand );
	priv->thousand_sep = atoi( text );
	g_free( text );

	/* decimal separator */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	/*g_debug( "do_init: decimal_sep='%s'", cstr );*/
	text = g_strdup( cstr ? cstr : st_def_decimal );
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
	if( priv->mode == OFA_SFMODE_EXPORT ){
		priv->h.with_headers = g_utf8_collate( cstr, "True" ) == 0;
	} else {
		priv->h.count_headers = atoi( cstr );
	}

	/* string delimiter */
	it = it ? it->next : NULL;
	cstr = ( it && it->data ) ? ( const gchar * ) it->data : NULL;
	text = g_strdup( cstr ? cstr : st_def_string_delim );
	priv->string_delim = atoi( text );
	g_free( text );

	if( prefs_list ){
		ofa_settings_free_string_list( prefs_list );
	}

	g_free( keyname );
}

/**
 * ofa_stream_format_get_name:
 */
const gchar *
ofa_stream_format_get_name( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), NULL );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->name );
}

/**
 * ofa_stream_format_get_mode:
 */
ofeSFMode
ofa_stream_format_get_mode( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), 0 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->mode );
}

/**
 * ofa_stream_format_get_has_charmap:
 */
gboolean
ofa_stream_format_get_has_charmap( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_CHARMAP );
}

/**
 * ofa_stream_format_get_charmap:
 */
const gchar *
ofa_stream_format_get_charmap( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), NULL );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->charmap );
}

/**
 * ofa_stream_format_get_has_date:
 */
gboolean
ofa_stream_format_get_has_date( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_DATEFMT );
}

/**
 * ofa_stream_format_get_date_format:
 */
myDateFormat
ofa_stream_format_get_date_format( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), -1 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, -1 );

	return( priv->date_format );
}

/**
 * ofa_stream_format_get_has_thousand:
 */
gboolean
ofa_stream_format_get_has_thousand( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_THOUSANDSEP );
}

/**
 * ofa_stream_format_get_thousand_sep:
 */
gchar
ofa_stream_format_get_thousand_sep( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), 0 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->thousand_sep );
}

/**
 * ofa_stream_format_get_has_decimal:
 */
gboolean
ofa_stream_format_get_has_decimal( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_DECIMALSEP );
}

/**
 * ofa_stream_format_get_decimal_sep:
 */
gchar
ofa_stream_format_get_decimal_sep( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), 0 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->decimal_sep );
}

/**
 * ofa_stream_format_get_has_field:
 */
gboolean
ofa_stream_format_get_has_field( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_FIELDSEP );
}

/**
 * ofa_stream_format_get_field_sep:
 */
gchar
ofa_stream_format_get_field_sep( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), 0 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->field_sep );
}

/**
 * ofa_stream_format_get_has_strdelim:
 */
gboolean
ofa_stream_format_get_has_strdelim( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_STRDELIM );
}

/**
 * ofa_stream_format_get_string_delim:
 */
gchar
ofa_stream_format_get_string_delim( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), 0 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );

	return( priv->string_delim );
}

/**
 * ofa_stream_format_get_has_headers:
 */
gboolean
ofa_stream_format_get_has_headers( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->indicators & OFA_SFHAS_HEADERS );
}

/**
 * ofa_stream_format_get_with_headers:
 */
gboolean
ofa_stream_format_get_with_headers( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), FALSE );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );
	g_return_val_if_fail( priv->mode == OFA_SFMODE_EXPORT, FALSE );

	return( priv->h.with_headers );
}

/**
 * ofa_stream_format_get_headers_count:
 */
gint
ofa_stream_format_get_headers_count( const ofaStreamFormat *settings )
{
	ofaStreamFormatPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ), 0 );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_val_if_fail( !priv->dispose_has_run, 0 );
	g_return_val_if_fail( priv->mode == OFA_SFMODE_IMPORT, FALSE );

	return( priv->h.count_headers );
}

/**
 * ofa_stream_format_set:
 * @settings:
 * @charmap:
 * @date_format:
 * @thousand_sep:
 * @decimal_sep:
 * @field_sep:
 * @count_headers:
 *  is headers count on import,
 *  is with_headers if greater than zero on export.
 */
void
ofa_stream_format_set( ofaStreamFormat *settings,
								gboolean has_charmap, const gchar *charmap,
								gboolean has_datefmt, myDateFormat datefmt,
								gboolean has_thousand_sep, gchar thousand_sep,
								gboolean has_decimal_sep, gchar decimal_sep,
								gboolean has_field_sep, gchar field_sep,
								gboolean has_string_delim, gchar string_delim,
								gboolean has_headers, gint count_headers )
{
	static const gchar *thisfn = "ofa_stream_format_set";
	ofaStreamFormatPrivate *priv;
	GList *prefs_list;
	gchar *sdate, *sthousand, *sdecimal, *sfield, *sheaders, *sstrdelim;
	gchar *keyname, *sinds;

	g_return_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ));

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_if_fail( !priv->dispose_has_run );

	prefs_list = NULL;

	/* charmap (may be %NULL) */
	g_free( priv->charmap );
	priv->charmap = NULL;
	priv->indicators &= ~OFA_SFHAS_CHARMAP;
	if( has_charmap ){
		priv->charmap = g_strdup( charmap );
		priv->indicators |= OFA_SFHAS_CHARMAP;
	}
	prefs_list = g_list_append( prefs_list, ( gpointer ) charmap );

	/* date format  */
	priv->date_format = 0;
	priv->indicators &= ~OFA_SFHAS_DATEFMT;
	if( has_datefmt ){
		priv->date_format = datefmt;
		sdate = g_strdup_printf( "%d", datefmt );
		priv->indicators |= OFA_SFHAS_DATEFMT;
	} else {
		sdate = g_strdup( "" );
	}
	prefs_list = g_list_append( prefs_list, sdate );

	/* thousand separator */
	priv->thousand_sep = '\0';
	priv->indicators &= ~OFA_SFHAS_THOUSANDSEP;
	if( has_thousand_sep ){
		priv->thousand_sep = thousand_sep;
		priv->indicators |= OFA_SFHAS_THOUSANDSEP;
	}
	sthousand = g_strdup_printf( "%d", thousand_sep );
	prefs_list = g_list_append( prefs_list, sthousand );

	/* decimal separator */
	priv->decimal_sep = '\0';
	priv->indicators &= ~OFA_SFHAS_DECIMALSEP;
	if( has_decimal_sep ){
		priv->decimal_sep = decimal_sep;
		priv->indicators |= OFA_SFHAS_DECIMALSEP;
	}
	sdecimal = g_strdup_printf( "%d", decimal_sep );
	prefs_list = g_list_append( prefs_list, sdecimal );

	/* field separator */
	priv->field_sep = '\0';
	priv->indicators &= ~OFA_SFHAS_FIELDSEP;
	if( has_field_sep ){
		priv->field_sep = field_sep;
		priv->indicators |= OFA_SFHAS_FIELDSEP;
	}
	sfield = g_strdup_printf( "%d", field_sep );
	prefs_list = g_list_append( prefs_list, sfield );

	/* string delimiter */
	priv->string_delim = '\0';
	priv->indicators &= ~OFA_SFHAS_STRDELIM;
	if( has_string_delim ){
		priv->string_delim = string_delim;
		priv->indicators |= OFA_SFHAS_STRDELIM;
	}
	sstrdelim = g_strdup_printf( "%d", string_delim );
	prefs_list = g_list_append( prefs_list, sstrdelim );

	/* with headers */
	priv->h.count_headers = 0;
	priv->indicators &= ~OFA_SFHAS_HEADERS;
	if( has_headers ){
		if( priv->mode == OFA_SFMODE_EXPORT ){
			priv->h.with_headers = count_headers > 0;
			sheaders = g_strdup_printf( "%s", priv->h.with_headers ? "True":"False" );
		} else {
			priv->h.count_headers = count_headers;
			sheaders = g_strdup_printf( "%d", priv->h.count_headers );
		}
		priv->indicators |= OFA_SFHAS_HEADERS;
	} else {
		sheaders = g_strdup( "" );
	}
	prefs_list = g_list_append( prefs_list, sheaders );

	/* prefix with indicators */
	sinds = g_strdup_printf( "%d", priv->indicators );
	prefs_list = g_list_prepend( prefs_list, sinds );

	/* save in user preferences */
	keyname = get_key_name( priv->name, priv->mode );
	g_debug( "%s: keyname=%s", thisfn, keyname );
	g_return_if_fail( my_strlen( keyname ));
	ofa_settings_user_set_string_list( keyname, prefs_list );
	g_free( keyname );

	g_list_free( prefs_list );
	g_free( sinds );
	g_free( sstrdelim );
	g_free( sheaders );
	g_free( sfield );
	g_free( sdecimal );
	g_free( sthousand );
	g_free( sdate );
}

/**
 * ofa_stream_format_set_name:
 * @settings:
 * @name:
 *
 * Change the name of the settings.
 * This will so also change the key in user settings.
 *
 * This let us read a default user preference, and then write to a new
 * (hopefully more specific) user preference.
 */
void
ofa_stream_format_set_name( ofaStreamFormat *settings, const gchar *name )
{
	ofaStreamFormatPrivate *priv;

	g_return_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ));
	g_return_if_fail( my_strlen( name ));

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->name );
	priv->name = g_strdup( name );
}

/**
 * ofa_stream_format_set_mode:
 * @settings:
 * @mode:
 *
 * Set the import/export mode.
 */
void
ofa_stream_format_set_mode( ofaStreamFormat *settings, ofeSFMode mode )
{
	ofaStreamFormatPrivate *priv;

	g_return_if_fail( settings && OFA_IS_STREAM_FORMAT( settings ));
	g_return_if_fail( mode == OFA_SFMODE_EXPORT || mode == OFA_SFMODE_IMPORT );

	priv = ofa_stream_format_get_instance_private( settings );

	g_return_if_fail( !priv->dispose_has_run );

	priv->mode = mode;
}

static gchar *
get_key_name( const gchar *name, ofeSFMode mode )
{
	gchar *keyname;

	keyname = NULL;

	if( my_strlen( name ) &&
			( mode == OFA_SFMODE_EXPORT || mode == OFA_SFMODE_IMPORT )){
		keyname = g_strdup_printf( "%s-%s-format", name, ofa_stream_format_get_mode_str( mode ));
	}

	return( keyname );
}
