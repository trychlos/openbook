/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "my/my-char.h"
#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"

#include "importers/ofa-importer-txt-lcl.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImporterTxtLclPrivate;

typedef struct _sParser                   sParser;

#define IMPORTER_CANON_NAME              "LCL tabulated-BAT importer"
#define IMPORTER_VERSION                  PACKAGE_VERSION

static GList *st_accepted_contents      = NULL;

typedef struct {
	const gchar *bat_label;
	const gchar *ofa_label;
}
	lclPaiement;

static const lclPaiement st_lcl_paiements[] = {
		{ "Carte",       "CB" },
		{ "Virement",    "VIR" },
		{ "Prélèvement", "PREL" },
		{ "Chèque",      "CH" },
		{ "TIP",         "TIP" },
		{ 0 }
};

static void             iident_iface_init( myIIdentInterface *iface );
static gchar           *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar           *iident_get_version( const myIIdent *instance, void *user_data );
static void             iimporter_iface_init( ofaIImporterInterface *iface );
static const GList     *iimporter_get_accepted_contents( const ofaIImporter *instance, ofaIGetter *getter );
static gboolean         iimporter_is_willing_to( const ofaIImporter *instance, ofaIGetter *getter, const gchar *uri, GType type );
static gboolean         is_willing_to_parse( const ofaImporterTxtLcl *self, ofaIGetter *getter, const gchar *uri );
static ofaStreamFormat *iimporter_get_default_format( const ofaIImporter *instance, ofaIGetter *getter, gboolean *is_updatable );
static GSList          *iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr );
static GSList          *do_parse( ofaImporterTxtLcl *self, ofsImporterParms *parms, gchar **msgerr );
static gboolean         lcl_tabulated_text_v1_check( const ofaImporterTxtLcl *self, const sParser *parser, ofaStreamFormat *format, const GSList *lines );
static GSList          *lcl_tabulated_text_v1_parse( ofaImporterTxtLcl *self, const sParser *parser, ofsImporterParms *parms, GSList *lines );
static GSList          *parse_solde_v1( ofaImporterTxtLcl *self, const sParser *parser, ofsImporterParms *parms, GSList *fields );
static GSList          *parse_detail_v1( ofaImporterTxtLcl *self, const sParser *parser, ofsImporterParms *parms, GSList *fields );
static sParser         *get_willing_to_parser( const ofaImporterTxtLcl *self, ofaStreamFormat *format, const GSList *lines );
static ofaStreamFormat *get_default_stream_format( const ofaImporterTxtLcl *self, ofaIGetter *getter );
static GSList          *split_by_field( const ofaImporterTxtLcl *self, const gchar *line, ofaStreamFormat *format );
static gchar           *get_ref_paiement( const gchar *str );
static gchar           *concatenate_string( ofaImporterTxtLcl *self, const sParser *parser, GSList **list, const gchar *prev );

G_DEFINE_TYPE_EXTENDED( ofaImporterTxtLcl, ofa_importer_txt_lcl, OFA_TYPE_IMPORTER_TXT, 0,
		G_ADD_PRIVATE( ofaImporterTxtLcl )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

/* A description of the import functions we are able to manage here.
 * If several versions happen to be managed, then the most recent
 * should be set first.
 */
struct _sParser {
	const gchar *label;
	guint        version;
	gboolean   (*fnTest) ( const ofaImporterTxtLcl *, const sParser *, ofaStreamFormat *, const GSList * );
	GSList *   (*fnParse)( ofaImporterTxtLcl *, const sParser *, ofsImporterParms *, GSList * );
};

static sParser st_parsers[] = {
		{ N_( "LCL.xls (tabulated text) 2014" ), 1, lcl_tabulated_text_v1_check, lcl_tabulated_text_v1_parse },
		{ 0 }
};

static void
importer_txt_lcl_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_txt_lcl_finalize";

	g_return_if_fail( instance && OFA_IS_IMPORTER_TXT_LCL( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_txt_lcl_parent_class )->finalize( instance );
}

static void
importer_txt_lcl_dispose( GObject *instance )
{
	ofaImporterTxtLclPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_TXT_LCL( instance ));

	priv = ofa_importer_txt_lcl_get_instance_private( OFA_IMPORTER_TXT_LCL( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_txt_lcl_parent_class )->dispose( instance );
}

static void
ofa_importer_txt_lcl_init( ofaImporterTxtLcl *self )
{
	static const gchar *thisfn = "ofa_importer_txt_lcl_init";
	ofaImporterTxtLclPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_TXT_LCL( self ));

	priv = ofa_importer_txt_lcl_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_txt_lcl_class_init( ofaImporterTxtLclClass *klass )
{
	static const gchar *thisfn = "ofa_importer_txt_lcl_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_txt_lcl_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_txt_lcl_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_importer_txt_lcl_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( IMPORTER_CANON_NAME ));
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
	static const gchar *thisfn = "ofa_importer_txt_lcl_iimporter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_accepted_contents = iimporter_get_accepted_contents;
	iface->is_willing_to = iimporter_is_willing_to;
	iface->get_default_format = iimporter_get_default_format;
	iface->parse = iimporter_parse;
}

static const GList *
iimporter_get_accepted_contents( const ofaIImporter *instance, ofaIGetter *getter )
{
	if( !st_accepted_contents ){
		st_accepted_contents = g_list_prepend( NULL, "application/vnd.ms-excel" );
	}

	return( st_accepted_contents );
}

static gboolean
iimporter_is_willing_to( const ofaIImporter *instance, ofaIGetter *getter, const gchar *uri, GType type )
{
	gboolean ok;

	ok = ofa_importer_txt_is_willing_to( OFA_IMPORTER_TXT( instance ), getter, uri, iimporter_get_accepted_contents( instance, getter )) &&
			type == OFO_TYPE_BAT &&
			is_willing_to_parse( OFA_IMPORTER_TXT_LCL( instance ), getter, uri );

	return( ok );
}

static ofaStreamFormat *
iimporter_get_default_format( const ofaIImporter *instance, ofaIGetter *getter, gboolean *updatable )
{
	ofaStreamFormat *format;

	format = get_default_stream_format( OFA_IMPORTER_TXT_LCL( instance ), getter );

	if( updatable ){
		*updatable = FALSE;
	}

	return( format );
}

/*
 * do the minimum to identify the file: parse the first line
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
is_willing_to_parse( const ofaImporterTxtLcl *self, ofaIGetter *getter, const gchar *uri )
{
	ofaStreamFormat *format;
	GSList *lines;
	sParser *parser;

	parser = NULL;
	format = get_default_stream_format( self, getter );
	lines = my_utils_uri_get_lines( uri, ofa_stream_format_get_charmap( format ), NULL, NULL );

	if( lines ){
		parser = get_willing_to_parser( self, format, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	}

	g_object_unref( format );

	return( parser != NULL );
}

static GSList *
iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr )
{
	g_return_val_if_fail( parms->getter && OFA_IS_IGETTER( parms->getter ), NULL );
	g_return_val_if_fail( my_strlen( parms->uri ), NULL );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), NULL );

	return( do_parse( OFA_IMPORTER_TXT_LCL( instance ), parms, msgerr ));
}

static GSList *
do_parse( ofaImporterTxtLcl *self, ofsImporterParms *parms, gchar **msgerr )
{
	GSList *lines, *out_by_fields;
	sParser *parser;

	if( msgerr ){
		*msgerr = NULL;
	}

	out_by_fields = NULL;
	lines = my_utils_uri_get_lines( parms->uri, ofa_stream_format_get_charmap( parms->format ), NULL, msgerr );

	if( *msgerr ){
		return( NULL );
	}

	if( lines ){
		parser = get_willing_to_parser( self, parms->format, lines );
		if( parser ){
			out_by_fields = parser->fnParse( self, parser, parms, lines );
		}
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	}

	return( out_by_fields );
}

/*
 * only interpret the first line
 *  07/11/2014	-34,0	Virement		PRLV SEPA Free Telecom
 */
static gboolean
lcl_tabulated_text_v1_check( const ofaImporterTxtLcl *self, const sParser *parser, ofaStreamFormat *format, const GSList *lines )
{
	static const gchar *thisfn = "ofa_importer_txt_lcl_v1_check";
	GSList *fields, *it;
	const gchar *cstr;
	GDate date;
	gdouble amount;

	/* only interpret first line */
	fields = split_by_field( self, lines->data, format );

	/* first field = value date */
	it = fields ? fields : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	my_date_set_from_str( &date, cstr, ofa_stream_format_get_date_format( format ));
	if( !my_date_is_valid( &date )){
		g_debug( "%s: unable to parse the date: '%s'", thisfn, cstr );
		return( FALSE );
	}
	/* second field is the amount */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	amount = my_double_set_from_str( cstr,
			ofa_stream_format_get_thousand_sep( format ), ofa_stream_format_get_decimal_sep( format ));
	if( !amount ){
		g_debug( "%s: unable to parse the amount: '%s'", thisfn, cstr );
		return( FALSE );
	}
	/* other fields may be empty */

	g_slist_free_full( fields, ( GDestroyNotify ) g_free );

	return( TRUE );
}

/*
 * solde is in the last line
 */
static GSList  *
lcl_tabulated_text_v1_parse( ofaImporterTxtLcl *self, const sParser *parser, ofsImporterParms *parms, GSList *lines )
{
	GSList *output;
	GSList *rev_lines, *it, *fields;

	output = NULL;

	rev_lines = g_slist_reverse( lines );
	fields = split_by_field( self, ( const gchar * ) rev_lines->data, parms->format );
	output = g_slist_prepend( output, parse_solde_v1( self, parser, parms, fields ));
	g_slist_free_full( fields, ( GDestroyNotify ) g_free );

	rev_lines = rev_lines->next;
	for( it=rev_lines ; it ; it=it->next ){
		fields = split_by_field( self, ( const gchar * ) it->data, parms->format );
		output = g_slist_prepend( output, parse_detail_v1( self, parser, parms, fields ));
		g_slist_free_full( fields, ( GDestroyNotify ) g_free );
	}

	return( g_slist_reverse( output ));
}

static GSList  *
parse_solde_v1( ofaImporterTxtLcl *self, const sParser *parser, ofsImporterParms *parms, GSList *fields )
{
	GSList *output, *it;
	const gchar *cstr;
	gchar *rib, *sdate, *ssolde;

	output = NULL;

	/* ending date */
	it = fields ? fields : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	sdate = g_strdup( cstr );

	/* ending solde */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	ssolde = g_strdup( cstr );

	/* no reference */
	it = it ? it->next : NULL;
	it = it ? it->next : NULL;

	/* rib */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	rib = g_strstrip( g_strdup( cstr ));;

	output = g_slist_prepend( output, g_strdup( "1" ));
	output = g_slist_prepend( output, g_strdup( "" ));					/* id placeholder */
	output = g_slist_prepend( output, g_strdup( parms->uri ));
	output = g_slist_prepend( output, g_strdup( parser->label ));
	output = g_slist_prepend( output, rib );
	output = g_slist_prepend( output, g_strdup( "" ));					/* currency */
	output = g_slist_prepend( output, g_strdup( "" ));					/* begin date */
	output = g_slist_prepend( output, g_strdup( "" ));					/* begin solde */
	output = g_slist_prepend( output, g_strdup( "N" ));
	output = g_slist_prepend( output, sdate );
	output = g_slist_prepend( output, ssolde );
	output = g_slist_prepend( output, g_strdup( "Y" ));

	return( g_slist_reverse( output ));
}

static GSList  *
parse_detail_v1( ofaImporterTxtLcl *self, const sParser *parser, ofsImporterParms *parms, GSList *fields )
{
	GSList *output, *it;
	const gchar *cstr;
	gchar *sdate, *samount, *sref, *tmp, *slabel;

	output = NULL;

	/* effect date */
	it = fields ? fields : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	sdate = g_strdup( cstr );

	/* amount */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	samount = g_strdup( cstr );

	/* reference */
	it = it ? it->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	sref = get_ref_paiement( cstr );
	tmp = concatenate_string( self, parser, &it, sref );
	g_free( sref );
	sref = tmp ? tmp : g_strdup( "" );

	/* label */
	slabel = concatenate_string( self, parser, &it, NULL );
	tmp = concatenate_string( self, parser, &it, slabel );
	g_free( slabel );
	slabel = tmp ? tmp : g_strdup( "" );

	output = g_slist_prepend( output, g_strdup( "2" ));
	output = g_slist_prepend( output, g_strdup( "" ));		/* id placeholder */
	output = g_slist_prepend( output, g_strdup( "" ));		/* operation date */
	output = g_slist_prepend( output, sdate );
	output = g_slist_prepend( output, sref );
	output = g_slist_prepend( output, slabel );
	output = g_slist_prepend( output, samount );
	output = g_slist_prepend( output, g_strdup( "" ));		/* currency */

	return( g_slist_reverse( output ));
}

static sParser *
get_willing_to_parser( const ofaImporterTxtLcl *self, ofaStreamFormat *format, const GSList *lines )
{
	sParser *parser;
	gint i;

	parser = NULL;

	for( i=0 ; st_parsers[i].label ; ++i ){
		if( st_parsers[i].fnTest( self, &st_parsers[i], format, lines )){
			parser = &st_parsers[i];
			break;
		}
	}

	return( parser );
}

static ofaStreamFormat *
get_default_stream_format( const ofaImporterTxtLcl *self, ofaIGetter *getter )
{
	ofaStreamFormat *format;

	format = ofa_stream_format_new( getter, NULL, OFA_SFMODE_IMPORT );

	ofa_stream_format_set( format,
			TRUE,  "ISO-8859-15",			/* Western Europe */
			TRUE,  MY_DATE_DMYY,			/* date format dd/mm/yyyy */
			FALSE, MY_CHAR_ZERO,			/* no thousand sep */
			TRUE,  MY_CHAR_COMMA,			/* comma decimal sep */
			TRUE,  MY_CHAR_TAB,				/* tab field sep */
			FALSE, MY_CHAR_ZERO,			/* no string delim */
			0 );							/* no header */

	ofa_stream_format_set_field_updatable( format, OFA_SFHAS_ALL, FALSE );

	return( format );
}

/*
 * output new GSList with newly allocated strings
 * free with g_slist_free_full( list, ( GDestroyNotify ) g_free )
 */
static GSList *
split_by_field( const ofaImporterTxtLcl *self, const gchar *line, ofaStreamFormat *format )
{
	GSList *out;
	gchar **tokens, **iter;
	gchar *str;

	out = NULL;

	if( ofa_stream_format_get_has_field( format )){
		str = g_strdup_printf( "%c", ofa_stream_format_get_field_sep( format ));
		tokens = g_strsplit( line, str, -1 );
		g_free( str );

		iter = tokens;

		while( *iter ){
			out = g_slist_prepend( out, g_strstrip( g_strdup( *iter )));
			iter++;
		}

		g_strfreev( tokens );

	} else {
		out = g_slist_prepend( out, g_strdup( line ));
	}

	return( g_slist_reverse( out ));
}

static gchar *
get_ref_paiement( const gchar *cstr )
{
	gchar *found, *str;
	gint i;

	found = NULL;
	str = g_strstrip( g_strdup( cstr ));

	if( my_strlen( str )){
		for( i=0 ; st_lcl_paiements[i].bat_label ; ++ i ){
			if( !g_utf8_collate( str, st_lcl_paiements[i].bat_label )){
				found = g_strstrip( g_strdup( st_lcl_paiements[i].ofa_label ));
				break;
			}
		}
		if( !found ){
			found = g_strdup( str );
		}
	}

	g_free( str );
	return( found );
}

/*
 * concatenate the next string
 */
static gchar *
concatenate_string( ofaImporterTxtLcl *self, const sParser *parser, GSList **list, const gchar *prev )
{
	GSList *it;
	const gchar *cstr;
	gchar *label, *tmp, *tmp2;

	label = my_strlen( prev ) ? g_strstrip( g_strdup( prev )) : g_strdup( "" );

	it = ( *list ) ? ( *list )->next : NULL;
	cstr = it ? ( const gchar * ) it->data : NULL;
	if( my_strlen( cstr )){
		if( my_strlen( label )){
			tmp = g_strdup_printf( "%s ", label );
			g_free( label );
			label = tmp;
		}
		tmp = g_strstrip( g_strdup( cstr ));
		tmp2 = g_strdup_printf( "%s%s", label, tmp );
		g_free( tmp );
		g_free( label );
		label = tmp2;
	}

	*list = it;

	return( label );
}
