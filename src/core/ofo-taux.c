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

#include <stdlib.h>
#include <string.h>

#include "core/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-model.h"
#include "api/ofo-sgbd.h"
#include "api/ofo-taux.h"

/* priv instance data
 */
struct _ofoTauxPrivate {

	/* sgbd data
	 */
	gchar     *mnemo;
	gchar     *label;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
	GList     *valids;
};

/* these are sgbd datas for each validitiy period
 */
typedef struct {
	GDate    begin;
	GDate    end;
	gdouble  rate;
}
	sTauxValid;

#if 0
/* the structure used when searching for a rate by mnemo,
 *  at a specified date (cf. ofo_taux_get_by_mnemo())
 */
typedef struct {
	const gchar *mnemo;
	const GDate *date;
}
	sCmpMneDat;
#endif

G_DEFINE_TYPE( ofoTaux, ofo_taux, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, taux )

static GList         *taux_load_dataset( void );
static void           taux_set_val_begin( sTauxValid *tv, const GDate *date );
static void           taux_set_val_end( sTauxValid *tv, const GDate *date );
static void           taux_set_val_taux( sTauxValid *tv, gdouble rate );
static ofoTaux       *taux_find_by_mnemo( GList *set, const gchar *mnemo );
static void           taux_add_val_detail( ofoTaux *taux, sTauxValid *detail );
static gboolean       taux_do_insert( ofoTaux *taux, const ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_insert_main( ofoTaux *taux, const ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_delete_validities( ofoTaux *taux, const ofoSgbd *sgbd );
static gboolean       taux_insert_validities( ofoTaux *taux, const ofoSgbd *sgbd );
static gboolean       taux_insert_validity( ofoTaux *taux, sTauxValid *sdet, const ofoSgbd *sgbd );
static gboolean       taux_do_update( ofoTaux *taux, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_update_main( ofoTaux *taux, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_do_delete( ofoTaux *taux, const ofoSgbd *sgbd );
static gint           taux_cmp_by_mnemo( const ofoTaux *a, const gchar *mnemo );
static gint           taux_cmp_by_vdata( sTauxVData *a, sTauxVData *b, gboolean *consistent );
static ofoTaux       *taux_import_csv_taux( GSList *fields, gint count, gint *errors );
static sTauxValid    *taux_import_csv_valid( GSList *fields, gint count, gint *errors, gchar **mnemo );
static gboolean       taux_do_drop_content( const ofoSgbd *sgbd );

static void
taux_free_validity( sTauxValid *sval )
{
	g_free( sval );
}

static void
taux_free_validities( ofoTaux *taux )
{
	g_list_free_full( taux->private->valids, ( GDestroyNotify ) taux_free_validity );
	taux->private->valids = NULL;
}

static void
ofo_taux_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_taux_finalize";
	ofoTauxPrivate *priv;

	priv = OFO_TAUX( instance )->private;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->maj_user );

	taux_free_validities( OFO_TAUX( instance ));

	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_taux_parent_class )->finalize( instance );
}

static void
ofo_taux_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_TAUX( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_taux_parent_class )->dispose( instance );
}

static void
ofo_taux_init( ofoTaux *self )
{
	static const gchar *thisfn = "ofo_taux_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoTauxPrivate, 1 );
}

static void
ofo_taux_class_init( ofoTauxClass *klass )
{
	static const gchar *thisfn = "ofo_taux_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_taux_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_taux_finalize;
}

/**
 * ofo_taux_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoTaux rates, ordered by ascending
 * mnemonic. The returned list is owned by the #ofoTaux class, and
 * should not be freed by the caller.
 */
GList *
ofo_taux_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_taux_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

	return( st_global->dataset );
}

static GList *
taux_load_dataset( void )
{
	const ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	ofoTaux *taux;
	GList *dataset, *it;
	sTauxValid *valid;
	gchar *query;

	dataset = NULL;
	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT TAX_MNEMO,TAX_LABEL,TAX_NOTES,"
			"	TAX_MAJ_USER,TAX_MAJ_STAMP"
			"	FROM OFA_T_TAUX" );

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		taux = ofo_taux_new();
		ofo_taux_set_mnemo( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_label( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_notes( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_maj_user( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str(( gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, taux );
	}

	ofo_sgbd_free_result( result );

	for( it=dataset ; it ; it=it->next ){

		taux = OFO_TAUX( it->data );
		taux->private->valids = NULL;

		query = g_strdup_printf(
					"SELECT TAX_VAL_DEB,TAX_VAL_FIN,TAX_VAL_TAUX"
					"	FROM OFA_T_TAUX_VAL "
					"	WHERE TAX_MNEMO='%s'",
					ofo_taux_get_mnemo( taux ));

		result = ofo_sgbd_query_ex( sgbd, query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			valid = g_new0( sTauxValid, 1 );
			taux_set_val_begin( valid, my_utils_date_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			taux_set_val_end( valid, my_utils_date_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			if( icol->data ){
				taux_set_val_taux( valid, g_ascii_strtod(( gchar * ) icol->data, NULL ));
			}

			taux->private->valids = g_list_prepend( taux->private->valids, valid );
		}

		ofo_sgbd_free_result( result );
		g_free( query );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_taux_get_by_mnemo:
 *
 * Returns: the searched taux, or %NULL.
 *
 * The returned object is owned by the #ofoTaux class, and should
 * not be unreffed by the caller.
 */
ofoTaux *
ofo_taux_get_by_mnemo( const ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

	return( taux_find_by_mnemo( st_global->dataset, mnemo ));
}

static ofoTaux *
taux_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) taux_cmp_by_mnemo );
	if( found ){
		return( OFO_TAUX( found->data ));
	}

	return( NULL );
}

/**
 * ofo_taux_new:
 */
ofoTaux *
ofo_taux_new( void )
{
	ofoTaux *taux;

	taux = g_object_new( OFO_TYPE_TAUX, NULL );

	return( taux );
}

/**
 * ofo_taux_get_mnemo:
 */
const gchar *
ofo_taux_get_mnemo( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		return(( const gchar * ) taux->private->mnemo );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_label:
 */
const gchar *
ofo_taux_get_label( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		return(( const gchar * ) taux->private->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_notes:
 */
const gchar *
ofo_taux_get_notes( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		return(( const gchar * ) taux->private->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_maj_user:
 */
const gchar *
ofo_taux_get_maj_user( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		return(( const gchar * ) taux->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_maj_stamp:
 */
const GTimeVal *
ofo_taux_get_maj_stamp( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &taux->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_min_valid:
 */
const GDate *
ofo_taux_get_min_valid( const ofoTaux *taux )
{
	GList *iv;
	const GDate *min;
	sTauxValid *sval;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		for (min=NULL, iv=taux->private->valids ; iv ; iv=iv->next ){
			sval = ( sTauxValid * ) iv->data;
			if( !min ){
				min = &sval->begin;
			} else if( my_utils_date_cmp( &sval->begin, min, TRUE ) < 0 ){
				min = &sval->begin;
			}
		}

		return( min );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_max_valid:
 */
const GDate *
ofo_taux_get_max_valid( const ofoTaux *taux )
{
	GList *iv;
	const GDate *max;
	sTauxValid *sval;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		for (max=NULL, iv=taux->private->valids ; iv ; iv=iv->next ){
			sval = ( sTauxValid * ) iv->data;
			if( !max ){
				max = &sval->end;
			} else if( my_utils_date_cmp( &sval->end, max, FALSE ) > 0 ){
				max = &sval->end;
			}
		}

		return( max );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_add_val:
 */
void
ofo_taux_add_val( ofoTaux *taux, const gchar *begin, const gchar *end, const char *rate )
{
	sTauxValid *sval;

	g_return_if_fail( taux && OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		sval = g_new0( sTauxValid, 1 );
		g_date_set_parse( &sval->begin, begin );
		g_date_set_parse( &sval->end, end );
		sval->rate = g_ascii_strtod( rate, NULL );
		taux_add_val_detail( taux, sval );
	}
}

static void
taux_add_val_detail( ofoTaux *taux, sTauxValid *detail )
{
	taux->private->valids = g_list_append( taux->private->valids, detail );
}

/**
 * ofo_taux_free_val_all:
 *
 * Clear all validities of the rate object.
 * This is normally done just before adding new validities, when
 * preparing for a sgbd update.
 */
void
ofo_taux_free_val_all( ofoTaux *taux )
{

	g_return_if_fail( taux && OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		taux_free_validities( taux );
	}
}

/**
 * ofo_taux_get_val_count:
 */
gint
ofo_taux_get_val_count( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), 0 );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		return( g_list_length( taux->private->valids ));
	}

	return( 0 );
}

/**
 * ofo_taux_get_val_begin:
 */
const GDate *
ofo_taux_get_val_begin( const ofoTaux *taux, gint idx )
{
	GList *nth;
	sTauxValid *rate;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		nth = g_list_nth( taux->private->valids, idx );
		if( nth ){
			rate = ( sTauxValid * ) nth->data;
			return(( const GDate * ) &rate->begin );
		}
	}

	return( NULL );
}

/**
 * ofo_taux_get_val_end:
 */
const GDate *
ofo_taux_get_val_end( const ofoTaux *taux, gint idx )
{
	GList *nth;
	sTauxValid *rate;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		nth = g_list_nth( taux->private->valids, idx );
		if( nth ){
			rate = ( sTauxValid * ) nth->data;
			return(( const GDate * ) &rate->end );
		}
	}

	return( NULL );
}

/**
 * ofo_taux_get_val_rate:
 */
gdouble
ofo_taux_get_val_rate( const ofoTaux *taux, gint idx )
{
	GList *nth;
	sTauxValid *rate;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), 0 );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		nth = g_list_nth( taux->private->valids, idx );
		if( nth ){
			rate = ( sTauxValid * ) nth->data;
			return( rate->rate );
		}
	}

	return( 0 );
}

/**
 * ofo_taux_get_rate_at_date:
 */
gdouble
ofo_taux_get_rate_at_date( const ofoTaux *taux, const GDate *date )
{
	GList *iva;
	sTauxValid *svalid;

	g_return_val_if_fail( taux && OFO_IS_TAUX( taux ), 0 );
	g_return_val_if_fail( date && g_date_valid( date ), 0 );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		for( iva=taux->private->valids ; iva ; iva=iva->next ){
			svalid = ( sTauxValid * ) iva->data;

			if( g_date_valid( &svalid->begin )){
				if( g_date_compare( &svalid->begin, date ) > 0 ){
					continue;
				}
				if( !g_date_valid( &svalid->end ) || g_date_compare( &svalid->end, date ) >= 0 ){
					return( svalid->rate );
				}
			} else {
				if( !g_date_valid( &svalid->end ) || g_date_compare( &svalid->end, date ) >= 0 ){
					return( svalid->rate );
				}
			}
		}
	}

	return( 0 );
}

/**
 * ofo_taux_is_deletable:
 *
 * A rate cannot be deleted if it is referenced in the debit or the
 * credit formulas of a model detail line.
 */
gboolean
ofo_taux_is_deletable( const ofoTaux *taux )
{
	ofoDossier *dossier;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		dossier = OFO_DOSSIER( st_global->dossier );

		return( !ofo_model_use_taux( dossier, ofo_taux_get_mnemo( taux )));
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_taux_is_valid:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate mnemo or so.
 *
 * In order to check that all provided periods of validity are
 * consistent between each others, we are trying to sort them from the
 * infinite past to the infinite future - if this doesn't work
 * (probably because overlapping each others), then the provided data
 * is not valid
 */
gboolean
ofo_taux_is_valid( const gchar *mnemo, const gchar *label, GList *validities )
{
	gboolean ok;
	gboolean consistent;

	ok = mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 );

	consistent = TRUE;
	validities = g_list_sort_with_data(
			validities, ( GCompareDataFunc ) taux_cmp_by_vdata, &consistent );
	ok &= consistent;

	return( ok );
}

/**
 * ofo_taux_set_mnemo:
 */
void
ofo_taux_set_mnemo( ofoTaux *taux, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_free( taux->private->mnemo );
		taux->private->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_taux_set_label:
 */
void
ofo_taux_set_label( ofoTaux *taux, const gchar *label )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_free( taux->private->label );
		taux->private->label = g_strdup( label );
	}
}

/**
 * ofo_taux_set_notes:
 */
void
ofo_taux_set_notes( ofoTaux *taux, const gchar *notes )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_free( taux->private->notes );
		taux->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_taux_set_maj_user:
 */
void
ofo_taux_set_maj_user( ofoTaux *taux, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_free( taux->private->maj_user );
		taux->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_taux_set_maj_stamp:
 */
void
ofo_taux_set_maj_stamp( ofoTaux *taux, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		memcpy( &taux->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

static void
taux_set_val_begin( sTauxValid *tv, const GDate *date )
{
	memcpy( &tv->begin, date, sizeof( GDate ));
}

static void
taux_set_val_end( sTauxValid *tv, const GDate *date )
{
	memcpy( &tv->end, date, sizeof( GDate ));
}

static void
taux_set_val_taux( sTauxValid *tv, gdouble value )
{
	tv->rate = value;
}

/**
 * ofo_taux_insert:
 *
 * First creation of a new rate. This may contain zéro to n validity
 * datail rows. But, if it doesn't, then we take care of removing all
 * previously existing old validity rows.
 */
gboolean
ofo_taux_insert( ofoTaux *taux )
{
	static const gchar *thisfn = "ofo_taux_insert";

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_debug( "%s: taux=%p", thisfn, ( void * ) taux );

		if( taux_do_insert(
					taux,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, taux );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
taux_do_insert( ofoTaux *taux, const ofoSgbd *sgbd, const gchar *user )
{
	return( taux_insert_main( taux, sgbd, user ) &&
			taux_delete_validities( taux, sgbd ) &&
			taux_insert_validities( taux, sgbd ));
}

static gboolean
taux_insert_main( ofoTaux *taux, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_taux_get_label( taux ));
	notes = my_utils_quote( ofo_taux_get_notes( taux ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_TAUX" );

	g_string_append_printf( query,
			"	(TAX_MNEMO,TAX_LABEL,TAX_NOTES,"
			"	TAX_MAJ_USER, TAX_MAJ_STAMP) VALUES ('%s','%s',",
			ofo_taux_get_mnemo( taux ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_taux_set_maj_user( taux, user );
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

static gboolean
taux_delete_validities( ofoTaux *taux, const ofoSgbd *sgbd )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TAUX_VAL WHERE TAX_MNEMO='%s'",
					ofo_taux_get_mnemo( taux ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gboolean
taux_insert_validities( ofoTaux *taux, const ofoSgbd *sgbd )
{
	gboolean ok;
	GList *idet;
	sTauxValid *sdet;

	ok = TRUE;
	for( idet=taux->private->valids ; idet ; idet=idet->next ){
		sdet = ( sTauxValid * ) idet->data;
		ok &= taux_insert_validity( taux, sdet, sgbd );
	}

	return( ok );
}

static gboolean
taux_insert_validity( ofoTaux *taux, sTauxValid *sdet, const ofoSgbd *sgbd )
{
	gboolean ok;
	GString *query;
	gchar *dbegin, *dend, *rate;

	dbegin = my_utils_sql_from_date( &sdet->begin );
	dend = my_utils_sql_from_date( &sdet->end );
	rate = my_utils_sql_from_double( sdet->rate );

	query = g_string_new( "INSERT INTO OFA_T_TAUX_VAL " );

	g_string_append_printf( query,
			"	(TAX_MNEMO,"
			"	TAX_VAL_DEB,TAX_VAL_FIN,TAX_VAL_TAUX) "
			"	VALUES ('%s',",
					ofo_taux_get_mnemo( taux ));

	if( dbegin && g_utf8_strlen( dbegin, -1 )){
		g_string_append_printf( query, "'%s',", dbegin );
	} else {
		query = g_string_append( query, "0," );
	}

	if( dend && g_utf8_strlen( dend, -1 )){
		g_string_append_printf( query, "'%s',", dend );
	} else {
		query = g_string_append( query, "0," );
	}

	g_string_append_printf( query, "%s)", rate );

	ok = ofo_sgbd_query( sgbd, query->str );

	g_string_free( query, TRUE );
	g_free( dbegin );
	g_free( dend );
	g_free( rate );

	return( ok );
}

/**
 * ofo_taux_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_taux_update( ofoTaux *taux, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_taux_update";

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_debug( "%s: taux=%p, prev_mnemo=%s", thisfn, ( void * ) taux, prev_mnemo );

		if( taux_do_update(
					taux,
					prev_mnemo,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_UPDATE_DATASET( st_global, taux, prev_mnemo );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
taux_do_update( ofoTaux *taux, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user )
{
	return( taux_update_main( taux, prev_mnemo, sgbd, user ) &&
			taux_delete_validities( taux, sgbd ) &&
			taux_insert_validities( taux, sgbd ));
}

static gboolean
taux_update_main( ofoTaux *taux, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_taux_get_label( taux ));
	notes = my_utils_quote( ofo_taux_get_notes( taux ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_TAUX SET " );

	g_string_append_printf( query, "TAX_MNEMO='%s',", ofo_taux_get_mnemo( taux ));
	g_string_append_printf( query, "TAX_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "TAX_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "TAX_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	TAX_MAJ_USER='%s',TAX_MAJ_STAMP='%s'"
			"	WHERE TAX_MNEMO='%s'",
					user, stamp, prev_mnemo );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_taux_set_maj_user( taux, user );
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

/**
 * ofo_taux_delete:
 */
gboolean
ofo_taux_delete( ofoTaux *taux )
{
	static const gchar *thisfn = "ofo_taux_delete";

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_taux_is_deletable( taux ), FALSE );

	if( !OFO_BASE( taux )->prot->dispose_has_run ){

		g_debug( "%s: taux=%p", thisfn, ( void * ) taux );

		if( taux_do_delete(
					taux,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, taux );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
taux_do_delete( ofoTaux *taux, const ofoSgbd *sgbd )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TAUX WHERE TAX_MNEMO='%s'",
					ofo_taux_get_mnemo( taux ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TAUX_VAL WHERE TAX_MNEMO='%s'",
					ofo_taux_get_mnemo( taux ));

	ok &= ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
taux_cmp_by_mnemo( const ofoTaux *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_taux_get_mnemo( a ), mnemo ));
}

/*
 * sorting two period of validities
 * setting consistent to %FALSE if the two overlaps each other
 *
 * A period "a" is said lesser than a period "b" if "a" begins before "b".
 * If "a" and "b" begins on the same date, (this is an inconsistent
 * case), then "a" is said lesser than "b" if "a" ends before "b".
 * If "a" and "b" ends on the same date, then periods are said equal.
 *
 *                     +----------------------------------------------+
 *                     |                    "b"                       |
 *                     |         begin                  end           |
 *                     |    set      invalid      set      invalid    |
 * +-------------------+-----------+---------+-----------+------------+
 * | "a" begin set     |   bs-bs   |  bs-bi  |           |            |
 * | "a" begin invalid |   bi-bs   |  bi-bi  |           |            |
 * | "a" end set       |           |         |  es-es    |   es-ei    |
 * | "a" end invalid   |           |         |  ei-es    |   ei-ei    |
 * +-----+-------------+-----------+---------+-----------+------------+
 */

static gint
taux_cmp_by_vdata( sTauxVData *a, sTauxVData *b, gboolean *consistent )
{
	/* does 'a' start from the infinite ? */
	if( !g_date_valid( &a->begin )){
		/* 'a' starts from the infinite */
		if( !g_date_valid( &b->begin )){
			/* 'bi-bi' case
			 * the two dates start from the infinite: this is not
			 * consistent - compare the end dates */
			if( consistent ){
				*consistent = FALSE;
			}
			if( !g_date_valid( &a->end )){
				if( !g_date_valid( &b->end )){
					return( 0 );
				} else {
					return( -1 );
				}
			} else if( !g_date_valid( &b->end )){
				return( 1 );
			} else {
				return( g_date_compare( &a->end, &b->end ));
			}
		}
		/* 'bi-bs' case
		 *  'a' starts from the infinite while 'b-begin' is set
		 * for this be consistant, a must ends before b starts
		 * whatever be the case, 'a' is said lesser than 'b' */
		if( !g_date_valid( &a->end ) || g_date_compare( &a->end, &b->begin ) >= 0 ){
			if( consistent ){
				*consistent = FALSE;
			}
		}
		return( -1 );
	}

	/* a starts from a fixed date */
	if( !g_date_valid( &b->begin )){
		/* 'bs-bi' case
		 * 'b' is said lesser than 'a'
		 * for this be consistent, 'b' must ends before 'a' starts */
		if( !g_date_valid( &b->end ) || g_date_compare( &b->end, &a->begin ) >= 0 ){
			if( consistent ){
				*consistent = FALSE;
			}
		}
		return( 1 );
	}

	/* 'bs-bs' case
	 * 'a' and 'b' both starts from a set date: b must ends before 'a' starts */
	if( !g_date_valid( &b->end ) || g_date_compare( &b->end, &a->begin ) >= 0 ){
		if( consistent ){
			*consistent = FALSE;
		}
	}
	return( g_date_compare( &a->begin, &a->end ));
}

/**
 * ofo_taux_get_csv:
 */
GSList *
ofo_taux_get_csv( const ofoDossier *dossier )
{
	GList *set, *det;
	GSList *lines;
	gchar *str, *stamp;
	ofoTaux *taux;
	const gchar *muser;
	sTauxValid *sdet;
	gchar *sbegin, *send, *notes;

	OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

	lines = NULL;

	str = g_strdup_printf( "1;Mnemo;Label;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "2;Mnemo;Begin;End;Rate" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		taux = OFO_TAUX( set->data );

		notes = my_utils_export_multi_lines( ofo_taux_get_notes( taux ));
		muser = ofo_taux_get_maj_user( taux );
		stamp = my_utils_str_from_stamp( ofo_taux_get_maj_stamp( taux ));

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s",
				ofo_taux_get_mnemo( taux ),
				ofo_taux_get_label( taux ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( notes );
		g_free( stamp );

		lines = g_slist_prepend( lines, str );

		for( det=taux->private->valids ; det ; det=det->next ){
			sdet = ( sTauxValid * ) det->data;

			if( g_date_valid( &sdet->begin )){
				sbegin = my_utils_sql_from_date( &sdet->begin );
			} else {
				sbegin = g_strdup( "" );
			}

			if( g_date_valid( &sdet->end )){
				send = my_utils_sql_from_date( &sdet->end );
			} else {
				send = g_strdup( "" );
			}

			str = g_strdup_printf( "2;%s;%s;%s;%.2lf",
					ofo_taux_get_mnemo( taux ),
					sbegin,
					send,
					sdet->rate );

			g_free( sbegin );
			g_free( send );

			lines = g_slist_prepend( lines, str );
		}
	}

	return( g_slist_reverse( lines ));
}

/**
 * ofo_taux_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - taux mnemo
 * - label
 * - notes (opt)
 *
 * - 2:
 * - taux mnemo
 * - begin validity (opt)
 * - end validity (opt)
 * - rate
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'taux' records, then all 'validity' records...
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_taux_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_taux_import_csv";
	gint type;
	GSList *ili, *ico;
	ofoTaux *taux;
	sTauxValid *sdet;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;
	gchar *mnemo;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			ico=ili->data;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty line type", thisfn, count );
				errors += 1;
				continue;
			}
			type = atoi( str );
			switch( type ){
				case 1:
					taux = taux_import_csv_taux( ico, count, &errors );
					if( taux ){
						new_set = g_list_prepend( new_set, taux );
					}
					break;
				case 2:
					mnemo = NULL;
					sdet = taux_import_csv_valid( ico, count, &errors, &mnemo );
					if( sdet ){
						taux = taux_find_by_mnemo( new_set, mnemo );
						if( taux ){
							taux_add_val_detail( taux, sdet );
						}
						g_free( mnemo );
					}
					break;
				default:
					g_warning( "%s: (line %d) invalid line type: %d", thisfn, count, type );
					errors += 1;
					continue;
			}
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		taux_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			taux_do_insert(
					OFO_TAUX( ise->data ),
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_TAUX );

		g_list_free( new_set );

		st_global->send_signal_new = TRUE;
	}
}

static ofoTaux *
taux_import_csv_taux( GSList *fields, gint count, gint *errors )
{
	static const gchar *thisfn = "ofo_taux_import_csv_taux";
	ofoTaux *taux;
	const gchar *str;
	GSList *ico;
	gchar *splitted;

	taux = ofo_taux_new();

	/* taux mnemo */
	ico = fields->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty mnemo", thisfn, count );
		*errors += 1;
		g_object_unref( taux );
		return( NULL );
	}
	ofo_taux_set_mnemo( taux, str );

	/* taux label */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty label", thisfn, count );
		*errors += 1;
		g_object_unref( taux );
		return( NULL );
	}
	ofo_taux_set_label( taux, str );

	/* notes
	 * we are tolerant on the last field... */
	ico = ico->next;
	if( ico ){
		str = ( const gchar * ) ico->data;
		if( str && g_utf8_strlen( str, -1 )){
			splitted = my_utils_import_multi_lines( str );
			ofo_taux_set_notes( taux, splitted );
			g_free( splitted );
		}
	}

	return( taux );
}

static sTauxValid *
taux_import_csv_valid( GSList *fields, gint count, gint *errors, gchar **mnemo )
{
	static const gchar *thisfn = "ofo_taux_import_csv_valid";
	sTauxValid *detail;
	const gchar *str;
	GSList *ico;

	detail = g_new0( sTauxValid, 1 );

	/* taux mnemo */
	ico = fields->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty mnemo", thisfn, count );
		*errors += 1;
		g_free( detail );
		return( NULL );
	}
	*mnemo = g_strdup( str );

	/* taux begin validity */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	g_date_set_parse( &detail->begin, str );

	/* taux end validity */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	g_date_set_parse( &detail->end, str );

	/* taux rate */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	detail->rate = g_ascii_strtod( str, NULL );

	return( detail );
}

static gboolean
taux_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_TAUX" ) &&
			ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_TAUX_VAL" ));
}
