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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <api/ofa-iimporter.h>

#include "of1-importer.h"

/* private instance data
 */
struct _of1ImporterPrivate {
	gboolean  dispose_has_run;

	/* input parameters
	 */
	ofaIImporterParms *parms;

	/* file data
	 */
	GSList            *content;
	gchar             *etag;
};

typedef struct {
	const gchar *label;
	gint         type;
	gint       (*fn)( of1Importer * );
}
	ImportFormat;

static gint import_bourso_excel95_v1    ( of1Importer *importer );
static gint import_bourso_excel2002_v1  ( of1Importer *importer );
static gint import_lcl_tabulated_text_v1( of1Importer *importer );

static ImportFormat st_import_formats[] = {
		{ "Boursorama - Excel 95",        IMPORTER_TYPE_BAT1, import_bourso_excel95_v1 },
		{ "Boursorama - Excel 2002",      IMPORTER_TYPE_BAT1, import_bourso_excel2002_v1 },
		{ "LCL - Excel (tabulated text)", IMPORTER_TYPE_BAT1, import_lcl_tabulated_text_v1 },
		{ 0 }
};

static GType         st_module_type = 0;
static GObjectClass *st_parent_class = NULL;

static void         class_init( of1ImporterClass *klass );
static void         instance_init( GTypeInstance *instance, gpointer klass );
static void         instance_dispose( GObject *object );
static void         instance_finalize( GObject *object );

static void         iimporter_iface_init( ofaIImporterInterface *iface );
static guint        iimporter_get_interface_version( const ofaIImporter *importer );

static guint        of1_importer_import_from_uri( const ofaIImporter *importer, ofaIImporterParms *parms );
static gint         import_bourso_tabulated_text_v1( of1Importer *importer, const gchar *thisfn );
static gchar       *strip_field( gchar *begin );
static gdouble      get_double( const gchar *str );
static const gchar *find_lcl_ref_paiement( const gchar *str );
static gchar       *concatenate_labels( gchar ***iter );

GType
of1_importer_get_type( void )
{
	return( st_module_type );
}

void
of1_importer_register_type( GTypeModule *module )
{
	static const gchar *thisfn = "of1_importer_register_type";

	static GTypeInfo info = {
		sizeof( of1ImporterClass ),
		NULL,
		NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( of1Importer ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	static const GInterfaceInfo iimporter_iface_info = {
		( GInterfaceInitFunc ) iimporter_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	st_module_type = g_type_module_register_type( module, G_TYPE_OBJECT, "of1Importer", &info, 0 );

	g_type_module_add_interface( module, st_module_type, OFA_TYPE_IIMPORTER, &iimporter_iface_info );
}

static void
class_init( of1ImporterClass *klass )
{
	static const gchar *thisfn = "of1_importer_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = instance_dispose;
	G_OBJECT_CLASS( klass )->finalize = instance_finalize;
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "of1_importer_instance_init";
	of1Importer *self;

	g_return_if_fail( OF1_IS_IMPORTER( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OF1_IMPORTER( instance );

	self->private = g_new0( of1ImporterPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *object )
{
	of1Importer *self;

	g_return_if_fail( OF1_IS_IMPORTER( object ));

	self = OF1_IMPORTER( object );

	if( !self->private->dispose_has_run ){

		self->private->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->dispose( object );
}

static void
instance_finalize( GObject *object )
{
	static const gchar *thisfn = "of1_importer_instance_finalize";
	of1ImporterPrivate *priv;

	g_return_if_fail( OF1_IS_IMPORTER( object ));

	priv = OF1_IMPORTER( object )->private;

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	/* free data members here */
	g_slist_free_full( priv->content, ( GDestroyNotify ) g_free );
	g_free( priv->etag );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( st_parent_class )->finalize( object );
}

static void
iimporter_iface_init( ofaIImporterInterface *iface )
{
	static const gchar *thisfn = "of1_importer_iimporter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimporter_get_interface_version;
	iface->import_from_uri = of1_importer_import_from_uri;
}

static guint
iimporter_get_interface_version( const ofaIImporter *importer )
{
	return( 1 );
}

static guint
of1_importer_import_from_uri( const ofaIImporter *importer, ofaIImporterParms *parms )
{
	static const gchar *thisfn = "of1_importer_import_from_uri";
	of1ImporterPrivate *priv;
	GFile *gfile;
	gchar *contents;
	GError *error;
	gchar *str;
	guint code;
	gint i;
	gchar **lines, **iter;

	g_debug( "%s: importer=%p, parms=%p, uri=%s",
			thisfn, ( void * ) importer, parms, parms->uri );

	priv = OF1_IMPORTER( importer )->private;
	priv->parms = parms;

	code = IMPORTER_CODE_NOT_WILLING_TO;

	gfile = g_file_new_for_uri( parms->uri );
	error = NULL;
	if( !g_file_load_contents( gfile, NULL, &contents, NULL, &priv->etag, &error )){
		str = g_strdup( error->message );
		if( parms->messages ){
			parms->messages = g_slist_append( parms->messages, str );
		}
		g_debug( "%s: %s", thisfn, str );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}

	lines = g_strsplit( contents, "\n", -1 );
	g_free( contents );
	iter = lines;
	priv->content = NULL;
	while( *iter ){
		priv->content = g_slist_prepend( priv->content, g_strstrip( g_strdup( *iter )));
		iter++;
	}
	g_strfreev( lines );
	priv->content = g_slist_reverse( priv->content );
	g_debug( "%s: %d lines found", thisfn, g_slist_length( priv->content ));

	for( i=0 ; st_import_formats[i].label && code!=IMPORTER_CODE_OK ; ++i ){

		parms->type = st_import_formats[i].type;
		parms->batv1.format = g_strdup( st_import_formats[i].label );
		code = st_import_formats[i].fn( OF1_IMPORTER( importer ));

		if( code != IMPORTER_CODE_OK ){

			str = g_strdup_printf( "%s: %s", st_import_formats[i].label, _( "unable to parse "));
			if( parms->messages ){
				parms->messages = g_slist_append( parms->messages, str );
			}
			g_debug( "%s: %s", thisfn, str );

			ofa_iimporter_free_output( parms );
		}
	}

	return( code );
}

/*
 * As of 2014- 6- 1:
 * -----------------
 * "*** P<E9>riode : 01/01/2014 - 01/06/2014"
 * "*** Compte : 40618-80264-00040200033    -EUR "
 *
 * "DATE OPERATION"        "DATE VALEUR"   "LIBELLE"       "MONTANT"       "DEVISE"
 * " 02/01/2014"   " 02/01/2014"   "*PRLV Cotisat. Boursorama Protection 0  "      -00000000001,50 "EUR "
 * " 10/01/2014"   " 10/01/2014"   "TIP CFAB COMPTE REGLEMENT TI            "      -00000000220,02 "EUR "
 *
 * where spaces are tabulations
 */
static gint
import_bourso_excel95_v1( of1Importer *importer )
{
	static const gchar *thisfn = "of1_importer_import_bourso_excel95_v1";

	return( import_bourso_tabulated_text_v1( importer, thisfn ));
}

/*
 * note this definition is only for consistancy
 * if import_bourso_excel95() works fine on the input file, this
 * function will never be called
 */
static gint
import_bourso_excel2002_v1( of1Importer *importer )
{
	static const gchar *thisfn = "of1_importer_import_bourso_excel2002_v1";

	return( import_bourso_tabulated_text_v1( importer, thisfn ));
}

static gint
import_bourso_tabulated_text_v1( of1Importer *importer, const gchar *thisfn )
{
	of1ImporterPrivate *priv;
	ofaIImporterBatv1 *output;
	GSList *line;
	const gchar *str;
	gchar *found;
	gint bd, bm, by, ed, em, ey;
	ofaIImporterSBatv1 *bat;
	gchar **tokens, **iter;

	priv = importer->private;
	output = &priv->parms->batv1;
	output->count = 0;
	output->results = NULL;

	line = priv->content;
	str = line->data;
	g_debug( "%s: str='%s'", thisfn, str );
	if( !g_str_has_prefix( str, "\"*** P" )){
		g_debug( "%s: no '\"*** P' prefix", thisfn );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	found = g_strstr_len( str, -1, "riode : " );
	if( !found ){
		g_debug( "%s: 'riode : ' not found", thisfn );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	sscanf( found+9, "%d/%d/%d - %d/%d/%d", &bd, &bm, &by, &ed, &em, &ey );
	if( bd < 0 || bm < 0 || by < 0 || ed < 0 || em < 0 || ey < 0 ){
		g_debug( "%s: bd=%d, bm=%d, by=%d, ed=%d, em=%d, ey=%d", thisfn, bd, bm, by, ed, em, ey );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	if( bd > 31 || ed > 31 || bm > 12 || em > 12 ){
		g_debug( "%s: bd=%d, bm=%d, ed=%d, em=%d", thisfn, bd, bm, ed, em );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	g_date_clear( &output->begin, 1 );
	g_date_set_dmy( &output->begin, bd, bm, by );
	if( !g_date_valid( &output->begin )){
		g_debug( "%s: invalid begin date", thisfn );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	g_date_clear( &output->end, 1 );
	g_date_set_dmy( &output->end, ed, em, ey );
	if( !g_date_valid( &output->end )){
		g_debug( "%s: invalid end date", thisfn );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}

	line = line->next;
	str = line->data;
	g_debug( "%s: str='%s'", thisfn, str );
	if( !g_str_has_prefix( str, "\"*** Compte : " )){
		g_debug( "%s: no '\"*** Compte : ' prefix", thisfn );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	output->rib = g_strstrip( g_strndup( str+14, 24 ));
	found = g_strstr_len( str+38, -1, " -" );
	if( !found ){
		g_debug( "%s: ' -' not found", thisfn );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}
	output->currency = g_strndup( found+2, 3 );

	output->solde_set = FALSE;

	line = line->next;
	str = line->data;
	if( strlen( str )){
		g_debug( "%s: not empty '%s'", thisfn, str );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}

	line = line->next;
	str = line->data;
	if( !g_ascii_strcasecmp( str, "\"DATE OPERATION\"        \"DATE VALEUR\"   \"LIBELLE\"       \"MONTANT\"       \"DEVISE\"" )){
		g_debug( "%s: header not found: '%s'", thisfn, str );
		return( IMPORTER_CODE_UNABLE_TO_PARSE );
	}

	while( TRUE ){
		line = line->next;
		if( !line || !line->data || !g_utf8_strlen( line->data, -1 )){
			break;
		}
		bat = g_new0( ofaIImporterSBatv1, 1 );
		tokens = g_strsplit( line->data, "\t", -1 );
		iter = tokens;

		found = strip_field( *iter );
		/*g_debug( "%s: found dope='%s'", thisfn, found );*/
		sscanf( found, "%d/%d/%d", &bd, &bm, &by );
		g_free( found );
		if( bd < 0 || bm < 0 || by < 0 || bd > 31 || bm > 12 ){
			g_debug( "%s: bd=%d, bm=%d, by=%d", thisfn, bd, bm, by );
			return( IMPORTER_CODE_UNABLE_TO_PARSE );
		}
		g_date_clear( &bat->dope, 1 );
		g_date_set_dmy( &bat->dope, bd, bm, by );
		if( !g_date_valid( &bat->dope )){
			g_debug( "%s: invalid ope date", thisfn );
			return( IMPORTER_CODE_UNABLE_TO_PARSE );
		}

		iter += 1;
		found = strip_field( *iter );
		/*g_debug( "%s: found valeur='%s'", thisfn, found );*/
		sscanf( found, "%d/%d/%d", &bd, &bm, &by );
		g_free( found );
		if( bd < 0 || bm < 0 || by < 0 || bd > 31 || bm > 12 ){
			g_debug( "%s: bd=%d, bm=%d, by=%d", thisfn, bd, bm, by );
			return( IMPORTER_CODE_UNABLE_TO_PARSE );
		}
		g_date_clear( &bat->dvaleur, 1 );
		g_date_set_dmy( &bat->dvaleur, bd, bm, by );
		if( !g_date_valid( &bat->dvaleur )){
			g_debug( "%s invalid valeur date", thisfn );
			return( IMPORTER_CODE_UNABLE_TO_PARSE );
		}

		iter += 1;
		found = strip_field( *iter );
		/*g_debug( "%s: found='%s'", thisfn, found );*/
		bat->label = found;

		iter +=1 ;
		found = *iter;
		bat->amount = get_double( found );
		g_debug( "%s: str='%s', amount=%lf", thisfn, found, bat->amount );

		iter += 1;
		found = strip_field( *iter );
		/*g_debug( "%s: found='%s'", thisfn, found );*/
		bat->currency = found;

		output->count += 1;
		output->results = g_list_prepend( output->results, bat );
		g_strfreev( tokens );
	}

	output->results = g_list_reverse( output->results );
	return( IMPORTER_CODE_OK );
}

/*
 * As of 2014- 6- 1:
 * ----------------
 * 17/04/2014 -> -80,0   -> Carte     ->->       CB  BUFFETTI STILO   15/04/14                          0       Divers
 * 18/04/2014 -> 10000,0 -> Virement  ->->-> VIREMENT WIESER
 * 23/04/2014 -> -12,0   -> Ch<E8>que -> 8341505 ->->->->->
 *
 * Last line is the balance of the account.
 *
 * Ref may be:
 *  'Carte'        -> 'CB'
 *  'Virement'     -> 'Vir.'
 *  'Prélèvement'  -> 'Pr.'
 *  'Chèque'       -> 'Ch.'
 *  'TIP'          -> 'TIP'
 *
 * There is an unknown field, maybe empty, most of time at zero
 *
 * The category may be empty. Most of time, it is set. When it is set,
 * it is always equal to 'Divers'.
 *
 * The unknown field and the category arealways set, or unset, together.
 */

static gint
import_lcl_tabulated_text_v1( of1Importer *importer )
{
	static const gchar *thisfn = "of1_importer_import_lcl_tabulated_text";
	of1ImporterPrivate *priv;
	ofaIImporterBatv1 *output;
	GSList *line;
	gint bd, bm, by;
	ofaIImporterSBatv1 *bat;
	gchar **tokens, **iter;
	gint nb;

	priv = importer->private;
	output = &priv->parms->batv1;
	output->count = 0;

	line = priv->content;
	nb = g_slist_length( priv->content );

	while( TRUE ){
		if( !line || !line->data || !g_utf8_strlen( line->data, -1 )){
			break;
		}
		tokens = g_strsplit( line->data, "\t", -1 );
		iter = tokens;
		output->count += 1;

		if( output->count < nb ){
			bat = g_new0( ofaIImporterSBatv1, 1 );
			g_date_clear( &bat->dope, 1 );
			/*g_debug( "%s: str='%s'", thisfn, ( gchar * ) line->data );*/

			sscanf( *iter, "%d/%d/%d", &bd, &bm, &by );
			if( bd < 0 || bm < 0 || by < 0 || bd > 31 || bm > 12 ){
				g_debug( "%s: bd=%d, bm=%d, by=%d", thisfn, bd, bm, by );
				return( IMPORTER_CODE_UNABLE_TO_PARSE );
			}
			g_date_clear( &bat->dvaleur, 1 );
			g_date_set_dmy( &bat->dvaleur, bd, bm, by );
			if( !g_date_valid( &bat->dvaleur )){
				g_debug( "%s invalid valeur date", thisfn );
				return( IMPORTER_CODE_UNABLE_TO_PARSE );
			}

			iter +=1 ;
			bat->amount = get_double( *iter );

			iter += 1;
			if( *iter && g_utf8_strlen( *iter, -1 )){
				bat->ref = g_strdup( find_lcl_ref_paiement( *iter ));
			}

			iter += 1;
			bat->label = concatenate_labels( &iter );
			g_debug( "%s: nb=%d, count=%d, label='%s' amount=%lf", thisfn, nb, output->count, bat->label, bat->amount );

			/* do not interpret
			 * unknown field nor category */
			output->results = g_list_prepend( output->results, bat );

		} else {
			sscanf( *iter, "%d/%d/%d", &bd, &bm, &by );
			if( bd < 0 || bm < 0 || by < 0 || bd > 31 || bm > 12 ){
				g_debug( "%s: bd=%d, bm=%d, by=%d", thisfn, bd, bm, by );
				return( IMPORTER_CODE_UNABLE_TO_PARSE );
			}
			g_date_clear( &output->end, 1 );
			g_date_set_dmy( &output->end, bd, bm, by );
			if( !g_date_valid( &output->end )){
				g_debug( "%s invalid end date", thisfn );
				return( IMPORTER_CODE_UNABLE_TO_PARSE );
			}

			iter +=1 ;
			output->solde = get_double( *iter );
			output->solde_set = TRUE;

			iter += 1;
			/* no ref */

			iter += 1;
			output->rib = concatenate_labels( &iter );
			g_debug( "%s: account='%s' balance=%lf", thisfn, output->rib, output->solde );
		}

		g_strfreev( tokens );
		line = line->next;
	}

	output->results = g_list_reverse( output->results );
	return( IMPORTER_CODE_OK );
}

/*
 * returns the next unstripped double-quoted field, or NULL
 * as a newly allocated string
 * advance 'begin' to the character after the second quote
 */
#if 0
static gchar *
iter_next_field( gchar **begin )
{
	gchar *end;
	gchar *field;

	while( *begin[0] && *begin[0] != '"' ){
		*begin += 1;
	}
	if( !*begin[0] || !*begin[1] ){
		return( NULL );
	}
	*begin += 1;	/* first char after the first double-quote */
	end = *begin;
	while( end[0] && end[0] != '"' ){
		end += 1;
	}
	end += 1;	/* first char after the second double-quote */
	field = g_strstrip( g_strndup( *begin, end-*begin-1 ));
	begin = &end;

	return( field );
}
#endif

static gchar *
strip_field( gchar *str )
{
	gchar *s1, *s2, *e1, *e2;
	gchar *dest;

	s1 = g_utf8_strchr( str, -1, '"' );
	s2 = g_utf8_find_next_char( s1, NULL );
	e1 = g_utf8_strrchr( s2, -1, '"' );
	e2 = g_utf8_find_prev_char( s2, e1 );

	dest = g_strndup( s2, e2-s2+1 );

	return( g_strstrip ( dest ));
}

static gdouble
get_double( const gchar *str )
{
	static const gchar *thisfn = "of1_importer_get_double";
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
find_lcl_ref_paiement( const gchar *str )
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
concatenate_labels( gchar ***iter )
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
