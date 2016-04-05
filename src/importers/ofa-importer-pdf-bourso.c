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
#include <math.h>
#include <poppler.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-iident.h"
#include "my/my-iprogress.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"

#include "importers/ofa-importer-pdf-bourso.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* bat datas have to be read both from first and last pages
	 */
	gchar    *iban;
	gchar    *currency;
	GDate     begin_date;
	GDate     end_date;
	ofxAmount begin_amount;
}
	ofaImporterPdfBoursoPrivate;

typedef struct _sParser                    sParser;

#define IMPORTER_CANON_NAME               "Boursorama.pdf importer"
#define IMPORTER_VERSION                  "2015.2"

/* a data structure to host head detail line datas
 */
typedef struct {
	GDate     dope;
	gchar    *slabel;
	GDate     deffect;
	ofxAmount amount;
	gdouble   y;
}
	sLine;

static gdouble  st_x1_periode_begin     = 259;
static gdouble  st_y1_periode_begin     = 267;
static gchar   *st_header_extrait       = "Extrait de votre compte en ";
static gchar   *st_header_iban          = "I.B.A.N. ";
static gchar   *st_header_begin_solde   = "SOLDE AU : ";
static gchar   *st_footer_end_solde     = "Nouveau solde en ";
static gchar   *st_page_credit          = "Crédit";

static gdouble  st_label_min_x          = 80;
static gdouble  st_valeur_min_x         = 300;
static gdouble  st_debit_min_x          = 355;
static gdouble  st_credit_min_x         = 446;

static GList   *st_accepted_contents    = NULL;

static void             iident_iface_init( myIIdentInterface *iface );
static gchar           *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar           *iident_get_version( const myIIdent *instance, void *user_data );
static void             iimporter_iface_init( ofaIImporterInterface *iface );
static const GList     *iimporter_get_accepted_contents( const ofaIImporter *instance );
static gboolean         iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type );
static gboolean         is_willing_to_parse( const ofaImporterPdfBourso *self, const gchar *uri );
static GSList          *iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr );
static GSList          *do_parse( ofaImporterPdfBourso *self, ofsImporterParms *parms, gchar **msgerr );
static gboolean         bourso_pdf_v1_check( const ofaImporterPdfBourso *self, const sParser *parser, const ofaStreamFormat *format, const gchar *uri );
static GSList          *bourso_pdf_v1_parse( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms );
static gboolean         bourso_pdf_v1_parse_header_first( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list );
static gboolean         bourso_pdf_v1_parse_header_last( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list, GSList **fields );
static GList           *bourso_pdf_v1_parse_lines_rough( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, guint page_num, GList *rc_list );
static GList           *bourso_pdf_v1_parse_lines_merge( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *rough_list );
static GSList          *bourso_pdf_v1_parse_lines_build( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *filtered_list );
static sParser         *get_willing_to_parser( const ofaImporterPdfBourso *self, const ofaStreamFormat *format, const gchar *uri );
static ofaStreamFormat *get_default_stream_format( const ofaImporterPdfBourso *self );
static sLine           *find_line( GList **lines, gdouble acceptable_diff, gdouble y );
static void             free_line( sLine *line );

G_DEFINE_TYPE_EXTENDED( ofaImporterPdfBourso, ofa_importer_pdf_bourso, OFA_TYPE_IMPORTER_PDF, 0,
		G_ADD_PRIVATE( ofaImporterPdfBourso )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

/* A description of the import functions we are able to manage here.
 * If several versions happen to be managed, then the most recent
 * should be set first.
 */
struct _sParser {
	const gchar *label;
	guint        version;
	gboolean   (*fnTest) ( const ofaImporterPdfBourso *, const sParser *, const ofaStreamFormat *, const gchar * );
	GSList *   (*fnParse)( ofaImporterPdfBourso *, const sParser *, ofsImporterParms * );
};

static sParser st_parsers[] = {
		{ "Boursorama-PDF v1.2015", 1, bourso_pdf_v1_check, bourso_pdf_v1_parse },
		{ 0 }
};

static void
importer_pdf_bourso_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_pdf_bourso_finalize";
	ofaImporterPdfBoursoPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_PDF_BOURSO( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	priv = ofa_importer_pdf_bourso_get_instance_private( OFA_IMPORTER_PDF_BOURSO( instance ));

	g_free( priv->iban );
	g_free( priv->currency );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_pdf_bourso_parent_class )->finalize( instance );
}

static void
importer_pdf_bourso_dispose( GObject *instance )
{
	ofaImporterPdfBoursoPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_PDF_BOURSO( instance ));

	priv = ofa_importer_pdf_bourso_get_instance_private( OFA_IMPORTER_PDF_BOURSO( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_pdf_bourso_parent_class )->dispose( instance );
}

static void
ofa_importer_pdf_bourso_init( ofaImporterPdfBourso *self )
{
	static const gchar *thisfn = "ofa_importer_pdf_bourso_init";
	ofaImporterPdfBoursoPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_PDF_BOURSO( self ));

	priv = ofa_importer_pdf_bourso_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_pdf_bourso_class_init( ofaImporterPdfBoursoClass *klass )
{
	static const gchar *thisfn = "ofa_importer_pdf_bourso_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_pdf_bourso_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_pdf_bourso_finalize;
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
	iface->parse = iimporter_parse;
}

static const GList *
iimporter_get_accepted_contents( const ofaIImporter *instance )
{
	if( !st_accepted_contents ){
		st_accepted_contents = g_list_prepend( NULL, "application/pdf" );
	}

	return( st_accepted_contents );
}

static gboolean
iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type )
{
	gboolean ok;

	ok = ofa_importer_pdf_is_willing_to( OFA_IMPORTER_PDF( instance ), uri, iimporter_get_accepted_contents( instance )) &&
			type == OFO_TYPE_BAT &&
			is_willing_to_parse( OFA_IMPORTER_PDF_BOURSO( instance ), uri );

	return( ok );
}

/*
 * do the minimum to identify the file
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
is_willing_to_parse( const ofaImporterPdfBourso *self, const gchar *uri )
{
	ofaStreamFormat *format;
	sParser *parser;

	format = get_default_stream_format( self );

	parser = get_willing_to_parser( self, format, uri );

	g_object_unref( format );

	return( parser != NULL );
}

static GSList *
iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr )
{
	g_return_val_if_fail( parms->hub && OFA_IS_HUB( parms->hub ), NULL );
	g_return_val_if_fail( my_strlen( parms->uri ), NULL );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), NULL );

	return( do_parse( OFA_IMPORTER_PDF_BOURSO( instance ), parms, msgerr ));
}

static GSList *
do_parse( ofaImporterPdfBourso *self, ofsImporterParms *parms, gchar **msgerr )
{
	GSList *output;
	sParser *parser;

	if( msgerr ){
		*msgerr = NULL;
	}

	output = NULL;
	parser = get_willing_to_parser( self, parms->format, parms->uri );

	if( parser ){
		output = parser->fnParse( self, parser, parms );
	}

	return( output );
}

static gboolean
bourso_pdf_v1_check( const ofaImporterPdfBourso *self, const sParser *parser, const ofaStreamFormat *format, const gchar *uri )
{
	PopplerDocument *doc;
	PopplerPage *page;
	GError *error;
	gchar *text, *found;
	gboolean ok;

	ok = FALSE;
	error = NULL;
	doc = poppler_document_new_from_file( uri, NULL, &error );
	if( !doc ){
		g_error_free( error );
		return( ok );
	}

	page = poppler_document_get_page( doc, 0 );
	text = poppler_page_get_text( page );
	found = g_strstr_len( text, -1, st_header_extrait );
	if( found ){
		found = g_strstr_len( text, -1, "BOURSORAMA" );
		if( found ){
			ok = TRUE;
		}
	}

	g_free( text );
	g_object_unref( page );
	g_object_unref( doc );

	return( ok );
}

static GSList *
bourso_pdf_v1_parse( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms )
{
	GSList *output, *fields;
	PopplerDocument *doc;
	PopplerPage *page;
	gint pages_count;
	guint page_num;
	GError *error;
	GList *rc_list, *lines1, *lines2;
	gboolean ok;

	output = NULL;
	error = NULL;

	doc = poppler_document_new_from_file( parms->uri, NULL, &error );
	if( !doc ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, error->message );
		}
		g_error_free( error );
		return( NULL );
	}

	pages_count = poppler_document_get_n_pages( doc );

	/* get the bat datas from first and last page
	 */
	page = poppler_document_get_page( doc, 0 );
	rc_list = ofa_importer_pdf_get_layout( OFA_IMPORTER_PDF( self ), page );
	g_object_unref( page );
	ok = bourso_pdf_v1_parse_header_first( self, parser, parms, rc_list );
	ofa_importer_pdf_free_layout( rc_list );
	if( !ok ){
		return( NULL );
	}

	page = poppler_document_get_page( doc, pages_count-1 );
	rc_list = ofa_importer_pdf_get_layout( OFA_IMPORTER_PDF( self ), page );
	g_object_unref( page );
	ok = bourso_pdf_v1_parse_header_last( self, parser, parms, rc_list, &fields );
	ofa_importer_pdf_free_layout( rc_list );
	if( !ok ){
		return( NULL );
	}
	output = g_slist_prepend( output, fields );

	/* then get the lines from bat
	 */
	for( page_num=0 ; page_num < pages_count ; ++page_num ){
		page = poppler_document_get_page( doc, page_num );
		rc_list = ofa_importer_pdf_get_layout( OFA_IMPORTER_PDF( self ), page );
		g_object_unref( page );
		lines1 = bourso_pdf_v1_parse_lines_rough( self, parser, parms, page_num, rc_list );
		ofa_importer_pdf_free_layout( rc_list );
		lines2 = bourso_pdf_v1_parse_lines_merge( self, parser, parms, lines1 );
		g_list_free_full( lines1, ( GDestroyNotify ) free_line );
		output = g_slist_concat( output, bourso_pdf_v1_parse_lines_build( self, parser, parms, lines2 ));
		g_list_free_full( lines2, ( GDestroyNotify ) free_line );
	}

	g_object_unref( doc );
#if 0
	ofa_iimportable_set_count( OFA_IIMPORTABLE( importer ), priv->count );

	/* check the totals: this make sure we have all lines with right amounts */
	if( bat && ( priv->tot_debit || priv->tot_credit )){
		debit = 0;
		credit = 0;
		for( it=bat->details ; it ; it=it->next ){
			detail = ( ofsBatDetail *  ) it->data;
			if( detail->amount < 0 ){
				debit += -1*detail->amount;
			} else {
				credit += detail->amount;
			}
		}

		if( bat->begin_solde < 0 ){
			debit += -1*bat->begin_solde;
		} else {
			credit += bat->begin_solde;
		}

		sdebit = ofa_amount_to_str( priv->tot_debit, NULL );
		scredit = ofa_amount_to_str( priv->tot_credit, NULL );
		msg = g_strdup_printf( "Bank debit=%s, bank credit=%s", sdebit, scredit );
		ofa_iimportable_set_message(
				OFA_IIMPORTABLE( importer ), priv->count, IMPORTABLE_MSG_STANDARD, msg );
		g_free( msg );
		g_free( scredit );
		g_free( sdebit );

		if( debit == priv->tot_debit && credit == priv->tot_credit ){
			ofa_iimportable_set_message(
					OFA_IIMPORTABLE( importer ), priv->count, IMPORTABLE_MSG_STANDARD,
					_( "All lines successfully imported" ));

		} else {
			if( debit != priv->tot_debit ){
				sdebit = ofa_amount_to_str( debit, NULL );
				msg = g_strdup_printf( _( "Error detected: computed debit=%s" ), sdebit );
				ofa_iimportable_set_message(
						OFA_IIMPORTABLE( importer ), priv->count, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( sdebit );
			}
			if( credit != priv->tot_credit ){
				scredit = ofa_amount_to_str( credit, NULL );
				msg = g_strdup_printf( _( "Error detected: computed credit=%s" ), scredit );
				ofa_iimportable_set_message(
						OFA_IIMPORTABLE( importer ), priv->count, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( scredit );
			}
		}
	}
#endif

	return( g_slist_reverse( output ));
}

/*
 * parse the first page to get some datas:
 * - begin and end dates
 * - iban
 * - currency
 * - begin solde
 */
static gboolean
bourso_pdf_v1_parse_header_first( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list )
{
	ofaImporterPdfBoursoPrivate *priv;
	gdouble acceptable_diff, periode_x1, periode_y1;
	GList *it, *it_next;
	gboolean ok, begin_found, end_found, extrait_found, iban_found, begin_solde_found;
	ofsPdfRC *rc, *rc_next;

	priv = ofa_importer_pdf_bourso_get_instance_private( self );

	ok = TRUE;
	my_date_clear( &priv->begin_date );
	my_date_clear( &priv->end_date );
	begin_found = FALSE;
	end_found = FALSE;
	iban_found = FALSE;
	begin_solde_found = FALSE;

	acceptable_diff = ofa_importer_pdf_get_acceptable_diff();
	periode_x1 = st_x1_periode_begin - 10*acceptable_diff;
	periode_y1 = st_y1_periode_begin - 10*acceptable_diff;

	for( it=rc_list ; it ; it=it->next ){
		rc = ( ofsPdfRC * ) it->data;

		if( !extrait_found ){
			if( g_str_has_prefix( rc->text, st_header_extrait )){
				priv->currency = g_strdup( rc->text+my_strlen( st_header_extrait ));
				extrait_found = TRUE;
			}
		}

		if( !begin_found && rc->x1 > periode_x1 && rc->y1 > periode_y1 ){
			if( !my_collate( rc->text, "du" )){
				it = it->next;
				rc = ( ofsPdfRC * ) it->data;
				my_date_set_from_str( &priv->begin_date, rc->text, ofa_stream_format_get_date_format( parms->format ));
				begin_found = TRUE;
			}
		}

		if( begin_found && !end_found && rc->x1 > periode_x1 && rc->y1 > periode_y1 ){
			if( !my_collate( rc->text, "au" )){
				it = it->next;
				rc = ( ofsPdfRC * ) it->data;
				my_date_set_from_str( &priv->end_date, rc->text, ofa_stream_format_get_date_format( parms->format ));
				end_found = TRUE;
			}
		}

		if( !iban_found ){
			if( g_str_has_prefix( rc->text, st_header_iban )){
				priv->iban = g_strdup( rc->text+my_strlen( st_header_iban ));
				iban_found = TRUE;
			}
		}

		if( !begin_solde_found && g_str_has_prefix( rc->text, st_header_begin_solde )){
			it_next = it->next;
			rc_next = ( ofsPdfRC * ) it_next->data;
			priv->begin_amount = my_double_set_from_str( rc->text,
											ofa_stream_format_get_thousand_sep( parms->format ),
											ofa_stream_format_get_decimal_sep( parms->format ));
			if( rc_next->x1 < st_credit_min_x ){
				priv->begin_amount *= -1;
			}
			begin_solde_found = TRUE;
		}
	}

	if( !begin_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, _( "beginning date not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}
	if( !end_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, _( "ending date not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}
	if( !iban_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, _( "IBAN not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}
	if( !begin_solde_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, _( "beginning solde not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}

	return( ok );
}

/*
 * parse the last page to get the end solde
 * + if ok, setup the list of fields
 */
static gboolean
bourso_pdf_v1_parse_header_last( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list, GSList **fields )
{
	ofaImporterPdfBoursoPrivate *priv;
	gboolean ok;
	GList *it;
	ofsPdfRC *rc;
	gdouble acceptable_diff, y1;
	gboolean solde_label_found, end_solde_found;
	ofxAmount end_amount;
	gchar *sbegin_date, *send_date, *sbegin_solde, *send_solde;

	g_return_val_if_fail( fields, FALSE );

	priv = ofa_importer_pdf_bourso_get_instance_private( self );

	ok = TRUE;
	*fields = NULL;
	solde_label_found = FALSE;
	end_solde_found = FALSE;
	acceptable_diff = ofa_importer_pdf_get_acceptable_diff();

	for( it=rc_list ; it ; it=it->next ){
		rc = ( ofsPdfRC * ) it->data;

		/* search for the end line and get y coordinates */
		if( !solde_label_found && !my_collate( rc->text, st_footer_end_solde )){
			y1 = rc->y1 -2*acceptable_diff;
			solde_label_found = TRUE;
		}

		if( solde_label_found && !end_solde_found && fabs( rc->y1 - y1 ) < acceptable_diff && rc->x1 > st_debit_min_x ){
			end_amount = my_double_set_from_str( rc->text,
											ofa_stream_format_get_thousand_sep( parms->format ),
											ofa_stream_format_get_decimal_sep( parms->format ));
			if( rc->x1 < st_credit_min_x ){
				end_amount *= -1;
			}
			end_solde_found = TRUE;
		}
	}

	if( !end_solde_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, _( "ending solde not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;

	} else {
		sbegin_date = my_date_to_str( &priv->begin_date, MY_DATE_SQL );
		send_date = my_date_to_str( &priv->end_date, MY_DATE_SQL );
		sbegin_solde = my_double_to_sql( priv->begin_amount );
		send_solde = my_double_to_sql( end_amount );

		*fields = g_slist_prepend( *fields, g_strdup( "1" ));
		*fields = g_slist_prepend( *fields, g_strdup( parms->uri ));
		*fields = g_slist_prepend( *fields, g_strdup( parser->label ));
		*fields = g_slist_prepend( *fields, g_strdup( priv->iban ));
		*fields = g_slist_prepend( *fields, g_strdup( priv->currency ));
		*fields = g_slist_prepend( *fields, sbegin_date );
		*fields = g_slist_prepend( *fields, sbegin_solde );
		*fields = g_slist_prepend( *fields, g_strdup( "Y" ));
		*fields = g_slist_prepend( *fields, send_date );
		*fields = g_slist_prepend( *fields, send_solde );
		*fields = g_slist_prepend( *fields, g_strdup( "Y" ));
	}

	return( ok );
}

/*
 * Almost all header informations are found on the beginning of first page
 * (i.e. all but the ending solde which is on the last page)
 * We "just" have to scan all layout rectangles to find the desired datas.
 *
 * Returns: a GList of sLine structure, one for each parsed line
 * (as a consequence, some of them will have to be merged later)
 */
static GList *
bourso_pdf_v1_parse_lines_rough( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, guint page_num, GList *rc_list )
{
	GList *it, *lines;
	ofsPdfRC *rc;
	gdouble acceptable_diff, first_y;
	sLine *line;
	gchar *tmp, *str;

	lines = NULL;
	first_y = 0;
	acceptable_diff = ofa_importer_pdf_get_acceptable_diff();

	for( it=rc_list ; it ; it=it->next ){
		rc = ( ofsPdfRC * ) it->data;

		/* do not do anything while we do not have found the begin of
		 * the array - which is 'SOLDE AU : ' for page zero
		 * or 'Crédit' for others */
		if( first_y == 0 ){
			if( page_num == 0 && g_str_has_prefix( rc->text, st_header_begin_solde ) && rc->x2 < st_debit_min_x ){
				first_y = rc->y2 + acceptable_diff;
			}
			if( page_num > 0 && g_str_has_prefix( rc->text, st_page_credit ) && rc->x2 < st_debit_min_x ){
				first_y = rc->y2 + acceptable_diff;
			}
		}

		if( first_y > 0 && rc->y1 > first_y ){
			/* end of the page */
			if( g_str_has_prefix( rc->text, st_footer_end_solde )){
				break;
			}

			/* a transaction field */
			line = find_line( &lines, acceptable_diff, rc->y1 );

			if( rc->x1 < st_label_min_x ){
				tmp = g_strndup( rc->text, 10 );
				my_date_set_from_str( &line->dope, tmp, ofa_stream_format_get_date_format( parms->format ));
				g_free( tmp );
				if( my_strlen( rc->text ) > 10 ){
					g_free( line->slabel );
					line->slabel = g_strstrip( g_strdup( rc->text+10 ));
				}

			} else if( rc->x1 < st_valeur_min_x ){
				tmp = g_strstrip( g_strdup( rc->text ));
				if( my_strlen( line->slabel )){
					str = g_strconcat( line->slabel, " ", tmp, NULL );
				} else {
					str = g_strdup( tmp );
				}
				g_free( tmp );
				g_free( line->slabel );
				line->slabel = str;

			} else if( rc->x1 < st_debit_min_x ){
				my_date_set_from_str( &line->deffect, rc->text, ofa_stream_format_get_date_format( parms->format ));

			} else {
				line->amount = my_double_set_from_str( rc->text,
												ofa_stream_format_get_thousand_sep( parms->format ),
												ofa_stream_format_get_decimal_sep( parms->format ));
				if( rc->x1 < st_credit_min_x ){
					line->amount *= -1;
				}
			}
		}
	}

	return( lines );
}

/*
 * merge
 */
static GList *
bourso_pdf_v1_parse_lines_merge( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *rough_list )
{
	static const gchar *thisfn = "ofa_importer_pdf_bourso_pdf_v1_parse_lines_merge";
	GList *it, *lines;
	gdouble prev_y;
	sLine *line, *outline, *prev_line;
	gchar *str, *sdope, *sdeffect;

	lines = NULL;
	prev_line = NULL;
	prev_y = 0;

	/* we have all transaction lines, with each field normally set at
	 *  its place - but we have yet to filter some useless lines
	 * We have either:
	 * - a complete line with dope, label, deffect and amount
	 * - or line with only label, which completes the previous line's label */
	for( it=rough_list ; it ; it=it->next ){
		line = ( sLine * ) it->data;

		if( my_date_is_valid( &line->dope )){

			if( !my_date_is_valid( &line->deffect ) || line->amount == 0 ){

				sdope = my_date_to_str( &line->dope, MY_DATE_SQL );
				sdeffect = my_date_to_str( &line->deffect, MY_DATE_SQL );
				str = g_strdup_printf( _( "invalid line: operation=%s, label=%s, value=%s, amount=%lf" ),
						sdope, line->slabel, sdeffect, line->amount );
				if( parms->progress ){
					my_iprogress_set_text( parms->progress, self, str );
				} else {
					g_debug( "%s: %s", thisfn, str );
				}
				g_free( str );
				g_free( sdope );
				g_free( sdeffect );

				parms->parse_errs += 1;
				if( parms->stop ){
					break;
				} else {
					continue;
				}
			}

			outline = g_new0( sLine, 1 );
			my_date_set_from_date( &outline->dope, &line->dope );
			my_date_set_from_date( &outline->deffect, &line->deffect );
			outline->slabel = g_strdup( line->slabel );
			outline->amount = line->amount;
			prev_line = outline;
			prev_y = line->y;
			lines = g_list_prepend( lines, prev_line );

		} else if( my_date_is_valid( &line->deffect ) || line->amount != 0 || line->y - prev_y > 25){

				sdope = my_date_to_str( &line->dope, MY_DATE_SQL );
				sdeffect = my_date_to_str( &line->deffect, MY_DATE_SQL );
				str = g_strdup_printf( _( "invalid line: operation=%s, label=%s, value=%s, amount=%lf" ),
						sdope, line->slabel, sdeffect, line->amount );
				if( parms->progress ){
					my_iprogress_set_text( parms->progress, self, str );
				} else {
					g_debug( "%s: %s", thisfn, str );
				}
				g_free( str );
				g_free( sdope );
				g_free( sdeffect );

				parms->parse_errs += 1;
				if( parms->stop ){
					break;
				} else {
					continue;
				}
		} else {
			str = g_strdup_printf( "%s / %s", prev_line->slabel, line->slabel );
			g_free( prev_line->slabel );
			prev_line->slabel = str;
		}
	}

	return( lines );
}

static GSList *
bourso_pdf_v1_parse_lines_build( ofaImporterPdfBourso *self, const sParser *parser, ofsImporterParms *parms, GList *filtered_list )
{
	GSList *output, *fields;
	GList *it;
	sLine *line;
	gchar *sdope, *sdeffect, *samount;

	output = NULL;

	/* last build the detail lines */
	for( it=filtered_list ; it ; it=it->next ){
		line = ( sLine * ) it->data;

		fields = NULL;
		sdope = my_date_to_str( &line->dope, MY_DATE_SQL );
		sdeffect = my_date_to_str( &line->deffect, MY_DATE_SQL );
		samount = my_double_to_sql( line->amount );

		fields = g_slist_prepend( fields, g_strdup( "2" ));
		fields = g_slist_prepend( fields, sdope );
		fields = g_slist_prepend( fields, sdeffect );
		fields = g_slist_prepend( fields, g_strdup( "" ));
		fields = g_slist_prepend( fields, g_strdup( line->slabel ));
		fields = g_slist_prepend( fields, samount );
		fields = g_slist_prepend( fields, g_strdup( "" ));

		output = g_slist_prepend( output, g_slist_reverse( fields ));
	}

	return( output );
}

static sParser *
get_willing_to_parser( const ofaImporterPdfBourso *self, const ofaStreamFormat *format, const gchar *uri )
{
	sParser *parser;
	gint i;

	parser = NULL;

	for( i=0 ; st_parsers[i].label ; ++i ){
		if( st_parsers[i].fnTest( self, &st_parsers[i], format, uri )){
			parser = &st_parsers[i];
			break;
		}
	}

	return( parser );
}

static ofaStreamFormat *
get_default_stream_format( const ofaImporterPdfBourso *self )
{
	ofaStreamFormat *format;

	format = ofa_stream_format_new( NULL, OFA_SFMODE_IMPORT );

	ofa_stream_format_set( format,
			TRUE,  "ISO-8859-15",			/* Western Europe */
			TRUE,  MY_DATE_DMYY,			/* date format dd/mm/yyyy */
			FALSE, '\0',					/* no thousand sep */
			TRUE,  '.',						/* dot decimal sep */
			TRUE,  '\t',					/* tab field sep */
			TRUE,  '"',						/* double quote string delim */
			FALSE, 0 );						/* no header */

	return( format );
}

/*
 * find the sLine structure for the specified rc
 * allocated a new one if needed
 */
static sLine *
find_line( GList **lines, gdouble acceptable_diff, gdouble y )
{
	GList *it;
	sLine *line;

	for( it=( *lines ) ; it ; it=it->next ){
		line = ( sLine * ) it->data;
		if( fabs( line->y - y ) <= acceptable_diff ){
			return( line );
		}
	}

	line = g_new0( sLine, 1 );
	line->y = y;
	*lines = g_list_append( *lines, line );

	return( line );
}

static void
free_line( sLine *line )
{
	g_free( line->slabel );

	g_free( line );
}
