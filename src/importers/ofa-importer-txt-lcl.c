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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iimporter.h"
#include "api/ofa-preferences.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-bat.h"

#include "importers/ofa-importer-txt-lcl.h"

/* private instance data
 */
typedef struct {
	gboolean dispose_has_run;
}
	ofaImporterTxtLclPrivate;

#define IMPORTER_CANON_NAME              "LCL tabulated-BAT importer"
#define IMPORTER_VERSION                  PACKAGE_VERSION

static void         iident_iface_init( myIIdentInterface *iface );
static gchar       *iident_get_canon_name( const myIIdent *instance, void *user_data );
static gchar       *iident_get_version( const myIIdent *instance, void *user_data );
static void         iimporter_iface_init( ofaIImporterInterface *iface );
static const GList *iimporter_get_accepted_contents( const ofaIImporter *instance );
static gboolean     iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type );
static gboolean     is_willing_to_parse( const ofaImporterTxtLcl *instance, const gchar *uri )
static guint        iimporter_import( ofaIImporter *instance, ofsImporterParms *parms );
static guint        do_parse( ofaImporterTxtLcl *self, ofsImporterParms *parms );
static GDate       *scan_date_dmyy( GDate *date, const gchar *str );
static gboolean     lcl_tabulated_text_v1_check( ofaImporterTxtLcl *importer_txt_lcl );
static ofsBat      *lcl_tabulated_text_v1_import( ofaImporterTxtLcl *importer_txt_lcl );
static const gchar *lcl_get_ref_paiement( const gchar *str );
static gchar       *lcl_concatenate_labels( gchar ***iter );
static gdouble      get_double( const gchar *str );

G_DEFINE_TYPE_EXTENDED( ofaImporterTxtLcl, ofa_importer_txt_lcl, OFA_TYPE_IMPORTER_TXT, 0,
		G_ADD_PRIVATE( ofaImporterTxtLcl )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTER, iimporter_iface_init ))

/* A description of the import functions we are able to manage here.
 * If several versions happen to be managed, then should be set first
 * the most recent.
 */
typedef struct {
	const gchar *label;
	guint        version;
	gboolean   (*fnTest)  ( ofaImporterTxtLcl *, const ofaStreamFormat *, GSList * );
	ofsBat *   (*fnImport)( ofaImporterTxtLcl *, ofsImporterParms *, GSList * );
}
	ImportFormat;

static ImportFormat st_import_formats[] = {
		{ N_( "LCL .xls (tabulated text) 2014" ), 1, lcl_tabulated_text_v1_check, lcl_tabulated_text_v1_import },
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
	iface->import = iimporter_import;
}

static const GList *
iimporter_get_accepted_contents( const ofaIImporter *instance )
{
	return( ofa_importer_txt_get_accepted_contents( OFA_IMPORTER_TXT( instance )));
}

static gboolean
iimporter_is_willing_to( const ofaIImporter *instance, const gchar *uri, GType type )
{
	gboolean ok;

	ok = ofa_importer_txt_is_willing_to( OFA_IMPORTER_TXT( instance ), uri, type ) &&
			type == OFO_TYPE_BAT &&
			is_willing_to_parse( OFA_IMPORTER_TXT_LCL( instance ), uri );

	return( ok );
}

/*
 * do the minimum to identify the file
 * as this moment, it should not be needed to make any charmap conversion
 *
 * Returns: %TRUE if willing to import.
 */
static gboolean
is_willing_to_parse( const ofaImporterTxtLcl *self, const gchar *uri )
{
	ofaStreamFormat *format;
	GSList *lines;
	gint i;
	gboolean ok;

	ok = FALSE;

	format = ofa_stream_format_new( NULL, OFA_SFMODE_IMPORT );
	ofa_stream_format_set( format,
			TRUE,  "UTF-8",					/* charmap */
			TRUE,  MY_DATE_DMYY,			/* date format */
			FALSE, '\0',					/* thousand sep */
			TRUE,  ',',						/* decimal sep */
			FALSE, '\t',					/* field sep */
			FALSE, '\0',					/* string delim */
			FALSE, 0 );						/* headers */

	lines = my_utils_uri_get_lines( uri, ofa_stream_format_get_charmap( format ), NULL, NULL );

	if( lines ){
		for( i=0 ; st_import_formats[i].label ; ++i ){
			if( st_import_formats[i].fnTest( self, format, lines )){
				ok = TRUE;
				break;
			}
		}
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	}

	g_object_unref( format );

	return( ok );
}

static guint
iimporter_import( ofaIImporter *instance, ofsImporterParms *parms )
{
	g_return_val_if_fail( parms->hub && OFA_IS_HUB( parms->hub ), 1 );
	g_return_val_if_fail( my_strlen( parms->uri ), 1 );
	g_return_val_if_fail( parms->format && OFA_IS_STREAM_FORMAT( parms->format ), 1 );
	g_return_val_if_fail( ofa_stream_format_get_has_field( parms->format ), 1 );

	return( do_parse( OFA_IMPORTER_TXT_LCL( instance ), parms ));
}

static guint
do_parse( ofaImporterTxtLcl *self, ofsImporterParms *parms )
{
	GSList *lines;
	gchar *msgerr;
	gint i, idx, errors;

	errors = 0;
	msgerr = NULL;

	lines = my_utils_uri_get_lines( parms->uri, ofa_stream_format_get_charmap( parms->format ), &errors, &msgerr );
	if( msgerr ){
		if( parms->progress ){
			my_iprogress_set_text( parms->progress, msgerr );
		}
		g_free( msgerr );
		if( lines ){
			g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		}
		return( errors );
	}

	if( lines ){
		idx = -1;
		for( i=0 ; st_import_formats[i].label ; ++i ){
			if( st_import_formats[i].fnTest( self, parms->format, lines )){
				idx = i;
				break;
			}
		}
		if( idx >= 0 ){
			errors = st_import_formats[i].fnImport( self, parms, lines )
		}
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	}

	return( errors );
}

static gboolean
lcl_tabulated_text_v1_check( ofaImporterTxtLcl *importer_txt_lcl )
{
	ofaImporterTxtLclPrivate *priv;
	gchar **tokens, **iter;
	GDate date;

	priv = importer_txt_lcl->priv;
	tokens = g_strsplit( priv->lines->data, "\t", -1 );
	/* only interpret first line */
	/* first field = value date */
	iter = tokens;
	if( !scan_date_dmyy( &date, *iter )){
		return( FALSE );
	}
	iter += 1;
	if( !get_double( *iter )){
		return( FALSE );
	}
	/* other fields may be empty */

	priv->count = g_slist_length( priv->lines )-1;
	g_strfreev( tokens );

	return( TRUE );
}

static ofsBat  *
lcl_tabulated_text_v1_import( ofaImporterTxtLcl *importer_txt_lcl )
{
	ofaImporterTxtLclPrivate *priv;
	GSList *line;
	ofsBat *sbat;
	ofsBatDetail *sdet;
	gchar **tokens, **iter;
	gint count, nb;
	gchar *msg, *sbegin, *send;

	priv = importer_txt_lcl->priv;
	line = priv->lines;
	sbat = g_new0( ofsBat, 1 );
	nb = g_slist_length( priv->lines );
	count = 0;
	priv->errors = 0;

	while( TRUE ){
		if( !line || !my_strlen( line->data )){
			break;
		}
		tokens = g_strsplit( line->data, "\t", -1 );
		iter = tokens;
		count += 1;

		/* detail line */
		if( count < nb ){
			sdet = g_new0( ofsBatDetail, 1 );
			ofa_iimportable_increment_progress( OFA_IIMPORTABLE( importer_txt_lcl ), IMPORTABLE_PHASE_IMPORT, 1 );

			scan_date_dmyy( &sdet->deffect, *iter );

			iter += 1;
			sdet->amount = get_double( *iter );

			iter += 1;
			if( my_strlen( *iter )){
				sdet->ref = g_strdup( lcl_get_ref_paiement( *iter ));
			}

			iter += 1;
			sdet->label = lcl_concatenate_labels( &iter );

			/* do not interpret
			 * unknown field nor category */
			sbat->details = g_list_prepend( sbat->details, sdet );

		/* last line is the file footer */
		} else {
			scan_date_dmyy( &sbat->end, *iter );

			iter +=1 ;
			sbat->end_solde = get_double( *iter );
			sbat->end_solde_set = TRUE;

			iter += 1;
			/* no ref */

			iter += 1;
			sbat->rib = lcl_concatenate_labels( &iter );

			if( ofo_bat_exists( priv->hub, sbat->rib, &sbat->begin, &sbat->end )){
				sbegin = my_date_to_str( &sbat->begin, ofa_prefs_date_display());
				send = my_date_to_str( &sbat->end, ofa_prefs_date_display());
				msg = g_strdup_printf( _( "Already imported BAT file: RIB=%s, begin=%s, end=%s" ),
						sbat->rib, sbegin, send );
				ofa_iimportable_set_message( OFA_IIMPORTABLE( importer_txt_lcl ), nb, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( sbegin );
				g_free( send );
				priv->errors += 1;
				ofs_bat_free( sbat );
				sbat = NULL;
			}
		}

		g_strfreev( tokens );
		line = line->next;
	}

	return( sbat );
}

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

	if( my_strlen( str )){
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
		if( my_strlen( lab2 )){
			if( my_strlen( lab1->str )){
				lab1 = g_string_append( lab1, " " );
			}
			lab1 = g_string_append( lab1, lab2 );
		}
		g_free( lab2 );

		*iter += 1;
		if( **iter ){
			lab2 = g_strdup( **iter );
			g_strstrip( lab2 );
			if( my_strlen( lab2 )){
				if( my_strlen( lab1->str )){
					lab1 = g_string_append( lab1, " " );
				}
				lab1 = g_string_append( lab1, lab2 );
			}
			g_free( lab2 );

			*iter += 1;
			if( my_strlen( **iter )){
				lab2 = g_strdup( **iter );
				g_strstrip( lab2 );
				if( my_strlen( lab2 )){
					if( my_strlen( lab1->str )){
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

/*
 * amounts are expected to use comma as decimal separator
 *  and no thousand separator
 */
static gdouble
get_double( const gchar *str )
{
	gchar *dotsep;
	gdouble amount;

	dotsep = my_utils_str_replace( str, ",", "." );
	amount = my_double_set_from_sql( dotsep );
	g_free( dotsep );

	return( amount );
}
