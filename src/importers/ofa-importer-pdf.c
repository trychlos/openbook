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

static gint cmp_rectangles( ofsPdfRC *a, ofsPdfRC *b );

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
ofa_importer_pdf_is_willing_to( const ofaImporterPdf *instance, const gchar *uri, const GList *accepted_contents )
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
 * @page: a #PopplerPage page.
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
ofa_importer_pdf_get_layout( const ofaImporterPdf *instance, PopplerPage *page )
{
	static const gchar *thisfn = "ofa_importer_pdf_get_layout";
	ofaImporterPdfPrivate *priv;
	PopplerRectangle *rc_layout, *rc;
	guint rc_count, i;
	GList *rc_ordered, *it;
	gint len;
	ofsPdfRC *src;
	gchar *text;

	g_return_val_if_fail( instance && OFA_IS_IMPORTER_PDF( instance ), FALSE );

	priv = ofa_importer_pdf_get_instance_private( instance );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	/* extract all the text layout rectangles, only keeping the first
	 * of each serie - then sort them by line */
	poppler_page_get_text_layout( page, &rc_layout, &rc_count );

	if( 0 ){
		/* dump the full layout in poppler order */
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
		src = g_new0( ofsPdfRC, 1 );
		src->x1 = rc_layout[i].x1;
		src->y1 = rc_layout[i].y1;
		src->x2 = rc_layout[i].x2;
		src->y2 = rc_layout[i].y2;
		src->text = poppler_page_get_selected_text( page, POPPLER_SELECTION_LINE, &rc_layout[i] );
		if( 0 ){
			/* dump the selected layout in poppler order */
			g_debug( "%s: x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, src->x1, src->y1, src->x2, src->y2, src->text );
		}
		rc_ordered = g_list_insert_sorted( rc_ordered, src, ( GCompareFunc ) cmp_rectangles );
		len = my_strlen( src->text );
		i += len+1;
	}
	g_free( rc_layout );

	if( 0 ){
		/* dump the ordered selected layout */
		for( it=rc_ordered ; it ; it=it->next ){
			src = ( ofsPdfRC * ) it->data;
			g_debug( "%s: x1=%lf, y1=%lf, x2=%lf, y2=%lf, text='%s'",
					thisfn, src->x1, src->y1, src->x2, src->y2, src->text );
		}
	}

	return( rc_ordered );
}

/*
 * sort the rectangles (which are text layout) by ascending line, then
 * from left to right
 */
static gint
cmp_rectangles( ofsPdfRC *a, ofsPdfRC *b )
{
	/* not all lines are well aligned - so considered 1 dot diff is equal */
	if( fabs( a->y1 - b->y1 ) > st_acceptable_diff ){
		if( a->y1 < b->y1 ){
			return( -1 );
		}
		if( a->y1 > b->y1 ){
			return( 1 );
		}
	}

	return( a->x1 < b->x1 ? -1 : ( a->x1 > b->x1 ? 1 : 0 ));
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
