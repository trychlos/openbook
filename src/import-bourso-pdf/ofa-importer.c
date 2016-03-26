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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <math.h>
#include <poppler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-file-format.h"
#include "api/ofa-hub.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-bat.h"
#include "api/ofs-bat.h"

#include "ofa-importer.h"

/* private instance data
 */
struct _ofaBoursoPdfImporterPrivate {
	gboolean             dispose_has_run;

	const ofaFileFormat *settings;
	ofaHub              *hub;
	guint                count;
	guint                errors;

	ofxAmount            tot_debit;
	ofxAmount            tot_credit;

};

#define IMPORTER_DISPLAY_NAME            "Boursorama PDF Importer"
#define IMPORTER_VERSION                  PACKAGE_VERSION

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

#define DEBUG_LINE                      FALSE

static gdouble       st_label_min_x     = 80;
static gdouble       st_valeur_min_x    = 300;
static gdouble       st_debit_min_x     = 350;
static gdouble       st_credit_min_x    = 450;
static gchar        *st_iban            = "I.B.A.N. ";
static gchar        *st_begin_solde     = "SOLDE AU : ";
static gchar        *st_header_begin    = "Crédit";
static gchar        *st_end_solde       = "Nouveau solde en ";

static gdouble       st_half_y          = 6;		/* half of the height of a line */
static gdouble       st_diff            = 1.5;		/* acceptable diff */

static GType         st_module_type     = 0;
static GObjectClass *st_parent_class    = NULL;

static void      importer_finalize( GObject *object );
static void      importer_dispose( GObject *object );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      class_init( ofaBoursoPdfImporterClass *klass );
static void      iident_iface_init( myIIdentInterface *iface );
static gchar    *iident_get_display_name( const myIIdent *instance, void *user_data );
static gchar    *iident_get_version( const myIIdent *instance, void *user_data );
static void      iimportable_iface_init( ofaIImportableInterface *iface );
static guint     iimportable_get_interface_version( const ofaIImportable *bourso_pdf_importer );
static gboolean  iimportable_is_willing_to( ofaIImportable *importer, const gchar *uri, const ofaFileFormat *settings, void **ref, guint *count );
static guint     iimportable_import_uri( ofaIImportable *importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofaHub *hub, ofxCounter *imported_id );
static gboolean  bourso_pdf_v1_check( ofaBoursoPdfImporter *importer, const gchar *uri );
static ofsBat   *bourso_pdf_v1_import( ofaBoursoPdfImporter *importer, const gchar *uri );
static ofsBat   *read_header( ofaBoursoPdfImporter *importer, PopplerPage *page, GList *rc_list );
static void      read_lines( ofaBoursoPdfImporter *importer, ofsBat *bat, PopplerPage *page, gint page_i, GList *rc_list );
static GList    *get_ordered_layout_list( ofaBoursoPdfImporter *importer, PopplerPage *page );
static gint      cmp_rectangles( sRC *a, sRC *b );
static gdouble   get_double_from_str( const gchar *str );
static sLine    *find_line( GList **lines, gdouble y );
static void      free_rc( sRC *src );
static void      free_line( sLine *line );

/* a description of the import functions we are able to manage here
 */
typedef struct {
	const gchar *label;
	gint         version;
	gboolean   (*fnTest)  ( ofaBoursoPdfImporter *, const gchar * );
	ofsBat *   (*fnImport)( ofaBoursoPdfImporter *, const gchar * );
}
	ImportFormat;

static ImportFormat st_import_formats[] = {
		{ "Boursorama-PDF v1.2015", 1, bourso_pdf_v1_check, bourso_pdf_v1_import },
		{ 0 }
};

GType
ofa_bourso_pdf_importer_get_type( void )
{
	return( st_module_type );
}

void
ofa_bourso_pdf_importer_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_register_type";

	static GTypeInfo info = {
		sizeof( ofaBoursoPdfImporterClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaBoursoPdfImporter ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iident_iface_info = {
		( GInterfaceInitFunc ) iident_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "ofaBoursoPdfImporter", &info, 0 );

	g_type_module_add_interface( module, st_module_type, MY_TYPE_IIDENT, &iident_iface_info );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );
}

static void
importer_finalize( GObject *object )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_finalize";

	g_return_if_fail( object && OFA_IS_BOURSO_PDF_IMPORTER( object ));

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
importer_dispose( GObject *object )
{
	ofaBoursoPdfImporterPrivate *priv;

	g_return_if_fail( object && OFA_IS_BOURSO_PDF_IMPORTER( object ));

	priv = OFA_BOURSO_PDF_IMPORTER( object )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_instance_init";
	ofaBoursoPdfImporter *self;

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	g_return_if_fail( instance && OFA_IS_BOURSO_PDF_IMPORTER( instance ));

	self = OFA_BOURSO_PDF_IMPORTER( instance );

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BOURSO_PDF_IMPORTER, ofaBoursoPdfImporterPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
class_init( ofaBoursoPdfImporterClass *klass )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = importer_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_finalize;

	g_type_class_add_private( klass, sizeof( ofaBoursoPdfImporterPrivate ));
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_display_name = iident_get_display_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_display_name( const myIIdent *instance, void *user_data )
{
	return( g_strdup( IMPORTER_DISPLAY_NAME ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *user_data )
{
	return( g_strdup( IMPORTER_VERSION ));
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->is_willing_to = iimportable_is_willing_to;
	iface->import_uri = iimportable_import_uri;
}

static guint
iimportable_get_interface_version( const ofaIImportable *bourso_pdf_importer )
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
	static const gchar *thisfn = "ofa_bourso_pdf_importer_iimportable_is_willing_to";
	ofaBoursoPdfImporterPrivate *priv;
	gint i;
	gboolean ok;

	g_debug( "%s: importer=%p, uri=%s, settings=%p, count=%p",
			thisfn, ( void * ) importer, uri, ( void * ) settings, ( void * ) count );

	priv = OFA_BOURSO_PDF_IMPORTER( importer )->priv;
	ok = FALSE;

	priv->settings = settings;

	for( i=0 ; st_import_formats[i].label ; ++i ){
		if( st_import_formats[i].fnTest( OFA_BOURSO_PDF_IMPORTER( importer ), uri )){
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
iimportable_import_uri( ofaIImportable *importer, void *ref, const gchar *uri, const ofaFileFormat *settings, ofaHub *hub, ofxCounter *imported_id )
{
	static const gchar *thisfn = "ofa_bourso_pdf_importer_iimportable_import_uri";
	ofaBoursoPdfImporterPrivate *priv;
	gint idx;
	ofsBat *bat;

	g_debug( "%s: importer=%p, ref=%p, uri=%s, settings=%p, hub=%p, imported_id=%p",
			thisfn, ( void * ) importer, ref,
			uri, ( void * ) settings, ( void * ) hub, ( void * ) imported_id );

	priv = OFA_BOURSO_PDF_IMPORTER( importer )->priv;

	priv->settings = settings;
	priv->hub = hub;

	idx = GPOINTER_TO_INT( ref );
	bat = NULL;

	if( st_import_formats[idx].fnImport ){
		bat = st_import_formats[idx].fnImport( OFA_BOURSO_PDF_IMPORTER( importer ), uri );
		if( bat ){
			bat->uri = g_strdup( uri );
			bat->format = g_strdup( st_import_formats[idx].label );
			ofo_bat_import( importer, bat, hub, imported_id );
			ofs_bat_free( bat );
		}
	}

	return( priv->errors );
}

/*
 *
 */
static gboolean
bourso_pdf_v1_check( ofaBoursoPdfImporter *importer, const gchar *uri )
{
	static const gchar *thisfn = "ofa_importer_bourso_pdf_v1_check";
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
	found = g_strstr_len( text, -1, "Extrait de votre compte en" );
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

static ofsBat *
bourso_pdf_v1_import( ofaBoursoPdfImporter *importer, const gchar *uri )
{
	static const gchar *thisfn = "ofa_importer_bourso_pdf_v1_import";
	ofaBoursoPdfImporterPrivate *priv;
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

			sbegin = my_date_to_str( &bat->begin, ofa_prefs_date_display());
			send = my_date_to_str( &bat->end, ofa_prefs_date_display());

			if( ofo_bat_exists( priv->hub, bat->rib, &bat->begin, &bat->end )){
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

	return( bat );
}

static ofsBat *
read_header( ofaBoursoPdfImporter *importer, PopplerPage *page, GList *rc_list )
{
	/*static const gchar *thisfn = "ofa_importer_read_header";*/
	ofsBat *bat;
	gint i;
	GList *it, *it_next;
	sRC *src, *src_next;
	gboolean begin_found, end_found, iban_found, begin_solde_found;
	ofxAmount amount;

	bat = g_new0( ofsBat, 1 );
	bat->version = 1;

	begin_found = FALSE;
	end_found = FALSE;
	iban_found = FALSE;
	begin_solde_found = FALSE;

	/* having a word selection or a line selection doesn't change
	 * the result : two groups of lines have to be remediated */
	for( i=0, it=rc_list ; it ; ++i, it=it->next ){
		src = ( sRC * ) it->data;

		if( !begin_found && src->rc->x1 > 200 && src->rc->y1 > 200 ){
			if( !g_utf8_collate( src->text, "du" )){
				it = it->next;
				src = ( sRC * ) it->data;
				my_date_set_from_str( &bat->begin, src->text, ofa_prefs_date_display());
				begin_found = TRUE;
			}
		}

		if( begin_found && !end_found && src->rc->x1 > 200 && src->rc->y1 > 200 ){
			if( !g_utf8_collate( src->text, "au" )){
				it = it->next;
				src = ( sRC * ) it->data;
				my_date_set_from_str( &bat->end, src->text, ofa_prefs_date_display());
				end_found = TRUE;
			}
		}

		if( !iban_found ){
			if( g_str_has_prefix( src->text, st_iban )){
				bat->rib = g_strdup( src->text+my_strlen( st_iban ));
				iban_found = TRUE;
			}
		}

		if( !begin_solde_found && g_str_has_prefix( src->text, st_begin_solde )){
			it_next = it->next;
			src_next = ( sRC * ) it_next->data;
			amount = get_double_from_str( src_next->text );
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
read_lines( ofaBoursoPdfImporter *importer, ofsBat *bat, PopplerPage *page, gint page_i, GList *rc_list )
{
	static const gchar *thisfn = "ofa_importer_read_lines";
	ofaBoursoPdfImporterPrivate *priv;
	gint i;
	sRC *src;
	gdouble first_y, prev_y;
	GList *lines, *it;
	sLine *line;
	ofsBatDetail *detail, *prev_detail;
	ofxAmount debit, credit;
	gchar *str, *tmp;
	gboolean dbg1;

	g_debug( "%s: importer=%p, bat=%p, page=%p, page_i=%u, rc_list=%p",
			thisfn, ( void * ) importer, ( void * ) bat, ( void * ) page, page_i, ( void * ) rc_list );

	priv = importer->priv;
	first_y = 0;
	lines = NULL;
	prev_detail = NULL;
	prev_y = 0;
	dbg1 = FALSE;

	for( i=0, it=rc_list ; it ; ++i, it=it->next ){
		src = ( sRC * ) it->data;

		/* do not do anything while we do not have found the begin of
		 * the array - which is 'SOLDE AU : ' for page zero
		 * or 'Crédit' for others */
		if( first_y == 0 ){
			if( page_i == 0 ){
				if( g_str_has_prefix( src->text, st_begin_solde ) && src->rc->x2 < st_debit_min_x ){
					first_y = round( src->rc->y1 )+st_half_y;
				}
			} else if( !g_utf8_collate( src->text, st_header_begin ) && src->rc->x1 > st_debit_min_x ){
				first_y = round( src->rc->y1 )+st_half_y;
			}
		}

		if( first_y > 0 ){
			/* end of the page */
			if( g_str_has_prefix( src->text, "Montant frais bancaires" )){
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
					line->sdate = g_strndup( src->text, 10 );
					if( my_strlen( src->text ) > 10 ){
						g_free( line->slabel );
						line->slabel = g_strstrip( g_strdup( src->text+10 ));
					}
					if( dbg1 ){
						g_debug( "%s: setting as date", thisfn );
					}

				} else if( src->rc->x1 < st_valeur_min_x ){
					tmp = g_strstrip( g_strdup( src->text ));
					if( my_strlen( line->slabel )){
						str = g_strconcat( line->slabel, " ", tmp, NULL );
					} else {
						str = g_strdup( tmp );
					}
					g_free( tmp );
					g_free( line->slabel );
					line->slabel = str;
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

				/* end of the transaction list - this is the solde */
				if( g_str_has_prefix( src->text, st_end_solde )){
					g_free( bat->currency );
					bat->currency = g_strndup( src->text+my_strlen( st_end_solde ), 3 );
				}
			}
			ofa_iimportable_pulse( OFA_IIMPORTABLE( importer ), IMPORTABLE_PHASE_IMPORT );
		}
	}

	/* we have all transaction lines, with each field normally set at
	 *  its place - but we have yet to filter some useless lines */
	for( it=lines ; it ; it=it->next ){
		line = ( sLine * ) it->data;
		if( DEBUG_LINE ){
			g_debug( "%s: line=%s", thisfn, line->slabel );
		}

		if( line->sdate ){
			detail = g_new0( ofsBatDetail, 1 );
			detail->version = 1;
			my_date_set_from_str( &detail->dope, line->sdate, ofa_prefs_date_display());
			my_date_set_from_str( &detail->deffect, line->svaleur, ofa_prefs_date_display());
			detail->label = g_strdup( line->slabel );
			debit = get_double_from_str( line->sdebit );
			credit = get_double_from_str( line->scredit );
			detail->amount = credit - debit;
			if( my_date_is_valid( &detail->deffect ) && detail->amount != 0 ){
				prev_detail = detail;
				prev_y = line->y;
				bat->details = g_list_prepend( bat->details, detail );
				priv->count += 1;
			} else {
				ofs_bat_detail_free( detail );
				prev_detail = NULL;
				prev_y = 0;
			}

		} else if( line->slabel && g_str_has_prefix( line->slabel, st_end_solde )){
			debit = get_double_from_str( line->sdebit );
			credit = get_double_from_str( line->scredit );
			bat->end_solde = credit - debit;
			bat->end_solde_set = TRUE;

		} else if( !line->sdate && !line->svaleur &&
				!line->sdebit && !line->scredit && prev_detail &&
				line->y-prev_y <= 3*st_half_y ){
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
get_ordered_layout_list( ofaBoursoPdfImporter *importer, PopplerPage *page )
{
	static const gchar *thisfn = "ofa_importer_get_ordered_layout_list";
	PopplerRectangle *rc_layout, *rc;
	guint rc_count, i;
	GList *rc_ordered, *it;
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

	if( 0 ){
		for( it=rc_ordered ; it ; it=it->next ){
			src = ( sRC * ) it->data;
			g_debug( "%s: x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, src->rc->x1, src->rc->y1, src->rc->x2, src->rc->y2, src->text );
		}
	}
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

/*
 * amounts are specified with a dot as thousand separator, and a comma
 * as decimal separator (eg. 2.540,92), that my_double_set_from_str()
 * doesn't know how to handle because it is not the current locale
 */
static gdouble
get_double_from_str( const gchar *str )
{
	gdouble amount;
	GRegex *regex;
	gchar *dest1, *dest2;

	amount = 0;

	if( my_strlen( str )){
		/* remove the '.' thousand separator */
		regex = g_regex_new( "\\.", 0, 0, NULL );
		dest1 = g_regex_replace_literal( regex, str, -1, 0, "", 0, NULL );
		g_regex_unref( regex );
		/* replace the ',' command decimal separator with a dot */
		regex = g_regex_new( ",", 0, 0, NULL );
		dest2 = g_regex_replace_literal( regex, dest1, -1, 0, ".", 0, NULL );
		g_regex_unref( regex );
		/* evaluate */
		amount = g_strtod( dest2, NULL );
		/*
		g_debug( "get_double_from_str: str='%s', dest1='%s', dest2='%s', amount=%lf",
				str, dest1, dest2, amount );
				*/
		g_free( dest1 );
		g_free( dest2 );
	}

	return( amount );
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
