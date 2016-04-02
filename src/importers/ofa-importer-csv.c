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

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"

#include "importers/ofa-importer-csv.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImporterCSVPrivate;

#define IMPORTER_DISPLAY_NAME            "Text/CSV importer"
#define IMPORTER_VERSION                 "v 2016.1"

static GList *st_accepted_contents      = NULL;

static void         iident_iface_init( myIIdentInterface *iface );
static gchar       *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar       *iident_get_version( const myIIdent *instance, void *user_data );
static void         iimporter_iface_init( ofaIImporterInterface *iface );
static const GList *iimporter_get_accepted_contents( const ofaIImporter *instance );
static gboolean     iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type );
static guint        iimporter_import( ofaIImporter *instance, ofsImporterParms *parms );
static guint        do_import_csv( ofaIImporter *instance, ofsImporterParms *parms );
static GSList      *get_lines_from_content( const gchar *content, const ofaStreamFormat *settings, guint *errors, gchar **msgerr );
static GSList      *split_by_line( const gchar *content, const ofaStreamFormat *settings, guint *errors, gchar **msgerr );
static GSList      *split_by_field( const gchar *line, guint numline, const ofaStreamFormat *settings, guint *errors, gchar **msgerr );
static void         free_lines( GSList *lines );
static void         free_fields( GSList *fields );
static void         set_error_message( ofaIImporter *instance, ofsImporterParms *parms, const gchar *msgerr );

G_DEFINE_TYPE_EXTENDED( ofaImporterCSV, ofa_importer_csv, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaImporterCSV )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

static void
importer_csv_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_csv_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_IMPORTER_CSV( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_csv_parent_class )->finalize( instance );
}

static void
importer_csv_dispose( GObject *instance )
{
	ofaImporterCSVPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_CSV( instance ));

	priv = ofa_importer_csv_get_instance_private( OFA_IMPORTER_CSV( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_csv_parent_class )->dispose( instance );
}

static void
ofa_importer_csv_init( ofaImporterCSV *self )
{
	static const gchar *thisfn = "ofa_importer_csv_init";
	ofaImporterCSVPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_CSV( self ));

	priv = ofa_importer_csv_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_csv_class_init( ofaImporterCSVClass *klass )
{
	static const gchar *thisfn = "ofa_importer_csv_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_csv_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_csv_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_importer_csv_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( IMPORTER_DISPLAY_NAME ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( IMPORTER_VERSION ));
}

/*
 * ofaIImporter interface management
 */
static void
iimporter_iface_init( ofaIImporterInterface *iface )
{
	static const gchar *thisfn = "ofa_importer_csv_iimporter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_accepted_contents = iimporter_get_accepted_contents;
	iface->is_willing_to = iimporter_is_willing_to;
	iface->import = iimporter_import;
}

static const GList *
iimporter_get_accepted_contents( const ofaIImporter *instance )
{
	if( !st_accepted_contents ){
		st_accepted_contents = g_list_prepend( NULL, "text/csv" );
	}

	return( st_accepted_contents );
}

/*
 * just check that the provided file is a csv one
 */
static gboolean
iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type )
{
	gchar *filename, *content;
	gboolean ok;

	filename = g_filename_from_uri( uri, NULL, NULL );
	content = g_content_type_guess( filename, NULL, 0, NULL );
	ok = ( my_collate( content, "text/csv" ) == 0 );

	g_free( content );
	g_free( filename );

	return( ok );
}

static guint
iimporter_import( ofaIImporter *instance, ofsImporterParms *parms )
{
	g_return_val_if_fail( parms->hub && OFA_IS_HUB( parms->hub ), 1 );
	g_return_val_if_fail( my_strlen( parms->uri ), 1 );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), 1 );
	g_return_val_if_fail( ofa_stream_format_get_has_field( parms->format ), 1 );

	return( do_import_csv( instance, parms ));
}

static guint
do_import_csv( ofaIImporter *instance, ofsImporterParms *parms )
{
	guint count, errors;
	gchar *content, *msgerr;
	GSList *lines;
	gint headers_count;
	GObject *object;
	gboolean has_charmap;
	const gchar *charmap;

	/* load file content in memory + charmap conversion */
	msgerr = NULL;
	errors = 0;
	if( !errors ){
		has_charmap = ofa_stream_format_get_has_charmap( parms->format );
		charmap = has_charmap ? ofa_stream_format_get_charmap( parms->format ) : NULL;
		content = my_utils_uri_get_content( parms->uri, charmap, &errors, &msgerr );
		if( errors ){
			set_error_message( instance, parms, msgerr );
			g_free( msgerr );
		}
	}

	/* convert buffer content to list of lines, and lists of fields
	 * take into account field separator and string delimiter if any */
	lines = NULL;
	if( !errors ){
		lines = get_lines_from_content( content, parms->format, &errors, &msgerr );
		if( errors ){
			set_error_message( instance, parms, msgerr );
			g_free( msgerr );
		}
	}

	/* import the dataset */
	if( lines ){
		parms->lines_count = g_slist_length( lines );
		headers_count = ofa_stream_format_get_headers_count( parms->format );
		count = parms->lines_count - headers_count;

		if( count > 0 ){
			object = g_object_new( parms->type, NULL );

			if( OFA_IS_IIMPORTABLE( object )){
				errors = ofa_iimportable_import( OFA_IIMPORTABLE( object ), instance, parms, g_slist_nth( lines, headers_count ));

			} else {
				errors += 1;
				msgerr = g_strdup_printf(
						_( "Target class name %s does not implement ofaIImportable interface (but should)" ),
						g_type_name( parms->type ));
				set_error_message( instance, parms, msgerr );
				g_free( msgerr );
			}
			g_object_unref( object );

		} else if( count < 0 ){
			errors += 1;
			msgerr = g_strdup_printf(
							_( "Expected headers count=%u greater than count of lines=%u read from '%s' file" ),
								headers_count, parms->lines_count, parms->uri );
			set_error_message( instance, parms, msgerr );
			g_free( msgerr );
		}

		free_lines( lines );
	}

	g_free( content );

	return( errors );
}

/*
 * Returns a GSList of lines, where each lines->data is a GSList of fields.
 * We have to explore the content to extract each field which
 * may or may not be quoted, considering only newlines which are not
 * inside a quoted field.
 */
static GSList *
get_lines_from_content( const gchar *content, const ofaStreamFormat *settings, guint *errors, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_importer_csv_get_lines_from_content";
	GSList *eol_list, *splitted, *it;
	guint numline;

	/* UTF-8 validation */
	if( !g_utf8_validate( content, -1, NULL )){
		*errors += 1;
		*msgerr = g_strdup_printf( _( "The provided string is not UTF8-valide: '%s'" ), content );
		return( NULL );
	}

	/* split on end-of-line */
	eol_list = split_by_line( content, settings, errors, msgerr );

	/* fields have now to be splitted when field separator is not backslashed */
	splitted = NULL;
	numline = 0;
	for( it=eol_list ; it ; it=it->next ){
		numline += 1;
		splitted = g_slist_prepend( splitted, split_by_field(( const gchar * ) it->data, numline, settings, errors, msgerr ));
	}

	g_slist_free_full( eol_list, ( GDestroyNotify ) g_free );
	g_debug( "%s: splitted count=%d", thisfn, g_slist_length( splitted ));

	return( g_slist_reverse( splitted ));
}

/*
 * split the content into a GSList of lines
 * taking into account the possible backslashed end-of-lines
 */
static GSList *
split_by_line( const gchar *content, const ofaStreamFormat *settings, guint *errors, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_importer_csv_split_by_line";
	GSList *eol_list;
	gchar **lines, **it_line;
	gchar *prev, *temp;
	guint numline;

	/* split on end-of-line
	 * then re-concatenate segments when end-of-line was backslashed */
	lines = g_strsplit( content, "\n", -1 );
	it_line = lines;
	prev = NULL;
	eol_list = NULL;
	numline = 0;

	while( *it_line ){
		if( prev ){
			if( 0 ){
				g_debug( "num=%u line='%s'", numline, prev );
			}
			temp = g_utf8_substring( prev, 0, my_strlen( prev )-1 );
			g_free( prev );
			prev = temp;
			temp = g_strconcat( prev, "\n", *it_line, NULL );
			g_free( prev );
			prev = temp;
		} else {
			prev = g_strdup( *it_line );
		}
		if( my_strlen( prev ) && !g_str_has_suffix( prev, "\\" )){
			numline += 1;
			eol_list = g_slist_prepend( eol_list, g_strdup( prev ));
			g_free( prev );
			prev = NULL;
		} else if( !my_strlen( prev )){
			g_free( prev );
			prev = NULL;
		}
		it_line++;
	}
	g_strfreev( lines );

	if( 0 ){
		g_debug( "%s: tmp_list count=%d", thisfn, g_slist_length( eol_list ));
	}

	return( g_slist_reverse( eol_list ));
}

/*
 * Returns a GSList of fields.
 */
static GSList *
split_by_field( const gchar *line, guint numline, const ofaStreamFormat *settings, guint *errors, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_importer_csv_split_by_field";
	GSList *out_list;
	gchar *fieldsep_str;
	gchar **fields, **it_field;
	gchar *prev, *temp;
	gchar fieldsep, strdelim;

	/* fields have now to be splitted when field separator is not backslashed */
	out_list = NULL;
	fieldsep = ofa_stream_format_get_field_sep( settings );
	fieldsep_str = g_strdup_printf( "%c", fieldsep );
	strdelim = 0;
	if( ofa_stream_format_get_has_strdelim( settings )){
		strdelim = ofa_stream_format_get_string_delim( settings );
	}
	fields = g_strsplit( line, fieldsep_str, -1 );
	it_field = fields;
	prev = NULL;
	while( *it_field ){
		if( prev ){
			temp = g_strconcat( prev, fieldsep_str, *it_field, NULL );
			g_free( prev );
			prev = temp;
		} else {
			prev = g_strdup( *it_field );
		}
		if( !g_str_has_suffix( prev, "\\" )){
			if( 0 ){
				g_debug( "%s: numline=%u, field='%s'", thisfn, numline, prev );
			}
			if( strdelim ){
				temp = my_utils_str_remove_str_delim( prev, fieldsep, strdelim );
				g_free( prev );
				prev = temp;
			}
			out_list = g_slist_prepend( out_list, prev );
			prev = NULL;
		}
		it_field++;
	}
	g_strfreev( fields );
	g_free( fieldsep_str );

	return( g_slist_reverse( out_list ));
}

static void
free_lines( GSList *lines )
{
	g_slist_free_full( lines, ( GDestroyNotify ) free_fields );
}

static void
free_fields( GSList *fields )
{
	g_slist_free_full( fields, ( GDestroyNotify ) g_free );
}

static void
set_error_message( ofaIImporter *instance, ofsImporterParms *parms, const gchar *msgerr )
{
	if( parms->progress ){
		my_iprogress_set_text( parms->progress, instance, msgerr );
	}
}
