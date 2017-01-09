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
#define IMPORTER_VERSION                 "2016.1"

static GList *st_accepted_contents      = NULL;

static void         iident_iface_init( myIIdentInterface *iface );
static gchar       *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar       *iident_get_version( const myIIdent *instance, void *user_data );
static void         iimporter_iface_init( ofaIImporterInterface *iface );
static const GList *iimporter_get_accepted_contents( const ofaIImporter *instance, ofaHub *hub );
static gboolean     iimporter_is_willing_to( const ofaIImporter *instance, ofaHub *hub, const gchar *uri, GType type );
static GSList      *iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr );
static GSList      *do_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr );
static GSList      *split_lines_by_field( GSList *lines, ofaStreamFormat *settings, gchar **msgerr );
static GSList      *split_by_field( const gchar *line, guint numline, ofaStreamFormat *settings, gchar **msgerr );

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
	iface->parse = iimporter_parse;
}

static const GList *
iimporter_get_accepted_contents( const ofaIImporter *instance, ofaHub *hub )
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
iimporter_is_willing_to( const ofaIImporter *instance, ofaHub *hub, const gchar *uri, GType type )
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

static GSList *
iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr )
{
	g_return_val_if_fail( parms->hub && OFA_IS_HUB( parms->hub ), NULL );
	g_return_val_if_fail( my_strlen( parms->uri ), NULL );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), NULL );
	g_return_val_if_fail( ofa_stream_format_get_has_field( parms->format ), NULL );

	return( do_parse( instance, parms, msgerr ));
}

static GSList *
do_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr )
{
	GSList *lines, *out_by_fields;
	guint error_count;

	error_count = 0;
	if( msgerr ){
		*msgerr = NULL;
	}

	lines = my_utils_uri_get_lines( parms->uri, ofa_stream_format_get_charmap( parms->format ), &error_count, msgerr );

	if( *msgerr ){
		return( NULL );
	}

	out_by_fields = split_lines_by_field( lines, parms->format, msgerr );

	g_slist_free_full( lines, ( GDestroyNotify ) g_free );

	return( out_by_fields );
}

/*
 * Returns a GSList of lines, where each lines->data is a GSList of fields.
 * We have to explore the content to extract each field which
 * may or may not be quoted, considering only newlines which are not
 * inside a quoted field.
 */
static GSList *
split_lines_by_field( GSList *lines, ofaStreamFormat *settings, gchar **msgerr )
{
	static const gchar *thisfn = "ofa_importer_csv_get_lines_from_content";
	GSList *splitted, *it;
	guint numline;

	/* fields have now to be splitted when field separator is not backslashed */
	splitted = NULL;
	numline = 0;
	for( it=lines ; it ; it=it->next ){
		numline += 1;
		splitted = g_slist_prepend( splitted, split_by_field(( const gchar * ) it->data, numline, settings, msgerr ));
	}
	g_debug( "%s: splitted count=%d", thisfn, g_slist_length( splitted ));

	return( g_slist_reverse( splitted ));
}

/*
 * Returns a GSList of fields.
 */
static GSList *
split_by_field( const gchar *line, guint numline, ofaStreamFormat *settings, gchar **msgerr )
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
