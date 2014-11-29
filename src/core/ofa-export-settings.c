/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "core/ofa-export-settings.h"

/* private instance data
 */
struct _ofaExportSettingsPrivate {
	gboolean        dispose_has_run;

	/* runtime data
	 */
	gchar          *name;				/* may be %NULL if defaults */
	ofaExportFormat export_format;
	gchar          *charmap;
	myDateFormat    date_format;
	gchar           decimal_sep;
	gchar           field_sep;			/* csv only */
	gboolean        with_headers;
};

typedef struct {
	ofaExportFormat format;
	const gchar    *label;
}
	sFormat;

static const sFormat st_export_format[] = {
		{ OFA_EXPORT_CSV,   N_( "CSV-like format" )},
		{ OFA_EXPORT_FIXED, N_( "Fixed format" )},
		{ 0 }
};

static const gchar *st_prefs            = "ExportSettings";

static gint         st_def_format       = OFA_EXPORT_CSV;
static const gchar *st_def_charmap      = "UTF-8";
static const gint   st_def_date         = MY_DATE_SQL;
static const gchar *st_def_decimal      = ".";
static const gchar *st_def_field_sep    = ";";
static const gchar *st_def_headers      = "True";

G_DEFINE_TYPE( ofaExportSettings, ofa_export_settings, G_TYPE_OBJECT )

static void  do_init( ofaExportSettings *self, const gchar *name );

static void
export_settings_finalize( GObject *instance )
{
	ofaExportSettingsPrivate *priv;

	static const gchar *thisfn = "ofa_export_settings_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EXPORT_SETTINGS( instance ));

	/* free data members here */
	priv = OFA_EXPORT_SETTINGS( instance )->priv;

	g_free( priv->charmap );
	g_free( priv->name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_settings_parent_class )->finalize( instance );
}

static void
export_settings_dispose( GObject *instance )
{
	ofaExportSettingsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EXPORT_SETTINGS( instance ));

	priv = OFA_EXPORT_SETTINGS( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_export_settings_parent_class )->dispose( instance );
}

static void
ofa_export_settings_init( ofaExportSettings *self )
{
	static const gchar *thisfn = "ofa_export_settings_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EXPORT_SETTINGS( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_EXPORT_SETTINGS, ofaExportSettingsPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_export_settings_class_init( ofaExportSettingsClass *klass )
{
	static const gchar *thisfn = "ofa_export_settings_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = export_settings_dispose;
	G_OBJECT_CLASS( klass )->finalize = export_settings_finalize;

	g_type_class_add_private( klass, sizeof( ofaExportSettingsPrivate ));
}

/**
 * ofa_export_settings_new:
 * @name: [allow-none]: the name of these export settings,
 *  or %NULL to get the defaults.
 *
 * Returns: a newly allocated #ofaExportSettings object.
 * The object has been initialized with the named defaults.
 */
ofaExportSettings *
ofa_export_settings_new( const gchar *name )
{
	ofaExportSettings *self;

	self = g_object_new( OFA_TYPE_EXPORT_SETTINGS, NULL );

	do_init( self, name );

	return( self );
}

static void
do_init( ofaExportSettings *self, const gchar *name )
{
	ofaExportSettingsPrivate *priv;
	gchar *pref_name;
	GList *prefs_list, *it;
	gchar *text;
	gint nth;
	const gchar *cstr;

	priv = self->priv;
	prefs_list = NULL;
	nth = -1;

	/* name of these export settings, may be %NULL if defaults */
	priv->name = g_strdup( name );

	/* get prefs */
	if( name && g_utf8_strlen( name, -1 )){
		pref_name = g_strdup_printf( "%s%s", st_prefs, name );
		prefs_list = ofa_settings_get_string_list( pref_name );
		g_free( pref_name );
	}
	if( !prefs_list ){
		prefs_list = ofa_settings_get_string_list( st_prefs );
	}

	/* export format */
	it = g_list_nth( prefs_list, ++nth );
	text = g_strdup_printf( "%d", st_def_format );
	cstr = ( it && g_utf8_strlen(( const gchar * ) it->data, -1 )) ?
								 ( const gchar * ) it->data : text;
	priv->export_format = atoi( cstr );
	g_free( text );

	/* charmap */
	it = g_list_nth( prefs_list, ++nth );
	cstr = ( it && g_utf8_strlen(( const gchar * ) it->data, -1 )) ?
								 ( const gchar * ) it->data : st_def_charmap;
	priv->charmap = g_strdup( cstr );

	/* date format */
	it = g_list_nth( prefs_list, ++nth );
	text = g_strdup_printf( "%d", st_def_date );
	cstr = ( it && g_utf8_strlen(( const gchar * ) it->data, -1 )) ?
								 ( const gchar * ) it->data : text;
	priv->date_format = atoi( cstr );
	g_free( text );

	/* decimal separator */
	it = g_list_nth( prefs_list, ++nth );
	cstr = ( it && g_utf8_strlen(( const gchar * ) it->data, -1 )) ?
								 ( const gchar * ) it->data : st_def_decimal;
	priv->decimal_sep = atoi( cstr );

	/* field separator */
	it = g_list_nth( prefs_list, ++nth );
	cstr = ( it && g_utf8_strlen(( const gchar * ) it->data, -1 )) ?
								 ( const gchar * ) it->data : st_def_field_sep;
	priv->field_sep = atoi( cstr );

	/* with headers */
	it = g_list_nth( prefs_list, ++nth );
	cstr = ( it && g_utf8_strlen(( const gchar * ) it->data, -1 )) ?
								 ( const gchar * ) it->data : st_def_headers;
	priv->with_headers = g_utf8_collate( cstr, "True" ) == 0;

	ofa_settings_free_string_list( prefs_list );
}

/**
 * ofa_export_settings_get_export_format:
 */
ofaExportFormat
ofa_export_settings_get_export_format( const ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->export_format );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_export_settings_get_export_format_str:
 */
const gchar *
ofa_export_settings_get_export_format_str( ofaExportFormat format )
{
	static const gchar *thisfn = "ofa_export_settings_get_export_format_str";
	gint i;

	for( i=0 ; st_export_format[i].format ; ++i ){
		if( st_export_format[i].format == format ){
			return( gettext( st_export_format[i].label ));
		}
	}

	/* only a debug message as the overflow of the previous loop is
	 * used when enumerating valid export formats */
	g_debug( "%s: unknown file format: %d", thisfn, format );
	return( NULL );
}

/**
 * ofa_export_settings_get_charmap:
 */
const gchar *
ofa_export_settings_get_charmap( const ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), NULL );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return(( const gchar * ) priv->charmap );
	}

	g_return_val_if_reached( NULL );
	return( NULL );
}

/**
 * ofa_export_settings_get_date_format:
 */
myDateFormat
ofa_export_settings_get_date_format( const ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), -1 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->date_format );
	}

	g_return_val_if_reached( -1 );
	return( -1 );
}

/**
 * ofa_export_settings_get_decimal_sep:
 */
gchar
ofa_export_settings_get_decimal_sep( const ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->decimal_sep );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_export_settings_get_field_sep:
 */
gchar
ofa_export_settings_get_field_sep( const ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), 0 );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->field_sep );
	}

	g_return_val_if_reached( 0 );
	return( 0 );
}

/**
 * ofa_export_settings_get_headers:
 */
gboolean
ofa_export_settings_get_headers( const ofaExportSettings *settings )
{
	ofaExportSettingsPrivate *priv;

	g_return_val_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ), FALSE );

	priv = settings->priv;

	if( !priv->dispose_has_run ){

		return( priv->with_headers );
	}

	g_return_val_if_reached( FALSE );
	return( FALSE );
}

/**
 * ofa_export_settings_set:
 * @settings:
 * @charmap:
 * @date_format:
 * @decimal_sep:
 * @with_headers:
 * @field_sep:
 */
void
ofa_export_settings_set( ofaExportSettings *settings,
								ofaExportFormat export_format,
								const gchar *charmap,
								myDateFormat date_format,
								gchar decimal_sep,
								gchar field_sep,
								gboolean with_headers )
{
	ofaExportSettingsPrivate *priv;
	GList *prefs_list;
	gchar *sexport, *sdate, *sdecimal, *sfield, *pref_name, *sheaders;

	g_return_if_fail( settings && OFA_IS_EXPORT_SETTINGS( settings ));
	g_return_if_fail( charmap && g_utf8_strlen( charmap, -1 ));
	g_return_if_fail( decimal_sep );
	g_return_if_fail( field_sep );

	priv = settings->priv;
	prefs_list = NULL;

	if( !priv->dispose_has_run ){

		/* export format */
		priv->export_format = export_format;
		sexport = g_strdup_printf( "%d", export_format );
		prefs_list = g_list_append( prefs_list, sexport );

		/* charmap */
		g_free( priv->charmap );
		priv->charmap = g_strdup( charmap );
		prefs_list = g_list_append( prefs_list, priv->charmap );

		/* date format */
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
		priv->with_headers = with_headers;
		sheaders = g_strdup_printf( "%s", priv->with_headers ? "True":"False" );
		prefs_list = g_list_append( prefs_list, sheaders );

		/* save in user preferences */
		if( priv->name && g_utf8_strlen( priv->name, -1 )){
			pref_name = g_strdup_printf( "%s%s", st_prefs, priv->name );
		} else {
			pref_name = g_strdup( st_prefs );
		}
		ofa_settings_set_string_list( pref_name, prefs_list );
		g_free( pref_name );
		g_list_free( prefs_list );

		g_free( sheaders );
		g_free( sfield );
		g_free( sdecimal );
		g_free( sdate );
		g_free( sexport );
	}
}
