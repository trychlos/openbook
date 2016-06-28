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

#include <math.h>
#include <poppler.h>

#include "my/my-utils.h"

#include "importers/ofa-importer-pdf.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImporterPdfPrivate;

static gdouble st_acceptable_diff       = 1.5;			/* acceptable diff between same boxes */

static GList *poppler_sort_rc_layout( PopplerRectangle *rc_layout, guint rc_count );
static gint   poppler_cmp_rc( PopplerRectangle *a, PopplerRectangle *b );
static GList *poppler_merge_to_pdf( GList *poppler_list, guint page_num, PopplerPage *page, const gchar *charset );
static GList *pdf_filter_one_time( GList *pdf_list );
static gint   pdf_cmp_rc( ofsPdfRC *a, ofsPdfRC *b );
static gint   cmp_rc( gdouble ax1, gdouble ay1, gdouble ax2, gdouble ay2, gdouble bx1, gdouble by1, gdouble bx2, gdouble by2 );

G_DEFINE_TYPE_EXTENDED( ofaImporterPdf, ofa_importer_pdf, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaImporterPdf ))

static void
importer_pdf_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_importer_pdf_finalize";

	g_return_if_fail( instance && OFA_IS_IMPORTER_PDF( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_pdf_parent_class )->finalize( instance );
}

static void
importer_pdf_dispose( GObject *instance )
{
	ofaImporterPdfPrivate *priv;

	g_return_if_fail( instance && OFA_IS_IMPORTER_PDF( instance ));

	priv = ofa_importer_pdf_get_instance_private( OFA_IMPORTER_PDF( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref instance members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_importer_pdf_parent_class )->dispose( instance );
}

static void
ofa_importer_pdf_init( ofaImporterPdf *self )
{
	static const gchar *thisfn = "ofa_importer_pdf_init";
	ofaImporterPdfPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_IMPORTER_PDF( self ));

	priv = ofa_importer_pdf_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_importer_pdf_class_init( ofaImporterPdfClass *klass )
{
	static const gchar *thisfn = "ofa_importer_pdf_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = importer_pdf_dispose;
	G_OBJECT_CLASS( klass )->finalize = importer_pdf_finalize;
}

/**
 * ofa_importer_pdf_is_willing_to:
 * @instance: a #ofaImporterPdf instance.
 * @uri: the uri to the filename to be imported.
 * @accepted_contents: the #GList of the accepted mimetypes.
 *
 * Returns: %TRUE if the guessed content of the @uri is listed in the
 * @accepted_contents.
 */
gboolean
ofa_importer_pdf_is_willing_to( ofaImporterPdf *instance, const gchar *uri, const GList *accepted_contents )
{
	ofaImporterPdfPrivate *priv;
	gchar *filename, *content;
	gboolean ok;

	g_return_val_if_fail( instance && OFA_IS_IMPORTER_PDF( instance ), FALSE );

	priv = ofa_importer_pdf_get_instance_private( instance );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	filename = g_filename_from_uri( uri, NULL, NULL );
	content = g_content_type_guess( filename, NULL, 0, NULL );

	ok = my_utils_str_in_list( content, accepted_contents );

	g_free( content );
	g_free( filename );

	return( ok );
}

/**
 * ofa_importer_pdf_get_layout:
 * @instance: a #ofaImporterPdf instance.
 * @doc: a #PopplerDocument document.
 * @page_num: the index of the desired page, counted from zero.
 * @charset: the input character set.
 *
 * Returns: an ordered (from left to right, and from top to bottom)
 * layout of #ofsPdfRC rectangles with text.
 *
 * Rationale: for a given text of n chars, we have n+1 layout rectangles.
 * Last is most of time a dot-only rectangle, but 2 or 3 times per
 * page, the last rectangle is bad and contains several lines.
 * So we prefer get the first rc and its text, then skip the n others.
 */
GList *
ofa_importer_pdf_get_layout( ofaImporterPdf *instance, PopplerDocument *doc, guint page_num, const gchar *charset )
{
	static const gchar *thisfn = "ofa_importer_pdf_get_layout";
	ofaImporterPdfPrivate *priv;
	PopplerPage *page;
	PopplerRectangle *rc_layout, *poppler_rc;
	guint rc_count;
	GList *rc_list, *rc_merged, *rc_filtered, *it;
	ofsPdfRC *pdf_rc;
	gchar *text;

	g_return_val_if_fail( instance && OFA_IS_IMPORTER_PDF( instance ), FALSE );

	priv = ofa_importer_pdf_get_instance_private( instance );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	/* extract all the poppler layout rectangles
	 * we get one Poppler rectangle for each glyph of the document
	 * they have to be sorted before trying to merge them
	 */
	page = poppler_document_get_page( doc, page_num );

	poppler_page_get_text_layout( page, &rc_layout, &rc_count );
	if( 1 ){
		g_debug( "%s: page_num=%u, got %u PopplerRectangles items", thisfn, page_num, rc_count );
	}
	rc_list = poppler_sort_rc_layout( rc_layout, rc_count );
	g_free( rc_layout );

	if( 0 ){
		/* dump the sorted layout */
		for( it=rc_list ; it ; it=it->next ){
			poppler_rc = ( PopplerRectangle * ) it->data;
			text = poppler_page_get_selected_text( page, POPPLER_SELECTION_LINE, poppler_rc );
			g_debug( "%s [poppler]: page_num=%u, x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, page_num, poppler_rc->x1, poppler_rc->y1, poppler_rc->x2, poppler_rc->y2, text );
			g_free( text );
		}
	}

	/* merge the adjacent PopplerRectangle's for the same text */
	rc_merged = poppler_merge_to_pdf( rc_list, page_num, page, charset );

	/* remove some rectangles which appear only one time */
	rc_filtered = pdf_filter_one_time( rc_merged );

	if( 0 ){
		/* dump the ordered selected layout */
		for( it=rc_filtered ; it ; it=it->next ){
			pdf_rc = ( ofsPdfRC * ) it->data;
			g_debug( "%s [pdf]: page_num=%u, count=%u, x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, pdf_rc->page_num, pdf_rc->count,
					pdf_rc->x1, pdf_rc->y1, pdf_rc->x2, pdf_rc->y2, pdf_rc->text );
		}
	}

	g_list_free( rc_merged );
	g_list_free_full( rc_list, ( GDestroyNotify ) poppler_rectangle_free );
	g_object_unref( page );

	return( rc_filtered );
}

/*
 * a standard text of n characters is most of the time represented here
 * with n+1 rectangles, one for each glyph plus a last of zero size
 */
static GList *
poppler_sort_rc_layout( PopplerRectangle *rc_layout, guint rc_count )
{
	static const gchar *thisfn = "ofa_importer_pdf_poppler_sort_rc_layout";
	GList *list;
	guint i, ignored;

	list = NULL;
	ignored = 0;

	for( i=0 ; i<rc_count ; ++i ){

		/* if the rectangle is zero size, then ignore */
		if( fabs( rc_layout[i].x1 - rc_layout[i].x2 ) < 1 && fabs( rc_layout[i].y1 - rc_layout[i].y2 ) < 1 ){
			if( 0 ){
				g_debug( "%s: ignoring zero size x1=%lf, y1=%lf, x2=%lf, y2=%lf",
						thisfn, rc_layout[i].x1, rc_layout[i].y1, rc_layout[i].x2, rc_layout[i].y2 );
			}
			ignored += 1;
			continue;
		}

		list = g_list_insert_sorted( list, poppler_rectangle_copy( &rc_layout[i] ), ( GCompareFunc ) poppler_cmp_rc );
	}

	if( 0 ){
		g_debug( "%s: %u (on %u) ignored zero size rectangles", thisfn, ignored, rc_count );
	}

	return( list );
}

/*
 * sort the rectangles (which are text layout) by ascending line, then
 * from left to right
 */
static gint
poppler_cmp_rc( PopplerRectangle *a, PopplerRectangle *b )
{
	return( cmp_rc( a->x1, a->y1, a->x2, a->y2, b->x1, b->y1, b->x2, b->y2 ));
}

/*
 * @list: a sorted list of PopplerRectangles:
 *
 * Returns: a merged list of ofsPdfRC rectangles.
 */
static GList *
poppler_merge_to_pdf( GList *poppler_list, guint page_num, PopplerPage *page, const gchar *charset )
{
	PopplerRectangle *poppler_rc;
	GList *pdf_merged, *it;
	gchar *text, *prev_text, *tmp, *str;
	ofsPdfRC *pdf_rc;
	GError *error;

	pdf_rc = NULL;
	prev_text = NULL;
	pdf_merged = NULL;

	/* merge the adjacent PopplerRectangle's for the same text */
	for( it=poppler_list ; it ; it=it->next ){
		poppler_rc = ( PopplerRectangle * ) it->data;

		text = poppler_page_get_selected_text( page, POPPLER_SELECTION_LINE, poppler_rc );

		/* convert to UTF-8 if needed */
		if( text && charset && my_collate( charset, "UTF-8" )){
			error = NULL;
			tmp = g_convert( text, -1, "UTF-8", charset, NULL, NULL, &error );
			if( !tmp ){
				str = g_strdup_printf( "'%s': unable to convert from %s to UTF-8: %s",
						text, charset, error->message );
				g_info( "%s", str );
				g_free( str );
			} else {
				g_free( text );
				text = tmp;
			}
		}

		if( pdf_rc && prev_text && !my_collate( prev_text, text )){
			if( pdf_rc->x2 < poppler_rc->x2 ){
				pdf_rc->x2 = poppler_rc->x2;
			}
			if( pdf_rc->y2 < poppler_rc->y2 ){
				pdf_rc->y2 = poppler_rc->y2;
			}
			pdf_rc->count += 1;
			pdf_merged = g_list_remove( pdf_merged, pdf_rc );

		} else {
			pdf_rc = g_new0( ofsPdfRC, 1 );
			pdf_rc->page_num = page_num;
			pdf_rc->count = 1;
			pdf_rc->x1 = poppler_rc->x1;
			pdf_rc->y1 = poppler_rc->y1;
			pdf_rc->x2 = poppler_rc->x2;
			pdf_rc->y2 = poppler_rc->y2;
			pdf_rc->text = g_strdup( text );
		}

		g_free( prev_text );
		prev_text = g_strdup( text );
		g_free( text );

		pdf_merged = g_list_insert_sorted( pdf_merged, pdf_rc, ( GCompareFunc ) pdf_cmp_rc );
	}

	g_free( prev_text );

	return( pdf_merged );
}

/*
 * pdf_list: the merged rectangles
 *
 * Returns: the same, without one-time occurrences
 */
static GList *
pdf_filter_one_time( GList *pdf_list )
{
	GList *pdf_filtered, *it;
	ofsPdfRC *pdf_rc;

	pdf_filtered = NULL;

	for( it=pdf_list ; it ; it=it->next ){
		pdf_rc = ( ofsPdfRC * ) it->data;

		if( pdf_rc->count > 1 ){
			pdf_filtered = g_list_prepend( pdf_filtered, pdf_rc );
		} else {
			ofa_importer_pdf_free_rc( pdf_rc );
		}
	}

	return( g_list_reverse( pdf_filtered ));
}

static gint
pdf_cmp_rc( ofsPdfRC *a, ofsPdfRC *b )
{
	return( cmp_rc( a->x1, a->y1, a->x2, a->y2, b->x1, b->y1, b->x2, b->y2 ));
}

static gint
cmp_rc( gdouble ax1, gdouble ay1, gdouble ax2, gdouble ay2, gdouble bx1, gdouble by1, gdouble bx2, gdouble by2 )
{
	if( ay2 <= by1 ){
		return( -1 );
	}
	if( ay1 >= by2 ){
		return( 1 );
	}
	return( ax2 <= bx1 ? -1 : ( ax1 >= bx2 ? 1 : 0 ));
}

/**
 * ofa_importer_pdf_get_acceptable_diff:
 *
 * Returns: the acceptable difference, in PDF points.
 */
gdouble
ofa_importer_pdf_get_acceptable_diff( void )
{
	return( st_acceptable_diff );
}

/**
 * ofa_importer_pdf_free_layout:
 * @layout: a #GList of #ofsPdfRC rectangles as returned by
 * #ofa_importer_pdf_get_layout() method.
 *
 * Free the resources allocated to @layout.
 */
void
ofa_importer_pdf_free_layout( GList *layout )
{
	g_list_free_full( layout, ( GDestroyNotify ) ofa_importer_pdf_free_rc );
}

/**
 * ofa_importer_pdf_dump_rc:
 * @rc: a #ofsPdfRC rectangle.
 * @label: a prefix to the dumped string.
 *
 * Dump the @rc.
 */
void
ofa_importer_pdf_dump_rc( const ofsPdfRC *rc, const gchar *label )
{
	static const gchar *thisfn = "ofa_importer_pdf_dump_rc";

	g_debug( "%s: page_num=%u, count=%u, x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
			label ? label : thisfn, rc->page_num, rc->count,
			rc->x1, rc->y1, rc->x2, rc->y2, rc->text );
}

/**
 * ofa_importer_pdf_free_rc:
 * @rc: a #ofsPdfRC rectangle.
 *
 * Free the resources allocated to the @rc.
 */
void
ofa_importer_pdf_free_rc( ofsPdfRC *rc )
{
	g_free( rc->text );
	g_free( rc );
}
