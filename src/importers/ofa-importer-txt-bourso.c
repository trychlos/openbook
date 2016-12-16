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

#include "my/my-char.h"
#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"

#include "importers/ofa-importer-txt-bourso.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImporterTxtBoursoPrivate;

typedef struct _sParser                   sParser;

#define IMPORTER_CANON_NAME              "Boursorama tabulated-BAT importer"
#define IMPORTER_VERSION                  PACKAGE_VERSION

static GList *st_accepted_contents      = NULL;

static void             iident_iface_init( myIIdentInterface *iface );
static gchar           *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar           *iident_get_version( const myIIdent *instance, void *user_data );
static void             iimporter_iface_init( ofaIImporterInterface *iface );
static const GList     *iimporter_get_accepted_contents( const ofaIImporter *instance, ofaHub *hub );
static gboolean         iimporter_is_willing_to( const ofaIImporter *instance, ofaHub *hub, const gchar *uri, GType type );
static gboolean         is_willing_to_parse( const ofaImporterTxtBourso *self, ofaHub *hub, const gchar *uri );
static ofaStreamFormat *iimporter_get_default_format( const ofaIImporter *instance, ofaHub *hub, gboolean *is_updatable );
static GSList          *iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr );
static GSList          *do_parse( ofaImporterTxtBourso *self, ofsImporterParms *parms, gchar **msgerr );
static gboolean         bourso_excel2002_v2_check( const ofaImporterTxtBourso *self, const sParser *parser, ofaStreamFormat *format, GSList *lines );
static GSList          *bourso_excel2002_v2_parse( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList *lines );
static gboolean         bourso_excel95_v1_check( const ofaImporterTxtBourso *self, const sParser *parser, ofaStreamFormat *format, GSList *lines );
static GSList          *bourso_excel95_v1_parse( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList *lines );
static gboolean         parse_v1_check( const ofaImporterTxtBourso *self, const sParser *parser, ofaStreamFormat *format, GSList *lines, const gchar *thisfn );
static GSList          *parse_v1_parse( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList *lines, const gchar *thisfn );
static gboolean         parse_v1_header( ofaStreamFormat *format, GSList **lines, gchar **dbegin, gchar **dend, gchar **rib, gchar **currency );
static gboolean         parse_v1_line_1( const gchar *line, gchar **dbegin, gchar **dend, ofaStreamFormat *format );
static gboolean         parse_v1_line_2( const gchar *line, gchar **rib, gchar **currency, ofaStreamFormat *format );
static GSList          *parse_v1_header_to_fields( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList **lines );
static GSList          *parse_v1_line_to_fields( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, const gchar *line );
static sParser         *get_willing_to_parser( const ofaImporterTxtBourso *self, ofaStreamFormat *format, GSList *lines );
static ofaStreamFormat *get_default_stream_format( const ofaImporterTxtBourso *self, ofaHub *hub );
static GSList          *split_by_field( const gchar *line, ofaStreamFormat *settings );

G_DEFINE_TYPE_EXTENDED( ofaImporterTxtBourso, ofa_importer_txt_bourso, OFA_TYPE_IMPORTER_TXT, 0,
		G_ADD_PRIVATE( ofaImporterTxtBourso )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

/* A description of the import functions we are able to manage here.
 * If several versions happen to be managed, then the most recent
 * should be set first.
 */
struct _sParser {
	const gchar *label;
	guint        version;
	gboolean   (*fnTest) ( const ofaImporterTxtBourso *, const sParser *, ofaStreamFormat *, GSList * );
	GSList *   (*fnParse)( ofaImporterTxtBourso *, const sParser *, ofsImporterParms *, GSList * );
};

static sParser st_parsers[] = {
		{ N_( "Boursorama.xls (tabulated text) Excel 2002" ), 1, bourso_excel2002_v2_check, bourso_excel2002_v2_parse },
		{ N_( "Boursorama.xls (tabulated text) Excel 95" ),   1, bourso_excel95_v1_check,   bourso_excel95_v1_parse },
		{ 0 }
};

static void
importer_txt_bourso_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_finalize";

	g_return_if_fail( instance && OFA_IS_IMPORTER_TXT_BOURSO( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_txt_bourso_parent_class )->finalize( instance );
}

static void
importer_txt_bourso_dispose( GObject *instance )
{
	ofaImporterTxtBoursoPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_TXT_BOURSO( instance ));

	priv = ofa_importer_txt_bourso_get_instance_private( OFA_IMPORTER_TXT_BOURSO( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_txt_bourso_parent_class )->dispose( instance );
}

static void
ofa_importer_txt_bourso_init( ofaImporterTxtBourso *self )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_init";
	ofaImporterTxtBoursoPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_TXT_BOURSO( self ));

	priv = ofa_importer_txt_bourso_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_txt_bourso_class_init( ofaImporterTxtBoursoClass *klass )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_txt_bourso_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_txt_bourso_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_iident_iface_init";

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
iimporter_get_accepted_contents( const ofaIImporter *instance, ofaHub *hub )
{
	if( !st_accepted_contents ){
		st_accepted_contents = g_list_prepend( NULL, "application/vnd.ms-excel" );
	}

	return( st_accepted_contents );
}

static gboolean
iimporter_is_willing_to( const ofaIImporter *instance, ofaHub *hub, const gchar *uri, GType type )
{
	gboolean ok;

	ok = ofa_importer_txt_is_willing_to( OFA_IMPORTER_TXT( instance ), hub, uri, iimporter_get_accepted_contents( instance, hub )) &&
			type == OFO_TYPE_BAT &&
			is_willing_to_parse( OFA_IMPORTER_TXT_BOURSO( instance ), hub, uri );

	return( ok );
}

/*
 * do the minimum to identify the file
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
is_willing_to_parse( const ofaImporterTxtBourso *self, ofaHub *hub, const gchar *uri )
{
	ofaStreamFormat *format;
	GSList *lines;
	sParser *parser;

	parser = NULL;
	format = get_default_stream_format( self, hub );
	lines = my_utils_uri_get_lines( uri, ofa_stream_format_get_charmap( format ), NULL, NULL );

	if( lines ){
		parser = get_willing_to_parser( self, format, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	}

	g_object_unref( format );

	return( parser != NULL );
}

static ofaStreamFormat *
iimporter_get_default_format( const ofaIImporter *instance, ofaHub *hub, gboolean *updatable )
{
	ofaStreamFormat *format;

	format = get_default_stream_format( OFA_IMPORTER_TXT_BOURSO( instance ), hub );

	if( updatable ){
		*updatable = FALSE;
	}

	return( format );
}

static GSList *
iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr )
{
	g_return_val_if_fail( parms->hub && OFA_IS_HUB( parms->hub ), NULL );
	g_return_val_if_fail( my_strlen( parms->uri ), NULL );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), NULL );

	return( do_parse( OFA_IMPORTER_TXT_BOURSO( instance ), parms, msgerr ));
}

static GSList *
do_parse( ofaImporterTxtBourso *self, ofsImporterParms *parms, gchar **msgerr )
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

static gboolean
bourso_excel2002_v2_check( const ofaImporterTxtBourso *self, const sParser *parser, ofaStreamFormat *format, GSList *lines )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_excel2002_v2_check";

	/*
	GSList *it;
	for( it=lines ; it ; it=it->next )
		g_debug( "%s", ( const gchar * ) it->data );
		*/

	return( parse_v1_check( self, parser, format, lines, thisfn ));
}

static GSList *
bourso_excel2002_v2_parse( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList *lines )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_bourso_excel2002_v2_parse";

	return( parse_v1_parse( self, parser, parms, lines, thisfn ));
}

/*
 * As of 2014- 6- 1:
 * -----------------
 * "*** Période : 01/01/2014 - 01/06/2014"
 * "*** Compte : 40618-80264-00040200033    -EUR "
 *
 * "DATE OPERATION"        "DATE VALEUR"   "LIBELLE"       "MONTANT"       "DEVISE"
 * " 02/01/2014"   " 02/01/2014"   "*PRLV Cotisat. Boursorama Protection 0  "      -00000000001,50 "EUR "
 * " 10/01/2014"   " 10/01/2014"   "TIP CFAB COMPTE REGLEMENT TI            "      -00000000220,02 "EUR "
 *
 * where spaces are tabulations
 *
 * NOTE
 * these definitions are only for consistancy
 * if bourso_excel95 format works fine on the input file, these
 * functions will never be called
 */
static gboolean
bourso_excel95_v1_check( const ofaImporterTxtBourso *self, const sParser *parser, ofaStreamFormat *format, GSList *lines )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_excel95_v1_check";

	return( parse_v1_check( self, parser, format, lines, thisfn ));
}

static GSList *
bourso_excel95_v1_parse( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList *lines )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_bourso_excel95_v1_parse";

	return( parse_v1_parse( self, parser, parms, lines, thisfn ));
}

static gboolean
parse_v1_check( const ofaImporterTxtBourso *self, const sParser *parser, ofaStreamFormat *format, GSList *lines, const gchar *thisfn )
{
	GSList *itl;
	gchar *sdbegin, *sdend;
	gchar *rib, *currency;
	gboolean ok;

	itl = lines;
	rib = NULL;
	currency = NULL;

	ok = lines ? parse_v1_header( format, &itl, &sdbegin, &sdend, &rib, &currency ) : FALSE;

	g_free( rib );
	g_free( currency );

	/* if the is ok, we are found to suppose that we
	 * have identified the input file */
	if( ok ){
		g_debug( "%s: nblines=%d", thisfn, g_slist_length( lines ));
	}

	return( ok );
}

static GSList *
parse_v1_parse( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList *lines, const gchar *thisfn )
{
	GSList *output, *itl, *follow;

	g_return_val_if_fail( lines, NULL );

	output = NULL;
	itl = lines;

	output = g_slist_prepend( output, parse_v1_header_to_fields( self, parser, parms, &itl ));

	follow = itl ? itl->next : NULL;
	for( itl=follow ; itl ; itl=itl->next ){
		output = g_slist_prepend( output, parse_v1_line_to_fields( self, parser, parms, itl->data ));
	}

	return( g_slist_reverse( output ));
}

/*
 * used by check
 */
static gboolean
parse_v1_header( ofaStreamFormat *format, GSList **lines, gchar **dbegin, gchar **dend, gchar **rib, gchar **currency )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_parse_v1_header";
	GSList *itl;
	const gchar *cstr;

	/* first line: "*** Période : dd/mm/yyyy - dd/mm/yyyy" */
	itl = *lines;
	if( !parse_v1_line_1(( const gchar * ) itl->data, dbegin, dend, format )){
		return( FALSE );
	}

	/* second line: "*** Compte : 40618-80264-00040200033    -EUR " */
	itl = itl ? itl->next : NULL;
	if( !parse_v1_line_2(( const gchar * ) itl->data, rib, currency, format )){
		return( FALSE );
	}

	/* third line is empty */
	itl = itl ? itl->next : NULL;
	cstr = itl ? ( const gchar * ) itl->data : NULL;
	if( my_strlen( cstr )){
		g_debug( "%s: third line is not empty: '%s' strlen=%ld", thisfn, cstr, my_strlen( cstr ));
		return( FALSE );
	}

	/* fourth line: "DATE OPERATION"        "DATE VALEUR"   "LIBELLE"       "MONTANT"       "DEVISE" */
	itl = itl ? itl->next : NULL;
	cstr = itl ? ( const gchar * ) itl->data : NULL;
	if( !g_ascii_strcasecmp( cstr, "\"DATE OPERATION\"        \"DATE VALEUR\"   \"LIBELLE\"       \"MONTANT\"       \"DEVISE\"" )){
		g_debug( "%s: fourth line not recognized: '%s'", thisfn, cstr );
		return( FALSE );
	}

	*lines = itl;

	return( TRUE );
}

/*
 * first line:
 * "*** Période : 01/11/2014 - 30/11/2014"
 *
 * We check the date validities because this same function is used
 * when checking if this importer is willing to import the file
 */
static gboolean
parse_v1_line_1( const gchar *line, gchar **dbegin, gchar **dend, ofaStreamFormat *format )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_parse_v1_line_1";
	gchar *found;
	GDate date;

	if( !g_str_has_prefix( line, "\"*** P" )){
		g_debug( "%s: no '***P' prefix", thisfn );
		return( FALSE );
	}
	found = g_strstr_len( line, -1, "riode : " );
	if( !found ){
		g_debug( "%s: no 'riode' sufix", thisfn );
		return( FALSE );
	}
	/* dd/mm/yyyy - dd/mm/yyyy */
	*dbegin = g_strstrip( g_strndup( found+8, 10 ));
	my_date_set_from_str( &date, *dbegin, ofa_stream_format_get_date_format( format ));
	if( !my_date_is_valid( &date )){
		g_debug( "%s: beginning date is not recognized: '%s'", thisfn, *dbegin );
		return( FALSE );
	}

	*dend = g_strstrip( g_strndup( found+21, 10 ));
	my_date_set_from_str( &date, *dbegin, ofa_stream_format_get_date_format( format ));
	if( !my_date_is_valid( &date )){
		g_debug( "%s: ending date is not recognized: '%s'", thisfn, *dend );
		return( FALSE );
	}

	return( TRUE );
}

/*
 * second line:
 * "*** Compte : 40618-80264-00040200033    -EUR "
 *
 * Also used by check.
 */
static gboolean
parse_v1_line_2( const gchar *line, gchar **rib, gchar **currency, ofaStreamFormat *format )
{
	static const gchar *thisfn = "ofa_importer_txt_bourso_parse_v1_line_2";
	gchar *found;

	*rib = NULL;
	*currency = NULL;

	if( !g_str_has_prefix( line, "\"*** Compte : " )){
		g_debug( "%s: no '***Compte' prefix", thisfn );
		return( FALSE );
	}

	found = g_strstr_len( line+38, -1, " -" );
	if( !found ){
		g_debug( "%s: tiret not found", thisfn );
		return( FALSE );
	}

	*rib = g_strstrip( g_strndup( line+14, 24 ));
	*currency = g_strstrip( g_strndup( found+2, 3 ));

	return( TRUE );
}

static GSList *
parse_v1_header_to_fields( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, GSList **lines )
{
	GSList *fields;
	gchar *rib, *currency, *sdbegin, *sdend;

	if( !parse_v1_header( parms->format, lines, &sdbegin, &sdend, &rib, &currency )){
		return( NULL );
	}

	fields = NULL;

	fields = g_slist_prepend( fields, g_strdup( "1" ));
	fields = g_slist_prepend( fields, g_strdup( "" ));				/* id placeholder */
	fields = g_slist_prepend( fields, g_strdup( parms->uri ));
	fields = g_slist_prepend( fields, g_strdup( parser->label ));
	fields = g_slist_prepend( fields, rib );
	fields = g_slist_prepend( fields, currency );
	fields = g_slist_prepend( fields, sdbegin );
	fields = g_slist_prepend( fields, g_strdup( "" ));				/* begin solde */
	fields = g_slist_prepend( fields, g_strdup( "N" ));
	fields = g_slist_prepend( fields, sdend );
	fields = g_slist_prepend( fields, g_strdup( "" ));				/* end solde */
	fields = g_slist_prepend( fields, g_strdup( "N" ));

	return( g_slist_reverse( fields ));
}

static GSList *
parse_v1_line_to_fields( ofaImporterTxtBourso *self, const sParser *parser, ofsImporterParms *parms, const gchar *line )
{
	GSList *output, *fields, *itf;
	const gchar *cstr;
	gchar *label, *currency, *sdope, *sdeffect, *samount;

	output = NULL;

	if( my_strlen( line )){
		fields = split_by_field( line, parms->format );

		/* operation date */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		sdope = g_strdup( cstr );

		/* effect date */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		sdeffect = g_strdup( cstr );

		/* label */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		label = g_strstrip( g_strdup( cstr ));

		/* amount */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		samount = g_strdup( cstr );

		/* currency */
		itf = itf ? itf->next : NULL;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		currency = g_strstrip( g_strdup( cstr ));

		output = g_slist_prepend( output, g_strdup( "2" ));
		output = g_slist_prepend( output, g_strdup( "" ));			/* id placeholder */
		output = g_slist_prepend( output, sdope );
		output = g_slist_prepend( output, sdeffect );
		output = g_slist_prepend( output, g_strdup( "" ));			/* reference */
		output = g_slist_prepend( output, label );
		output = g_slist_prepend( output, samount );
		output = g_slist_prepend( output, currency );
	}

	return( g_slist_reverse( output ));
}

static sParser *
get_willing_to_parser( const ofaImporterTxtBourso *self, ofaStreamFormat *format, GSList *lines )
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
get_default_stream_format( const ofaImporterTxtBourso *self, ofaHub *hub )
{
	ofaStreamFormat *format;

	format = ofa_stream_format_new( hub, NULL, OFA_SFMODE_IMPORT );

	ofa_stream_format_set( format,
			TRUE,  "ISO-8859-15",			/* Western Europe */
			TRUE,  MY_DATE_DMYY,			/* date format dd/mm/yyyy */
			FALSE, MY_CHAR_ZERO,			/* no thousand sep */
			TRUE,  MY_CHAR_DOT,				/* dot decimal sep */
			TRUE,  MY_CHAR_TAB,				/* tab field sep */
			TRUE,  MY_CHAR_DQUOTE,			/* double quote string delim */
			0 );							/* no header */

	return( format );
}

/*
 * Returns a GSList of fields.
 */
static GSList *
split_by_field( const gchar *line, ofaStreamFormat *settings )
{
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
