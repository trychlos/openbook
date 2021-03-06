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
#include <math.h>
#include <poppler.h>

#include "my/my-char.h"
#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-iident.h"
#include "my/my-iprogress.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"

#include "importers/ofa-importer-pdf-lcl.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;
}
	ofaImporterPdfLclPrivate;

typedef struct _sParser                    sParser;

#define IMPORTER_CANON_NAME               "LCL-PDF Importer"
#define IMPORTER_VERSION                  "3.1-2021"

/* a data structure to host head detail line datas
 */
typedef struct {
	gchar    *dope;
	gchar    *label;
	gchar    *deffect;
	gchar    *amount;
	gdouble   y;
	gint      page_num;
}
	sLine;

static gchar   *st_header_extrait       = "RELEVE DE COMPTE";
static gchar   *st_header_banque        = "CREDIT LYONNAIS";
static gchar   *st_header_iban          = "IBAN : ";
static gchar   *st_header_ancien_solde   = "ANCIEN SOLDE";
static gchar   *st_footer_end_solde     = "SOLDE EN EUROS";
static gchar   *st_page_credit          = "CREDIT";
static gchar   *st_page_totaux          = "TOTAUX";
static gchar   *st_page_solde_intermed  = "SOLDE INTERMEDIAIRE A";
static gchar   *st_page_recapitulatif   = "Récapitulatif des frais perçus";

static gdouble  st_label_min_x          = 74;
static gdouble  st_valeur_min_x         = 360;
static gdouble  st_debit_min_x          = 409;
static gdouble  st_credit_min_x         = 482;
static gdouble  st_detail_max_x         = 555;
static gdouble  st_detail_max_y         = 820;

static GList   *st_accepted_contents    = NULL;

#define DEBUG_PARSE_PDFRC                FALSE		// lcl_pdf_v1_parse(): dump full ofsPdfRC layout
#define DEBUG_PARSE_ROUGH                TRUE		// lcl_pdf_v1_parse(): dump rough lines
#define DEBUG_ROUGH_PARSE                FALSE		// lcl_pdf_v1_parse_rough(): dump ofsPdfRC parsing

static void             iident_iface_init( myIIdentInterface *iface );
static gchar           *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar           *iident_get_version( const myIIdent *instance, void *user_data );
static void             iimporter_iface_init( ofaIImporterInterface *iface );
static const GList     *iimporter_get_accepted_contents( const ofaIImporter *instance, ofaIGetter *getter );
static gboolean         iimporter_is_willing_to( const ofaIImporter *instance, ofaIGetter *getter, const gchar *uri, GType type );
static gboolean         is_willing_to_parse( const ofaImporterPdfLcl *self, ofaIGetter *getter, const gchar *uri );
static ofaStreamFormat *iimporter_get_default_format( const ofaIImporter *instance, ofaIGetter *getter, gboolean *is_updatable );
static GSList          *iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr );
static GSList          *do_parse( ofaImporterPdfLcl *self, ofsImporterParms *parms, gchar **msgerr );
static gboolean         lcl_pdf_v1_check( const ofaImporterPdfLcl *self, const sParser *parser, const ofaStreamFormat *format, const gchar *uri );
static GSList          *lcl_pdf_v1_parse( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms );
static GSList          *lcl_pdf_v1_parse_header( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list );
static GList           *lcl_pdf_v1_parse_rough( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list );
static GList           *lcl_pdf_v1_parse_merge( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *rough_list );
static GSList          *lcl_pdf_v1_parse_lines_build( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *filtered_list );
static sParser         *get_willing_to_parser( const ofaImporterPdfLcl *self, const ofaStreamFormat *format, const gchar *uri );
static ofaStreamFormat *get_default_stream_format( const ofaImporterPdfLcl *self, ofaIGetter *getter );
static sLine           *find_line( GList **lines, gdouble acceptable_diff, ofsPdfRC *rc );
static void             dump_line_list( GList *lines, const gchar *label );
static void             dump_line( sLine *line, const gchar *label );
static void             free_line( sLine *line );
static gchar           *get_amount( ofsPdfRC *rc );

G_DEFINE_TYPE_EXTENDED( ofaImporterPdfLcl, ofa_importer_pdf_lcl, OFA_TYPE_IMPORTER_PDF, 0,
		G_ADD_PRIVATE( ofaImporterPdfLcl )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

/* A description of the import functions we are able to manage here.
 * If several versions happen to be managed, then the most recent
 * should be set first.
 */
struct _sParser {
	const gchar *label;
	guint        version;
	gboolean   (*fnTest) ( const ofaImporterPdfLcl *, const sParser *, const ofaStreamFormat *, const gchar * );
	GSList *   (*fnParse)( ofaImporterPdfLcl *, const sParser *, ofsImporterParms * );
};

static sParser st_parsers[] = {
		{ "LCL-PDF Importer v3.1-2021", 1, lcl_pdf_v1_check, lcl_pdf_v1_parse },
		{ 0 }
};

static void
importer_pdf_lcl_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_finalize";

	g_return_if_fail( instance && OFA_IS_IMPORTER_PDF_LCL( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_pdf_lcl_parent_class )->finalize( instance );
}

static void
importer_pdf_lcl_dispose( GObject *instance )
{
	ofaImporterPdfLclPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_PDF_LCL( instance ));

	priv = ofa_importer_pdf_lcl_get_instance_private( OFA_IMPORTER_PDF_LCL( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_pdf_lcl_parent_class )->dispose( instance );
}

static void
ofa_importer_pdf_lcl_init( ofaImporterPdfLcl *self )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_init";
	ofaImporterPdfLclPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_PDF_LCL( self ));

	priv = ofa_importer_pdf_lcl_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_pdf_lcl_class_init( ofaImporterPdfLclClass *klass )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_pdf_lcl_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_pdf_lcl_finalize;
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
iimporter_get_accepted_contents( const ofaIImporter *instance, ofaIGetter *getter )
{
	if( !st_accepted_contents ){
		st_accepted_contents = g_list_prepend( NULL, "application/pdf" );
	}

	return( st_accepted_contents );
}

static gboolean
iimporter_is_willing_to( const ofaIImporter *instance, ofaIGetter *getter, const gchar *uri, GType type )
{
	gboolean ok;

	ok = ofa_importer_pdf_is_willing_to( OFA_IMPORTER_PDF( instance ), getter, uri, iimporter_get_accepted_contents( instance, getter )) &&
			type == OFO_TYPE_BAT &&
			is_willing_to_parse( OFA_IMPORTER_PDF_LCL( instance ), getter, uri );

	return( ok );
}

/*
 * do the minimum to identify the file
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
is_willing_to_parse( const ofaImporterPdfLcl *self, ofaIGetter *getter, const gchar *uri )
{
	ofaStreamFormat *format;
	sParser *parser;

	format = get_default_stream_format( self, getter );

	parser = get_willing_to_parser( self, format, uri );

	g_object_unref( format );

	return( parser != NULL );
}

static ofaStreamFormat *
iimporter_get_default_format( const ofaIImporter *instance, ofaIGetter *getter, gboolean *updatable )
{
	ofaStreamFormat *format;

	format = get_default_stream_format( OFA_IMPORTER_PDF_LCL( instance ), getter );

	if( updatable ){
		*updatable = FALSE;
	}

	return( format );
}

static GSList *
iimporter_parse( ofaIImporter *instance, ofsImporterParms *parms, gchar **msgerr )
{
	g_return_val_if_fail( parms->getter && OFA_IS_IGETTER( parms->getter ), NULL );
	g_return_val_if_fail( my_strlen( parms->uri ), NULL );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), NULL );

	return( do_parse( OFA_IMPORTER_PDF_LCL( instance ), parms, msgerr ));
}

static GSList *
do_parse( ofaImporterPdfLcl *self, ofsImporterParms *parms, gchar **msgerr )
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
lcl_pdf_v1_check( const ofaImporterPdfLcl *self, const sParser *parser, const ofaStreamFormat *format, const gchar *uri )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_v1_check";
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
		found = g_strstr_len( text, -1, st_header_banque );
		if( found ){
			ok = TRUE;
		} else {
			g_debug( "%s: '%s' not found", thisfn, st_header_banque );
		}
	} else {
		g_debug( "%s: '%s' not found", thisfn, st_header_extrait );
	}

	g_free( text );
	g_object_unref( page );
	g_object_unref( doc );

	return( ok );
}

/*
 * v3.1-2021
 * 	 Starting with "Relevé de compte du 02.10.2018 au 31.10.2018 - N° 167"
 *   LCL adds to the left right corner of the first page some sort of template reference 'K6EXTP23'
 *   Have to ignore this ref.
 */
static GSList *
lcl_pdf_v1_parse( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_lcl_pdf_v1_parse";
	GSList *output;
	PopplerDocument *doc;
	GError *error;
	GList *rc_list, *it, *lines1, *lines2;

	output = NULL;
	error = NULL;

	doc = poppler_document_new_from_file( parms->uri, NULL, &error );
	if( !doc ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, error->message );
		}
		g_error_free( error );
		return( NULL );
	}

	/* get the full layout once for the whole document */
	rc_list = ofa_importer_pdf_get_doc_layout( OFA_IMPORTER_PDF( self ), doc, ofa_stream_format_get_charmap( parms->format ));
	if( DEBUG_PARSE_PDFRC ){
		for( it=rc_list ; it ; it=it->next ){
			ofa_importer_pdf_dump_rc(( ofsPdfRC* ) it->data, thisfn );
		}
	}

	/* expected output must begin with global BAT datas
	 * which are found in first and one of the last pages.... */
	output = g_slist_prepend( output, lcl_pdf_v1_parse_header( self, parser, parms, rc_list ));

	/* then get the lines from bat file
	 * all relevant line pieces are extracted from each pages before trying to merge these segments */
	lines1 = lcl_pdf_v1_parse_rough( self, parser, parms, rc_list );
	if( DEBUG_PARSE_ROUGH ){
		g_debug( "%s: dumping rough read lines", thisfn );
		dump_line_list( lines1, thisfn );
	}

	lines2 = lcl_pdf_v1_parse_merge( self, parser, parms, lines1 );
	g_list_free_full( lines1, ( GDestroyNotify ) free_line );

	output = g_slist_concat( output, lcl_pdf_v1_parse_lines_build( self, parser, parms, lines2 ));
	g_list_free_full( lines2, ( GDestroyNotify ) free_line );

	g_object_unref( doc );

	return( output );
}

/*
 * parse all the pages to get BAT header datas
 * returns: the list of fields
 */
static GSList *
lcl_pdf_v1_parse_header( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list )
{
	GSList *fields;
	GList *it, *it_next;
	ofsPdfRC *rc, *rc_next;
	gboolean ok, begin_end_found, iban_found, begin_solde_found, end_solde_found;
	gchar begin_date[88], end_date[88], foo[88];
	gchar *iban, *begin_solde, *end_solde;

	ok = TRUE;
	fields = NULL;
	begin_solde = NULL;
	end_solde = NULL;
	begin_end_found = FALSE;
	iban_found = FALSE;
	begin_solde_found = FALSE;
	end_solde_found = FALSE;

	for( it=rc_list ; it ; it=it->next ){
		rc = ( ofsPdfRC * ) it->data;

		if( !begin_end_found ){
			if( sscanf( rc->text, "du %s au %s - N° %s", begin_date, end_date, foo )){
				begin_end_found = TRUE;
			}
		}

		if( !iban_found ){
			if( g_str_has_prefix( rc->text, st_header_iban )){
				iban = g_strdup( rc->text+my_strlen( st_header_iban ));
				iban_found = TRUE;
			}
		}

		if( !begin_solde_found && !my_collate( rc->text, st_header_ancien_solde ) ){
			it_next = it->next;
			rc_next = ( ofsPdfRC * ) it_next->data;
			begin_solde = get_amount( rc_next );
			begin_solde_found = TRUE;
		}

		if( !end_solde_found && g_str_has_prefix( rc->text, st_footer_end_solde )){
			it_next = it->next;
			rc_next = ( ofsPdfRC * ) it_next->data;
			end_solde = get_amount( rc_next );
			end_solde_found = TRUE;
			break;
		}
	}

	if( !begin_end_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, _( "neither beginning not ending dates found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}
	if( !iban_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, _( "IBAN not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}
	if( !begin_solde_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, _( "beginning solde not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}
	if( !end_solde_found ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, _( "ending solde not found" ));
		}
		parms->parse_errs += 1;
		ok = FALSE;
	}

	if( ok ){
		fields = g_slist_prepend( fields, g_strdup( "1" ));
		fields = g_slist_prepend( fields, g_strdup( "" ));					/* id placeholder */
		fields = g_slist_prepend( fields, g_strdup( parms->uri ));
		fields = g_slist_prepend( fields, g_strdup( parser->label ));
		fields = g_slist_prepend( fields, g_strdup( iban ));
		fields = g_slist_prepend( fields, g_strdup( "EUR" ));
		fields = g_slist_prepend( fields, g_strdup( begin_date ));
		fields = g_slist_prepend( fields, begin_solde );
		fields = g_slist_prepend( fields, g_strdup( "Y" ));
		fields = g_slist_prepend( fields, g_strdup( end_date ));
		fields = g_slist_prepend( fields, end_solde );
		fields = g_slist_prepend( fields, g_strdup( "Y" ));
	}
	return( g_slist_reverse( fields ));
}

/*
 * Returns: a GList of sLine structure, one for each parsed line
 * (as a consequence, some of them will have to be merged later)
 * We are trying here to only keep useful datas, i.e. the data which will be
 * converted to actual BAT lines
 */
static GList *
lcl_pdf_v1_parse_rough( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *rc_list )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_lcl_pdf_v1_parse_rough";
	GList *it, *lines;
	ofsPdfRC *rc;
	gdouble acceptable_diff, first_y;
	sLine *line;
	gchar *tmp, *str;

	lines = NULL;

	/* for each page, we do not try to interpret anything while we do not have found
	 * the beginning of useful datas - then we scan for end of page, and so we reset
	 * the beginning of next page
	 */
	first_y = 0;
	acceptable_diff = ofa_importer_pdf_get_acceptable_diff();

	for( it=rc_list ; it ; it=it->next ){
		rc = ( ofsPdfRC * ) it->data;
		if( DEBUG_ROUGH_PARSE ){
			ofa_importer_pdf_dump_rc( rc, thisfn );
		}

		/* do not do anything while we do not have found the begin of
		 * the array - which is 'ANCIEN SOLDE' for page zero
		 * or the common column headers for other pages */
		if( !first_y ){
			if( DEBUG_ROUGH_PARSE ){
				g_debug( "%s: ignored as first_y is zero", thisfn );
			}
			// x1=286.064000, y1=466.188000, x2=354.068000, y2=474.513000, text='ANCIEN SOLDE'
			if( rc->page_num == 0 && g_str_has_prefix( rc->text, st_header_ancien_solde ) && rc->x1 > 257 && rc->x1 < 315 ){
				first_y = rc->y2;
			}
			// x1=504.350000, y1=442.288000, x2=537.848000, y2=450.613000, text='CREDIT'
			if( rc->page_num > 0 && g_str_has_prefix( rc->text, st_page_credit ) && rc->x1 > 454 && rc->x1 < 554 ){
				first_y = rc->y2;
			}
			continue;
		}

		/* end of useful page datas
		 * x1=524.048000, y1=821.256000, x2=560.520000, y2=828.656000, text='Page 1 / 3
		 * this also marks our search for the beginning of the next page */
		if( rc->y2 >= st_detail_max_y ){
			if( DEBUG_ROUGH_PARSE ){
				g_debug( "%s: ignored as rc->y2 >= st_detail_max_y", thisfn );
			}
			first_y = 0;
			continue;
		}

		/* ignore any sort of data outside of the detail line (e.g. blueprint template ref) */
		if( rc->x1 >= st_detail_max_x ){
			if( DEBUG_ROUGH_PARSE ){
				g_debug( "%s: ignored as rc->x1 >= st_detail_max_x", thisfn );
			}
			continue;
		}

		/* also we can safely ignore all datas after SOLDE EN EUROS
		 * note that this leave the date of this solde as the last (unfinished) line in the list */
		if( g_str_has_prefix( rc->text, st_footer_end_solde )){
			if( DEBUG_ROUGH_PARSE ){
				g_debug( "%s: break as g_str_has_prefix( rc->text, st_footer_end_solde )", thisfn );
			}
			break;
		}

		if( first_y > 0 && rc->y1 > first_y ){

			/* a transaction field */
			line = find_line( &lines, acceptable_diff, rc );

			if( rc->x1 < st_label_min_x ){
				line->dope = g_strstrip( g_strndup( rc->text, 10 ));
				if( my_strlen( rc->text ) > 10 ){
					line->label = g_strstrip( g_strdup( rc->text+10 ));
				}

			} else if( rc->x1 < st_valeur_min_x ){
				tmp = g_strstrip( g_strdup( rc->text ));
				if( my_strlen( line->label )){
					str = g_strconcat( line->label, " ", tmp, NULL );
				} else {
					str = g_strdup( tmp );
				}
				g_free( tmp );
				g_free( line->label );
				line->label = str;

			} else if( rc->x1 < st_debit_min_x ){
				line->deffect = g_strstrip( g_strndup( rc->text, 10 ));

			} else {
				line->amount = get_amount( rc );
			}

			if( DEBUG_ROUGH_PARSE ){
				dump_line( line, thisfn );
			}
		} else {
			if( DEBUG_ROUGH_PARSE ){
				g_debug( "%s: ignored as first_y <= 0 || rc->y1 <= first_y", thisfn );
			}
		}
	}

	return( lines );
}

/*
 * merge
 *
 * Rationale: the previous lcl_pdf_v1_parse_rough() function has build sLine's structs
 * which each handles a printed line of the PDF.
 * We have here to:
 * - merge multi-lines data
 * - filter unrelevant lines
 */
static GList *
lcl_pdf_v1_parse_merge( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *rough_list )
{
	static const gchar *thisfn = "ofa_importer_pdf_lcl_pdf_v1_parse_merge";
	GList *it, *lines;
	gdouble prev_y;
	sLine *line, *outline, *prev_line;
	gchar *str;

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

		/* just ignore solde intermediaire */
		if( line->label && g_str_has_prefix( line->label, st_page_solde_intermed )){
			continue;
		}

		/* just ignore debits/credits totaux on the last page */
		if( line->label && g_str_has_prefix( line->label, st_page_totaux )){
			continue;
		}

		/* just ignore Récapitulatif des frais perçus (may be on its own page) */
		if( line->label && g_str_has_prefix( line->label, st_page_recapitulatif )){
			continue;
		}

		if( line->dope ){

			if( !line->deffect || !line->amount ){
				/* this error in the last line can be safely ignored as this was the
				 * beginning of the SOLDE EN EUROS line */
				if( !it->next ){
					continue;
				}

				str = g_strdup_printf( _( "invalid line: operation=%s, label=%s, value=%s, amount=%s" ),
						line->dope, line->label, line->deffect, line->amount );
				if( parms->progress ){
					my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, str );
				} else {
					g_debug( "%s: %s", thisfn, str );
				}
				g_free( str );

				parms->parse_errs += 1;
				if( parms->stop ){
					break;
				} else {
					continue;
				}
			}

			outline = g_new0( sLine, 1 );
			outline->dope = g_strdup( line->dope );
			outline->deffect = g_strdup( line->deffect );
			outline->label = g_strdup( line->label );
			outline->amount = g_strdup( line->amount );
			prev_line = outline;
			prev_y = line->y;
			lines = g_list_prepend( lines, prev_line );

		} else if( line->deffect || line->amount || line->y - prev_y > 25){

				str = g_strdup_printf( _( "invalid line: operation=%s, label=%s, value=%s, amount=%s" ),
						line->dope, line->label, line->deffect, line->amount );
				if( parms->progress ){
					my_iprogress_set_text( parms->progress, self, MY_PROGRESS_ERROR, str );
				} else {
					g_debug( "%s: %s", thisfn, str );
				}
				g_free( str );

				parms->parse_errs += 1;
				if( parms->stop ){
					break;
				} else {
					continue;
				}

		} else {
			str = g_strdup_printf( "%s / %s", prev_line->label, line->label );
			g_free( prev_line->label );
			prev_line->label = str;
			prev_y = line->y;
		}
	}

	return( lines );
}

static GSList *
lcl_pdf_v1_parse_lines_build( ofaImporterPdfLcl *self, const sParser *parser, ofsImporterParms *parms, GList *filtered_list )
{
	GSList *output, *fields;
	GList *it;
	sLine *line;

	output = NULL;

	/* last build the detail lines */
	for( it=filtered_list ; it ; it=it->next ){
		line = ( sLine * ) it->data;

		fields = NULL;

		fields = g_slist_prepend( fields, g_strdup( "2" ));
		fields = g_slist_prepend( fields, g_strdup( "" ));					/* id placeholder */
		fields = g_slist_prepend( fields, g_strdup( line->dope ));
		fields = g_slist_prepend( fields, g_strdup( line->deffect ));
		fields = g_slist_prepend( fields, g_strdup( "" ));
		fields = g_slist_prepend( fields, g_strdup( line->label ));
		fields = g_slist_prepend( fields, g_strdup( line->amount ));
		fields = g_slist_prepend( fields, g_strdup( "" ));

		output = g_slist_prepend( output, g_slist_reverse( fields ));
	}

	return( output );
}

static sParser *
get_willing_to_parser( const ofaImporterPdfLcl *self, const ofaStreamFormat *format, const gchar *uri )
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
get_default_stream_format( const ofaImporterPdfLcl *self, ofaIGetter *getter )
{
	ofaStreamFormat *format;

	format = ofa_stream_format_new( getter, NULL, OFA_SFMODE_IMPORT );

	ofa_stream_format_set( format,
			TRUE,  "UTF-8",
			TRUE,  MY_DATE_DMYDOT,			/* date format dd.mm.yyyy */
			TRUE,  MY_CHAR_SPACE,			/* space thousand sep */
			TRUE,  MY_CHAR_COMMA,			/* comma decimal sep */
			FALSE, MY_CHAR_ZERO,			/* no field sep */
			FALSE, MY_CHAR_ZERO,			/* no string delim */
			0 );							/* no header */

	ofa_stream_format_set_field_updatable( format, OFA_SFHAS_ALL, FALSE );

	return( format );
}

/*
 * find the sLine structure for the specified rc
 * allocating a new one if needed
 */
static sLine *
find_line( GList **lines, gdouble acceptable_diff, ofsPdfRC *rc )
{
	GList *it;
	sLine *line;

	for( it=( *lines ) ; it ; it=it->next ){
		line = ( sLine * ) it->data;
		if( fabs( line->y - rc->y1 ) <= acceptable_diff && rc->page_num == line->page_num ){
			return( line );
		}
	}

	line = g_new0( sLine, 1 );
	line->y = rc->y1;
	line->page_num = rc->page_num;
	*lines = g_list_append( *lines, line );

	return( line );
}

static void
dump_line_list( GList *lines, const gchar *label )
{
	GList *it;

	for( it=lines ; it ; it=it->next ){
		dump_line(( sLine * ) it->data, label );
	}
}

static void
dump_line( sLine *line, const gchar *label )
{
	g_debug( "%s: dope='%s', label='%s', deffect='%s', amount='%s', y=%.2f",
			label, line->dope, line->label, line->deffect, line->amount, line->y );
}

static void
free_line( sLine *line )
{
	g_free( line->dope );
	g_free( line->deffect );
	g_free( line->label );
	g_free( line->amount );

	g_free( line );
}

static gchar *
get_amount( ofsPdfRC *rc )
{
	gchar *amount;

	amount = rc->x1 < st_credit_min_x ? g_strdup_printf( "-%s", rc->text ) : g_strdup( rc->text );

	return( amount );
}
