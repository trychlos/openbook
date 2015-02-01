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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <math.h>
#include <poppler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api/my-date.h>
#include <api/my-double.h>
#include <api/my-utils.h>
#include <api/ofa-file-format.h>
#include <api/ofa-iimportable.h>
#include <api/ofo-bat.h>

#include "ofa-importer.h"

/* private instance data
 */
struct _ofaLclPdfImporterPrivate {
	gboolean             dispose_has_run;

	const ofaFileFormat *settings;
	ofoDossier          *dossier;
	guint                count;
	guint                errors;

	ofxAmount            tot_debit;
	ofxAmount            tot_credit;

};

typedef struct {
	PopplerRectangle *rc;
	gchar            *text;
}
	sRC;

typedef struct {
	gchar  *sdate;
	gchar  *slabel;
	gchar  *svaleur;
	gchar  *sdebit;
	gchar  *scredit;
	gdouble y;
}
	sLine;

/* a description of the import functions we are able to manage here
 */
typedef struct {
	const gchar *label;
	gint         version;
	gboolean   (*fnTest)  ( ofaLclPdfImporter *, const gchar * );
	ofsBat *   (*fnImport)( ofaLclPdfImporter *, const gchar * );
}
	ImportFormat;

static gboolean lcl_pdf_v1_check( ofaLclPdfImporter *importer, const gchar *uri );
static ofsBat  *lcl_pdf_v1_import( ofaLclPdfImporter *importer, const gchar *uri );

static ImportFormat st_import_formats[] = {
		{ "LCL-PDF v1.2014", 1, lcl_pdf_v1_check, lcl_pdf_v1_import },
		{ 0 }
};

/*static gdouble       st_date_min_x      = 35;*/
static gdouble       st_label_min_x     = 70;
static gdouble       st_valeur_min_x    = 360;
static gdouble       st_debit_min_x     = 410;
static gdouble       st_credit_min_x    = 490;
static gdouble       st_half_y          = 6;		/* half of the height of a line */
static gdouble       st_diff            = 1.5;		/* acceptable diff */

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void         class_init( ofaLclPdfImporterClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_dispose( GObject *object );
static void         instance_finalize( GObject *object );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *lcl_pdf_importer );
static gboolean     iimportable_is_willing_to( ofaIImportable *importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count );
static guint        iimportable_import_uri( ofaIImportable *importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofoDossier *dossier );
static ofsBat      *read_header( ofaLclPdfImporter *importer, PopplerPage *page, GList *rc_list );
static void         read_lines( ofaLclPdfImporter *importer, ofsBat *bat, PopplerPage *page, gint page_i, GList *rc_list );
static GList       *get_ordered_layout_list( ofaLclPdfImporter *importer, PopplerPage *page );
static gint         cmp_rectangles( sRC *a, sRC *b );
static gboolean     get_dot_dmyy( GDate *date, const gchar *sdate );
static void         get_dope_from_str( GDate *date, ofsBat *bat, const gchar *sdate );
static sLine       *find_line( GList **lines, gdouble y );
static void         free_rc( sRC *src );
static void         free_line( sLine *line );

GType
ofa_lcl_pdf_importer_get_type( void )
{
	return( st_module_type );
}

void
ofa_lcl_pdf_importer_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_register_type";

	static GTypeInfo info = {
		sizeof( ofaLclPdfImporterClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaLclPdfImporter ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaLclPdfImporter", &info, 0 );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );
}

static void
class_init( ofaLclPdfImporterClass *klass )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;

	g_type_class_add_private( klass, sizeof( ofaLclPdfImporterPrivate ));
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_instance_init";
	ofaLclPdfImporter *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	g_return_if_fail( instance && OFA_IS_LCL_PDF_IMPORTER( instance ));

	self = OFA_LCL_PDF_IMPORTER( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_LCL_PDF_IMPORTER, ofaLclPdfImporterPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	ofaLclPdfImporterPrivate *priv;

	g_return_if_fail( object && OFA_IS_LCL_PDF_IMPORTER( object ));

	priv = OFA_LCL_PDF_IMPORTER( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_instance_finalize";

	g_return_if_fail( object && OFA_IS_LCL_PDF_IMPORTER( object ));

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->is_willing_to = iimportable_is_willing_to;
	iface->import_uri = iimportable_import_uri;
}

static guint
iimportable_get_interface_version( const ofaIImportable *lcl_pdf_importer )
{
	return( 1 );
}

/*
 * do the minimum to identify the file
 * as this moment, it should not be needed to make any charmap conversion
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
iimportable_is_willing_to( ofaIImportable *importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_iimportable_is_willing_to";
	ofaLclPdfImporterPrivate *priv;
	gint i;
	gboolean ok;

	g_debug( "%s: importer=%p, uri=%s, settings=%p, count=%p",
			thisfn, ( void * ) importer, uri, ( void * ) settings, ( void * ) count );

	priv = OFA_LCL_PDF_IMPORTER( importer )->priv;
	ok = FALSE;

	priv->settings = settings;

	for( i=0 ; st_import_formats[i].label ; ++i ){
		if( st_import_formats[i].fnTest( OFA_LCL_PDF_IMPORTER( importer ), uri )){
			*ref = GINT_TO_POINTER( i );
			*count = priv->count;
			ok = TRUE;
			break;
		}
	}

	return( ok );
}

/*
 * import the file
 */
static guint
iimportable_import_uri( ofaIImportable *importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_iimportable_import_uri";
	ofaLclPdfImporterPrivate *priv;
	gint idx;
	ofsBat *bat;

	g_debug( "%s: importer=%p, ref=%p, uri=%s, settings=%p, dossier=%p",
			thisfn, ( void * ) importer, ref,
			uri, ( void * ) settings, ( void * ) dossier );

	priv = OFA_LCL_PDF_IMPORTER( importer )->priv;

	priv->settings = settings;
	priv->dossier = dossier;

	idx = GPOINTER_TO_INT( ref );
	bat = NULL;

	if( st_import_formats[idx].fnImport ){
		bat = st_import_formats[idx].fnImport( OFA_LCL_PDF_IMPORTER( importer ), uri );
		if( bat ){
			bat->uri = g_strdup( uri );
			bat->format = g_strdup( st_import_formats[idx].label );
			ofo_bat_import( importer, bat, dossier );
			ofs_bat_free( bat );
		}
	}

	return( priv->errors );
}

/*
 *
 */
static gboolean
lcl_pdf_v1_check( ofaLclPdfImporter *importer, const gchar *uri )
{
	static const gchar *thisfn = "ofa_importer_lcl_pdf_v1_check";
	PopplerDocument *doc;
	PopplerPage *page;
	GError *error;
	gchar *text, *found;
	gboolean ok;

	ok = FALSE;
	error = NULL;
	doc = poppler_document_new_from_file( uri, NULL, &error );
	if( !doc ){
		g_debug( "%s: %s", thisfn, error->message );
		g_error_free( error );
		return( ok );
	}

	page = poppler_document_get_page( doc, 0 );
	text = poppler_page_get_text( page );
	found = g_strstr_len( text, -1, "RELEVE DE COMPTE" );
	if( found ){
		found = g_strstr_len( text, -1, "CREDIT LYONNAIS" );
		if( found ){
			ok = TRUE;
		}
	}

	g_free( text );
	g_object_unref( page );
	g_object_unref( doc );

	return( ok );
}

static ofsBat *
lcl_pdf_v1_import( ofaLclPdfImporter *importer, const gchar *uri )
{
	static const gchar *thisfn = "ofa_importer_get_file_content";
	ofaLclPdfImporterPrivate *priv;
	PopplerDocument *doc;
	PopplerPage *page;
	GError *error;
	ofsBat *bat;
	gint page_n, page_i;
	GList *rc_list, *it;
	ofxAmount debit, credit;
	ofsBatDetail *detail;
	gchar *sbegin, *send, *msg, *sdebit, *scredit;

	priv = importer->priv;
	bat = NULL;
	error = NULL;

	doc = poppler_document_new_from_file( uri, NULL, &error );
	if( !doc ){
		g_debug( "%s: %s", thisfn, error->message );
		g_error_free( error );
		return( bat );
	}

	page_n = poppler_document_get_n_pages( doc );
	for( page_i=0 ; page_i < page_n ; ++page_i ){
		page = poppler_document_get_page( doc, page_i );
		rc_list = get_ordered_layout_list( importer, page );

		if( page_i == 0 ){
			bat = read_header( importer, page, rc_list );

			sbegin = my_date_to_str( &bat->begin, MY_DATE_DMYY );
			send = my_date_to_str( &bat->end, MY_DATE_DMYY );

			if( ofo_bat_exists( priv->dossier, bat->rib, &bat->begin, &bat->end )){
				msg = g_strdup_printf( _( "Already imported BAT file: RIB=%s, begin=%s, end=%s" ),
						bat->rib, sbegin, send );
				ofa_iimportable_set_message(
						OFA_IIMPORTABLE( importer ), 0, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				priv->errors += 1;
				ofs_bat_free( bat );
				bat = NULL;

			} else {
				msg = g_strdup_printf( _( "Importing RIB=%s, begin=%s, end=%s" ),
						bat->rib, sbegin, send );
				ofa_iimportable_set_message(
						OFA_IIMPORTABLE( importer ), 0, IMPORTABLE_MSG_STANDARD, msg );
				g_free( msg );
			}

			g_free( sbegin );
			g_free( send );
		}

		if( bat ){
			read_lines( importer, bat, page, page_i, rc_list );
		}

		g_list_free_full( rc_list, ( GDestroyNotify ) free_rc );
		g_object_unref( page );
	}

	g_object_unref( doc );
	ofa_iimportable_set_count( OFA_IIMPORTABLE( importer ), priv->count );

	/* put back lines in their initial order */
	if( bat ){
		bat->details = g_list_reverse( bat->details );
	}

	/* display, just for make debug easier */
	if( bat ){
		ofs_bat_dump( bat );
	}

	/* check the totals: this make sure we have all lines with right
	 *  amounts */
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

		sdebit = my_double_to_str( priv->tot_debit );
		scredit = my_double_to_str( priv->tot_credit );
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
				sdebit = my_double_to_str( debit );
				msg = g_strdup_printf( _( "Error detected: computed debit=%s" ), sdebit );
				ofa_iimportable_set_message(
						OFA_IIMPORTABLE( importer ), priv->count, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( sdebit );
			}
			if( credit != priv->tot_credit ){
				scredit = my_double_to_str( credit );
				msg = g_strdup_printf( _( "Error detected: computed credit=%s" ), scredit );
				ofa_iimportable_set_message(
						OFA_IIMPORTABLE( importer ), priv->count, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( scredit );
			}
		}
	}

	return( bat );
}

static ofsBat *
read_header( ofaLclPdfImporter *importer, PopplerPage *page, GList *rc_list )
{
	static const gchar *thisfn = "ofa_importer_read_header";
	ofsBat *bat;
	gint i;
	GList *it, *it_next;
	sRC *src, *src_next;
	gchar sbegin[80], send[80], foo[80];
	GDate date;
	gboolean begin_end_found, iban_found, begin_solde_found;
	ofxAmount amount;

	bat = g_new0( ofsBat, 1 );
	bat->version = 1;

	begin_end_found = FALSE;
	iban_found = FALSE;
	begin_solde_found = FALSE;

	/* having a word selection or a line selection doesn't change
	 * the result : two groups of lines have to be remediated */
	for( i=0, it=rc_list ; it ; ++i, it=it->next ){
		src = ( sRC * ) it->data;

		if( !begin_end_found ){
			if( sscanf( src->text, "du %s au %s - N° %s", sbegin, send, foo )){
				if( get_dot_dmyy( &date, sbegin )){
					my_date_set_from_date( &bat->begin, &date );
				} else {
					g_debug( "%s: not a valid date: %s at line %u", thisfn, sbegin, i );
				}
				if( get_dot_dmyy( &date, send )){
					my_date_set_from_date( &bat->end, &date );
				} else {
					g_debug( "%s: not a valid date: %s at line %u", thisfn, send, i );
				}
				begin_end_found = TRUE;
			}
		}

		if( !iban_found ){
			if( g_str_has_prefix( src->text, "IBAN : " )){
				bat->rib = g_strdup( src->text+7 );
				iban_found = TRUE;
			}
		}

		if( !g_utf8_collate( src->text, "ANCIEN SOLDE" ) && !begin_solde_found ){
			it_next = it->next;
			src_next = ( sRC * ) it_next->data;
			amount = my_double_set_from_str( src_next->text );
			if( src_next->rc->x1 < st_credit_min_x ){
				amount *= -1;
			}
			bat->begin_solde = amount;
			bat->begin_solde_set = TRUE;
			begin_solde_found = TRUE;
		}
	}

	return( bat );
}

/*
 * extract the transaction lines from a page, and update the BAT structure
 */
static void
read_lines( ofaLclPdfImporter *importer, ofsBat *bat, PopplerPage *page, gint page_i, GList *rc_list )
{
	static const gchar *thisfn = "ofa_importer_read_lines";
	ofaLclPdfImporterPrivate *priv;
	gint i;
	sRC *src;
	gdouble first_y;
	GList *lines, *it;
	sLine *line;
	ofsBatDetail *detail, *prev_detail;
	ofxAmount debit, credit;
	gboolean next_is_last;
	gchar *str;
	gboolean dbg1;

	g_debug( "%s: importer=%p, bat=%p, page=%p, page_i=%u, rc_list=%p",
			thisfn, ( void * ) importer, ( void * ) bat, ( void * ) page, page_i, ( void * ) rc_list );

	priv = importer->priv;
	first_y = 0;
	lines = NULL;
	next_is_last = FALSE;
	prev_detail = NULL;
	dbg1 = FALSE;

	for( i=0, it=rc_list ; it ; ++i, it=it->next ){
		src = ( sRC * ) it->data;

		/* do not do anything while we do not have found the begin of
		 * the array - which is 'ANCIEN SOLDE' for page zero
		 * or 'DEBIT CREDIT' for others */
		if( first_y == 0 ){
			if( page_i == 0 ){
				if( !g_utf8_collate( src->text, "ANCIEN SOLDE" ) && src->rc->x2 < st_debit_min_x ){
					first_y = round( src->rc->y1 )+st_half_y;
				}
			} else if( !g_utf8_collate( src->text, "DEBIT" ) && src->rc->x1 > st_debit_min_x ){
				first_y = round( src->rc->y1 )+st_half_y;
			}
		}

		if( first_y > 0 ){
			/* end of the page */
			if( g_str_has_prefix( src->text, "Page " ) && src->rc->x1 > st_credit_min_x ){
				break;
			}

			/* a transaction field */
			if( src->rc->y1 > first_y ){
				line = find_line( &lines, src->rc->y1 );
				if( dbg1 ){
					g_debug( "%s: x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
							thisfn, src->rc->x1, src->rc->y1, src->rc->x2, src->rc->y2, src->text );
				}

				if( src->rc->x1 < st_label_min_x ){
					g_free( line->sdate );
					line->sdate = g_strdup( src->text );
					if( dbg1 ){
						g_debug( "%s: setting as date", thisfn );
					}

				} else if( src->rc->x1 < st_valeur_min_x ){
					g_free( line->slabel );
					line->slabel = g_strdup( src->text );
					if( dbg1 ){
						g_debug( "%s: setting as label", thisfn );
					}

				} else if( src->rc->x1 < st_debit_min_x ){
					g_free( line->svaleur );
					line->svaleur = g_strdup( src->text );
					if( dbg1 ){
						g_debug( "%s: setting as value", thisfn );
					}

				} else if( src->rc->x1 < st_credit_min_x ){
					g_free( line->sdebit );
					line->sdebit = g_strdup( src->text );
					if( dbg1 ){
						g_debug( "%s: setting as debit", thisfn );
					}

				} else {
					g_free( line->scredit );
					line->scredit = g_strdup( src->text );
					if( dbg1 ){
						g_debug( "%s: setting as credit", thisfn );
					}
				}

				if( next_is_last ){
					break;
				}

				/* end of the transaction list - next is the solde */
				if( g_str_has_prefix( src->text, "SOLDE EN EUROS" ) && src->rc->x1 > st_credit_min_x ){
					next_is_last = TRUE;
					continue;
				}
			}
			ofa_iimportable_pulse( OFA_IIMPORTABLE( importer ), IMPORTABLE_PHASE_IMPORT );
		}
	}

	/* we have all transaction lines, with each field normally set at
	 *  its place - but we have yet to filter some useless lines */
	for( it=lines ; it ; it=it->next ){
		line = ( sLine * ) it->data;

		/* intermediate balance at the end of month - not taken into account */
		if( !line->sdate &&
				g_str_has_prefix( line->slabel, "SOLDE INTERMEDIAIRE " ) &&
				!line->svaleur ){
			continue;
		}

		/* end of the transaction list
		 * will be used to check that we have all lines */
		if( !line->sdate &&
				!g_utf8_collate( line->slabel, "TOTAUX" ) &&
				!line->svaleur ){
			priv->tot_debit = my_double_set_from_str( line->sdebit );
			priv->tot_credit = my_double_set_from_str( line->scredit );
			continue;
		}

		/* final solde */
		if( g_str_has_prefix( line->slabel, "SOLDE EN " )){
			debit = my_double_set_from_str( line->sdebit );
			credit = my_double_set_from_str( line->scredit );
			bat->end_solde = credit - debit;
			bat->end_solde_set = TRUE;
			break;
		}

		if( line->sdate ){
			detail = g_new0( ofsBatDetail, 1 );
			detail->version = 1;
			get_dope_from_str( &detail->dope, bat, line->sdate );
			get_dot_dmyy( &detail->deffect, line->svaleur );
			detail->label = g_strdup( line->slabel );
			debit = my_double_set_from_str( line->sdebit );
			credit = my_double_set_from_str( line->scredit );
			detail->amount = credit - debit;
			prev_detail = detail;
			bat->details = g_list_prepend( bat->details, detail );
			priv->count += 1;

		} else if( !line->sdate && !line->svaleur && !line->sdebit && !line->scredit && prev_detail ){
			str = g_strdup_printf( "%s / %s", prev_detail->label, line->slabel );
			g_free( prev_detail->label );
			prev_detail->label = str;
		}
	}

	g_list_free_full( lines, ( GDestroyNotify ) free_line );
}

/*
 * for a given text of n chars, we have n+1 layout rectangles
 * last is most of time a dot-only rectangle, but 2 or 3 times per
 * page, the last rc is bad and contains several lines
 * so we could get the first rc and its text, then skip the n others
 */
static GList *
get_ordered_layout_list( ofaLclPdfImporter *importer, PopplerPage *page )
{
	static const gchar *thisfn = "ofa_importer_get_ordered_layout_list";
	PopplerRectangle *rc_layout, *rc;
	guint rc_count, i;
	GList *rc_ordered;
	gint len;
	sRC *src;
	gchar *text;

	/* extract all the text layout rectangles, only keeping the first
	 * of each serie - then sort them by line */
	poppler_page_get_text_layout( page, &rc_layout, &rc_count );

	if( 0 ){
		for( i=0 ; i<rc_count ; ++i ){
			rc = &rc_layout[i];
			text = poppler_page_get_selected_text( page, POPPLER_SELECTION_LINE, rc );
			g_debug( "%s: x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, rc->x1, rc->y1, rc->x2, rc->y2, text );
			g_free( text );
		}
	}

	rc_ordered = NULL;
	for( i=0 ; i<rc_count ; ){
		src = g_new0( sRC, 1 );
		src->rc = poppler_rectangle_copy( &rc_layout[i] );
		src->text = poppler_page_get_selected_text( page, POPPLER_SELECTION_LINE, src->rc );
		if( 0 ){
			g_debug( "%s: x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, src->rc->x1, src->rc->y1, src->rc->x2, src->rc->y2, src->text );
		}
		rc_ordered = g_list_insert_sorted( rc_ordered, src, ( GCompareFunc ) cmp_rectangles );
		len = my_strlen( src->text );
		i += len+1;
	}
	g_free( rc_layout );

	return( rc_ordered );
}

/*
 * sort the rectangles (which are text layout) by ascending line, then
 * from left to right
 */
static gint
cmp_rectangles( sRC *a, sRC *b )
{
	/* not all lines are well aligned - so considered 1 dot diff is equal */
	if( fabs( a->rc->y1 - b->rc->y1 ) > st_diff ){
		if( a->rc->y1 < b->rc->y1 ){
			return( -1 );
		}
		if( a->rc->y1 > b->rc->y1 ){
			return( 1 );
		}
	}

	return( a->rc->x1 < b->rc->x1 ? -1 : ( a->rc->x1 > b->rc->x1 ? 1 : 0 ));
}

static gboolean
get_dot_dmyy( GDate *date, const gchar *sdate )
{
	gint d, m, y;
	gboolean ok;

	ok = FALSE ;
	my_date_clear( date );

	if( sscanf( sdate, "%u.%u.%u", &d, &m, &y )){
		if( d <= 31 && m <= 12 && ( y >= 2000 || y < 100 )){
			g_date_set_day( date, d );
			g_date_set_month( date, m );
			g_date_set_year( date, ( y < 100 ) ? 2000+y : y );
			ok = TRUE;
		}
	}

	return( ok );
}

static void
get_dope_from_str( GDate *date, ofsBat *bat, const gchar *sdate )
{
	gint d, m, m_end, y_end;

	my_date_clear( date );

	if( sscanf( sdate, "%u.%u", &d, &m )){
		if( d <= 31 && m <= 12 ){
			g_date_set_day( date, d );
			g_date_set_month( date, m );
			m_end = g_date_get_month( &bat->end );
			y_end = g_date_get_year( &bat->end );
			if( m <= m_end ){
				g_date_set_year( date, y_end );
			} else {
				g_date_set_year( date, y_end+1 );
			}
		}
	}
}

/*
 * find the sLine structure for the specified rc
 * allocated a new one if needed
 */
static sLine *
find_line( GList **lines, gdouble y )
{
	GList *it;
	sLine *line;

	for( it=( *lines ) ; it ; it=it->next ){
		line = ( sLine * ) it->data;
		if( fabs( line->y - y ) <= st_diff ){
			return( line );
		}
	}

	line = g_new0( sLine, 1 );
	line->y = y;
	*lines = g_list_append( *lines, line );

	return( line );
}

static void
free_rc( sRC *src )
{
	g_free( src->text );
	poppler_rectangle_free( src->rc );
	g_free( src );
}

static void
free_line( sLine *line )
{
	g_free( line->sdate );
	g_free( line->slabel );
	g_free( line->svaleur );
	g_free( line->sdebit );
	g_free( line->scredit );

	g_free( line );
}

#if 0
typedef struct {
	const gchar *bat_label;
	const gchar *ofa_label;
}
	lclPaiement;

static const lclPaiement st_lcl_paiements[] = {
		{ "Carte",       "CB" },
		{ "Virement",    "Vir" },
		{ "Prélèvement", "Prel" },
		{ "Chèque",      "Ch" },
		{ "TIP",         "TIP" },
		{ 0 }
};

static const gchar *
lcl_get_ref_paiement( const gchar *str )
{
	gint i;

	if( str && g_utf8_strlen( str, -1 )){
		for( i=0 ; st_lcl_paiements[i].bat_label ; ++ i ){
			if( !g_utf8_collate( str, st_lcl_paiements[i].bat_label )){
				return( st_lcl_paiements[i].ofa_label );
			}
		}
	}

	return( NULL );
}

/*
 * return a newly allocated stripped string
 */
static gchar *
lcl_concatenate_labels( gchar ***iter )
{
	GString *lab1;
	gchar *lab2;

	lab1 = g_string_new( "" );
	if( **iter ){
		lab2 = g_strdup( **iter );
		g_strstrip( lab2 );
		if( lab2 && g_utf8_strlen( lab2, -1 )){
			if( g_utf8_strlen( lab1->str, -1 )){
				lab1 = g_string_append( lab1, " " );
			}
			lab1 = g_string_append( lab1, lab2 );
		}
		g_free( lab2 );

		*iter += 1;
		if( **iter ){
			lab2 = g_strdup( **iter );
			g_strstrip( lab2 );
			if( lab2 && g_utf8_strlen( lab2, -1 )){
				if( g_utf8_strlen( lab1->str, -1 )){
					lab1 = g_string_append( lab1, " " );
				}
				lab1 = g_string_append( lab1, lab2 );
			}
			g_free( lab2 );

			*iter += 1;
			if( **iter && g_utf8_strlen( **iter, -1 )){
				lab2 = g_strdup( **iter );
				g_strstrip( lab2 );
				if( lab2 && g_utf8_strlen( lab2, -1 )){
					if( g_utf8_strlen( lab1->str, -1 )){
						lab1 = g_string_append( lab1, " " );
					}
					lab1 = g_string_append( lab1, lab2 );
				}
				g_free( lab2 );
			}
		}
	}

	g_strstrip( lab1->str );
	return( g_string_free( lab1, FALSE ));
}

/*
 * parse a 'dd/mm/yyyy' date
 */
static GDate *
scan_date_dmyy( GDate *date, const gchar *str )
{
	gint d, m, y;

	my_date_clear( date );
	sscanf( str, "%d/%d/%d", &d, &m, &y );
	if( d <= 0 || m <= 0 || y < 0 || d > 31 || m > 12 ){
		return( NULL );
	}
	g_date_set_dmy( date, d, m, y );
	if( !my_date_is_valid( date )){
		return( NULL );
	}

	return( date );
}

static gdouble
get_double( const gchar *str )
{
	static const gchar *thisfn = "ofa_lcl_pdf_importer_get_double";
	gdouble amount1, amount2;
	gdouble entier1, entier2;

	amount1 = g_ascii_strtod( str, NULL );
	entier1 = trunc( amount1 );
	if( entier1 == amount1 ){
		amount2 = strtod( str, NULL );
		entier2 = trunc( amount2 );
		if( entier2 == amount2 ){
			if( entier1 != entier2 ){
				g_warning( "%s: unable to get double from str='%s'", thisfn, str );
				return( 0 );
			}
		}
		return( amount2 );
	}
	return( amount1 );
}
#endif
