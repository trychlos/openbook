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

#include "ui/my-utils.h"
#include "ui/ofo-base.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-model.h"
#include "ui/ofo-sgbd.h"
#include "ui/ofo-taux.h"

/* priv instance data
 */
struct _ofoTauxPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	gint     id;
	gchar   *mnemo;
	gchar   *label;
	gchar   *notes;
	gchar   *maj_user;
	GTimeVal maj_stamp;
	GList   *valids;
};

typedef struct {
	GDate    begin;
	GDate    end;
	gdouble  rate;
	gchar   *maj_user;
	GTimeVal maj_stamp;
}
	sTauxValid;

typedef struct {
	const gchar *mnemo;
	const GDate *date;
}
	sCmpMneDat;

typedef struct {
	gint         id;
	const gchar *mnemo;
	const GDate *begin;
	const GDate *end;
}
	sCheckMneBegEnd;

G_DEFINE_TYPE( ofoTaux, ofo_taux, OFO_TYPE_BASE )

#define OFO_TAUX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_TAUX, ofoTauxPrivate))

OFO_BASE_DEFINE_GLOBAL( st_global, taux )

static GList         *taux_load_dataset( void );
static void           taux_set_begin( sTauxValid *tv, const GDate *date );
static void           taux_set_end( sTauxValid *tv, const GDate *date );
static void           taux_set_taux( sTauxValid *tv, gdouble rate );
static void           taux_set_maj_user( sTauxValid *tv, const gchar *user);
static void           taux_set_maj_stamp( sTauxValid *tv, const GTimeVal *stamp );
static ofoTaux       *taux_find_by_mnemo( GList *set, const gchar *mnemo, const GDate *date );
static gboolean       taux_do_insert( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_insert_main( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_get_back_id( ofoTaux *taux, ofoSgbd *sgbd );
static gboolean       taux_do_update( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user );
static gboolean       taux_do_delete( ofoTaux *taux, ofoSgbd *sgbd );
static gint           taux_cmp_by_mnemo( const ofoTaux *a, const sCmpMneDat *parms );
static gint           taux_cmp_by_ptr( const ofoTaux *a, const ofoTaux *b );
/*static gint           taux_check_by_period( const ofoTaux *ref, sCheckMneBegEnd *candidate );*/

static void
ofo_taux_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_taux_finalize";
	ofoTaux *self;

	self = OFO_TAUX( instance );

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			self->priv->mnemo, self->priv->label );

	g_free( self->priv->mnemo );
	g_free( self->priv->label );
	g_free( self->priv->notes );
	g_free( self->priv->maj_user );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_taux_parent_class )->finalize( instance );
}

static void
ofo_taux_dispose( GObject *instance )
{
	ofoTaux *self;

	self = OFO_TAUX( instance );

	if( !self->priv->dispose_has_run ){

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_taux_parent_class )->dispose( instance );
}

static void
ofo_taux_init( ofoTaux *self )
{
	static const gchar *thisfn = "ofo_taux_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_TAUX_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	self->priv->id = OFO_BASE_UNSET_ID;
}

static void
ofo_taux_class_init( ofoTauxClass *klass )
{
	static const gchar *thisfn = "ofo_taux_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoTauxPrivate ));

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
ofo_taux_get_dataset( ofoDossier *dossier )
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
	ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	ofoTaux *taux;
	GList *dataset, *it;
	sTauxValid *valid;
	gchar *query;

	dataset = NULL;
	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT TAX_ID,TAX_MNEMO,TAX_LABEL,TAX_NOTES,"
			"	TAX_MAJ_USER,TAX_MAJ_STAMP"
			"	FROM OFA_T_TAUX "
			"	ORDER BY TAX_MNEMO ASC" );

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		taux = ofo_taux_new();
		ofo_taux_set_id( taux, atoi(( gchar * ) icol->data ));
		icol = icol->next;
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
		taux->priv->valids = NULL;

		query = g_strdup_printf(
					"SELECT TAX_VAL_DEB,TAX_VAL_FIN,TAX_VAL_TAUX,"
					"	TAX_VAL_MAJ_USER,TAX_VAL_MAJ_STAMP "
					"	FROM OFA_T_TAUX_VAL "
					"	WHERE TAX_ID=%d",
					ofo_taux_get_id( taux ));

		result = ofo_sgbd_query_ex( sgbd, query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			valid = g_new0( sTauxValid, 1 );
			taux_set_begin( valid, my_utils_date_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			taux_set_end( valid, my_utils_date_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			if( icol->data ){
				taux_set_taux( valid, g_ascii_strtod(( gchar * ) icol->data, NULL ));
			}
			icol = icol->next;
			taux_set_maj_user( valid, ( gchar * ) icol->data );
			icol = icol->next;
			taux_set_maj_stamp( valid, my_utils_stamp_from_str(( gchar * ) icol->data ));

			taux->priv->valids = g_list_prepend( taux->priv->valids, valid );
		}

		ofo_sgbd_free_result( result );
		g_free( query );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_taux_get_by_mnemo:
 * @date: [allow-none]: the effect date at which the rate must be valid
 *
 * Returns: the searched taux, or %NULL.
 *
 * The returned object is owned by the #ofoTaux class, and should
 * not be unreffed by the caller.
 */
ofoTaux *
ofo_taux_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo, const GDate *date )
{
	static const gchar *thisfn = "ofo_taux_get_by_mnemo";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s, date=%p",
			thisfn, ( void * ) dossier, mnemo, ( void * ) date );

	OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

	return( taux_find_by_mnemo( st_global->dataset, mnemo, date ));
}

static ofoTaux *
taux_find_by_mnemo( GList *set, const gchar *mnemo, const GDate *date )
{
	GList *found;
	sCmpMneDat parms;

	parms.mnemo = mnemo;
	parms.date = date;

	found = g_list_find_custom(
				set, &parms, ( GCompareFunc ) taux_cmp_by_mnemo );
	if( found ){
		return( OFO_TAUX( found->data ));
	}

	return( NULL );
}

/**
 * ofo_taux_is_data_valid:
 * @mnemo: desired mnemo
 * @begin: desired beginning of validity.
 *  If not valid, then it is considered without limit.
 * @end: desired end of validity
 *  If not valid, then it is considered without limit.
 *
 * Checks if it is possible to define a new period of validity with the
 * specified arguments, regarding the other taux already defined. In
 * particular, the desired validity period must not overlap an already
 * existing one.
 *
 * Returns: NULL if the definition would be possible, or a pointer
 * to the object which prevents the definition.
 */
/*
ofoTaux *
ofo_taux_is_data_valid( ofoDossier *dossier,
		gint id, const gchar *mnemo, const GDate *begin, const GDate *end )
{
	static const gchar *thisfn = "ofo_taux_is_data_valid";
	gchar *sbegin, *send;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );
	g_return_val_if_fail( begin, NULL );
	g_return_val_if_fail( end, NULL );

	sbegin = my_utils_display_from_date( begin, MY_UTILS_DATE_DMMM );
	send = my_utils_display_from_date( end, MY_UTILS_DATE_DMMM );

	g_debug( "%s: dossier=%p, id=%d, mnemo=%s, begin=%s, end=%s",
			thisfn, ( void * ) dossier, id, mnemo, sbegin, send );

	g_free( send );
	g_free( sbegin );

	OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

	return( taux_is_data_valid( st_global->dataset, id, mnemo, begin, end ));
}

static ofoTaux *
taux_is_data_valid( GList *set,
		gint id, const gchar *mnemo, const GDate *begin, const GDate *end )
{
	ofoTaux *taux;
	GList *found;
	sCheckMneBegEnd parms;

	taux = NULL;

	parms.id = id;
	parms.mnemo = mnemo;
	parms.begin = begin;
	parms.end = end;

	found = g_list_find_custom(
			set, &parms, ( GCompareFunc ) taux_check_by_period );
	if( found ){
		taux = OFO_TAUX( found->data );
	}

	return( taux );
}*/

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
 * ofo_taux_get_id:
 */
gint
ofo_taux_get_id( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), OFO_BASE_UNSET_ID );

	if( !taux->priv->dispose_has_run ){

		return( taux->priv->id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_taux_get_mnemo:
 */
const gchar *
ofo_taux_get_mnemo( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !taux->priv->dispose_has_run ){

		return(( const gchar * ) taux->priv->mnemo );
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

	if( !taux->priv->dispose_has_run ){

		return(( const gchar * ) taux->priv->label );
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

	if( !taux->priv->dispose_has_run ){

		return(( const gchar * ) taux->priv->notes );
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

	if( !taux->priv->dispose_has_run ){

		return(( const gchar * ) taux->priv->maj_user );
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

	if( !taux->priv->dispose_has_run ){

		return(( const GTimeVal * ) &taux->priv->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_taux_get_val_count:
 */
gint
ofo_taux_get_val_count( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), 0 );

	if( !taux->priv->dispose_has_run ){

		return( g_list_length( taux->priv->valids ));
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

	if( !taux->priv->dispose_has_run ){

		nth = g_list_nth( taux->priv->valids, idx );
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

	if( !taux->priv->dispose_has_run ){

		nth = g_list_nth( taux->priv->valids, idx );
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

	if( !taux->priv->dispose_has_run ){

		nth = g_list_nth( taux->priv->valids, idx );
		if( nth ){
			rate = ( sTauxValid * ) nth->data;
			return( rate->rate );
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
	/* a rate whose internal identifier is not set is deletable,
	 * but this should never appear */
	g_return_val_if_fail( ofo_taux_get_id( taux ) > 0, TRUE );

	if( !taux->priv->dispose_has_run ){

		dossier = OFO_DOSSIER( st_global->dossier );

		return( !ofo_model_use_taux( dossier, ofo_taux_get_mnemo( taux )));
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_taux_set_id:
 */
void
ofo_taux_set_id( ofoTaux *taux, gint id )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->id = id;
	}
}

/**
 * ofo_taux_set_mnemo:
 */
void
ofo_taux_set_mnemo( ofoTaux *taux, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_taux_set_label:
 */
void
ofo_taux_set_label( ofoTaux *taux, const gchar *label )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->label = g_strdup( label );
	}
}

/**
 * ofo_taux_set_notes:
 */
void
ofo_taux_set_notes( ofoTaux *taux, const gchar *notes )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_taux_set_val_begin:
 */
/*
void
ofo_taux_set_val_begin( ofoTaux *taux, const GDate *date )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		memcpy( &taux->priv->val_begin, date, sizeof( GDate ));
	}
}*/

static void
taux_set_begin( sTauxValid *tv, const GDate *date )
{
	memcpy( &tv->begin, date, sizeof( GDate ));
}

/**
 * ofo_taux_set_val_end:
 */
/*
void
ofo_taux_set_val_end( ofoTaux *taux, const GDate *date )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		memcpy( &taux->priv->val_end, date, sizeof( GDate ));
	}
}*/

static void
taux_set_end( sTauxValid *tv, const GDate *date )
{
	memcpy( &tv->end, date, sizeof( GDate ));
}

/**
 * ofo_taux_set_taux:
 */
/*
void
ofo_taux_set_taux( ofoTaux *taux, gdouble value )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->taux = value;
	}
}*/

static void
taux_set_taux( sTauxValid *tv, gdouble value )
{
	tv->rate = value;
}

/**
 * ofo_taux_set_maj_user:
 */
void
ofo_taux_set_maj_user( ofoTaux *taux, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->maj_user = g_strdup( maj_user );
	}
}

static void
taux_set_maj_user( sTauxValid *tv, const gchar *maj_user )
{
	tv->maj_user = g_strdup( maj_user );
}

/**
 * ofo_taux_set_maj_stamp:
 */
void
ofo_taux_set_maj_stamp( ofoTaux *taux, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		memcpy( &taux->priv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

static void
taux_set_maj_stamp( sTauxValid *tv, const GTimeVal *maj_stamp )
{
	memcpy( &tv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
}

/**
 * ofo_taux_insert:
 *
 * First creation of a new rate.
 */
gboolean
ofo_taux_insert( ofoTaux *taux, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_taux_insert";

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !taux->priv->dispose_has_run ){

		g_debug( "%s: taux=%p, dossier=%p",
				thisfn, ( void * ) taux, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

		if( taux_do_insert(
					taux,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_ADD_TO_DATASET( st_global, taux );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
taux_do_insert( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
{
	return( taux_insert_main( taux, sgbd, user ) &&
			taux_get_back_id( taux, sgbd ));
}

static gboolean
taux_insert_main( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
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
taux_get_back_id( ofoTaux *taux, ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;

	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()" );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_taux_set_id( taux, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

#if 0
static gboolean
taux_do_insert( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gchar *dbegin, *dend;
	gchar rate[1+G_ASCII_DTOSTR_BUF_SIZE];
	gboolean ok;
	gchar *stamp;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_taux_get_label( taux ));
	notes = my_utils_quote( ofo_taux_get_notes( taux ));
	stamp = my_utils_timestamp();
	dbegin = my_utils_sql_from_date( ofo_taux_get_val_begin( taux ));
	dend = my_utils_sql_from_date( ofo_taux_get_val_end( taux ));

	query = g_string_new( "INSERT INTO OFA_T_TAUX" );

	g_string_append_printf( query,
			"	(TAX_MNEMO,TAX_LABEL,TAX_NOTES,"
			"	TAX_VAL_DEB, TAX_VAL_FIN,TAX_TAUX,"
			"	TAX_MAJ_USER, TAX_MAJ_STAMP) VALUES ('%s','%s',",
			ofo_taux_get_mnemo( taux ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( dbegin && g_utf8_strlen( dbegin, -1 )){
		g_string_append_printf( query, "'%s',", dbegin );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( dend && g_utf8_strlen( dend, -1 )){
		g_string_append_printf( query, "'%s',", dend );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
				"%s,'%s','%s')",
				g_ascii_dtostr( rate, G_ASCII_DTOSTR_BUF_SIZE, ofo_taux_get_taux( taux )),
				user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_taux_set_maj_user( taux, user );
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str( stamp ));

		g_string_printf( query,
				"SELECT TAX_ID FROM OFA_T_TAUX"
				"	WHERE TAX_MNEMO='%s'",
				ofo_taux_get_mnemo( taux ));

		result = ofo_sgbd_query_ex( sgbd, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_taux_set_id( taux, atoi(( gchar * ) icol->data ));
			ofo_sgbd_free_result( result );
			ok = TRUE;
		}
	}

	g_string_free( query, TRUE );
	g_free( dbegin );
	g_free( dend );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}
#endif

/**
 * ofo_taux_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_taux_update( ofoTaux *taux, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_taux_update";

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !taux->priv->dispose_has_run ){

		g_debug( "%s: taux=%p, dossier=%p",
				thisfn, ( void * ) taux, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

		if( taux_do_update(
					taux,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_UPDATE_DATASET( st_global, taux );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
taux_do_update( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
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
			"	WHERE TAX_ID=%d",
					user, stamp, ofo_taux_get_id( taux ));

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

#if 0
static gboolean
taux_do_update( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;
	gchar *dbegin, *dend;
	gchar rate[1+G_ASCII_DTOSTR_BUF_SIZE];

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_taux_get_label( taux ));
	notes = my_utils_quote( ofo_taux_get_notes( taux ));
	stamp = my_utils_timestamp();
	dbegin = my_utils_sql_from_date( ofo_taux_get_val_begin( taux ));
	dend = my_utils_sql_from_date( ofo_taux_get_val_end( taux ));

	query = g_string_new( "UPDATE OFA_T_TAUX SET " );

	g_string_append_printf( query, "TAX_MNEMO='%s',", ofo_taux_get_mnemo( taux ));
	g_string_append_printf( query, "TAX_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "TAX_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "TAX_NOTES=NULL," );
	}

	if( dbegin && g_utf8_strlen( dbegin, -1 )){
		g_string_append_printf( query, "TAX_VAL_DEB='%s',", dbegin );
	} else {
		query = g_string_append( query, "TAX_VAL_DEB=NULL," );
	}

	if( dend && g_utf8_strlen( dend, -1 )){
		g_string_append_printf( query, "TAX_VAL_FIN='%s',", dend );
	} else {
		query = g_string_append( query, "TAX_VAL_FIN=NULL," );
	}

	g_string_append_printf( query,
			"	TAX_TAUX=%s,TAX_MAJ_USER='%s',TAX_MAJ_STAMP='%s'"
			"	WHERE TAX_ID=%d",
			g_ascii_dtostr( rate, G_ASCII_DTOSTR_BUF_SIZE, ofo_taux_get_taux( taux )),
			user, stamp, ofo_taux_get_id( taux ));

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_taux_set_maj_user( taux, user );
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
	return( TRUE );
}
#endif

/**
 * ofo_taux_delete:
 */
gboolean
ofo_taux_delete( ofoTaux *taux, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_taux_delete";

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !taux->priv->dispose_has_run ){

		g_debug( "%s: taux=%p, dossier=%p",
				thisfn, ( void * ) taux, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, taux );

		if( taux_do_delete(
					taux,
					ofo_dossier_get_sgbd( dossier ))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, taux );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
taux_do_delete( ofoTaux *taux, ofoSgbd *sgbd )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TAUX WHERE TAX_ID=%d",
					ofo_taux_get_id( taux ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TAUX_VAL WHERE TAX_ID=%d",
					ofo_taux_get_id( taux ));

	ok &= ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
taux_cmp_by_mnemo( const ofoTaux *a, const sCmpMneDat *parms )
{
/*
	gint cmp_mnemo;
	gint cmp_date;
	const GDate *val_begin, *val_end;

	cmp_mnemo = g_utf8_collate( ofo_taux_get_mnemo( a ), parms->mnemo );

	if( cmp_mnemo == 0 && parms->date && g_date_valid( parms->date )){
		val_begin = ofo_taux_get_val_begin( a );
		val_end = ofo_taux_get_val_end( a );
		if( val_begin && g_date_valid( val_begin )){
			cmp_date = g_date_compare( val_begin, parms->date );
			if( cmp_date >= 0 ){
				return( cmp_date );
			}
		}
		if( val_end && g_date_valid( val_end )){
			cmp_date = g_date_compare( val_end, parms->date );
			if( cmp_date <= 0 ){
				return( cmp_date );
			}
		}
		return( 0 );
	}

	return( cmp_mnemo );
	*/
	return( 0 );
}

static gint
taux_cmp_by_ptr( const ofoTaux *a, const ofoTaux *b )
{
	/*
	gint im;
	im = g_utf8_collate( ofo_taux_get_mnemo( a ), ofo_taux_get_mnemo( b ));
	if( im ){
		return( im );
	}
	return( g_date_compare( ofo_taux_get_val_begin( a ), ofo_taux_get_val_begin( b )));*/
	return( 0 );
}

/*
 * we are searching here a taux which would prevent the definition of a
 * new record with the specifications given in @check - no comparaison
 * needed, just return zero if such a record is found
 */
#if 0
static gint
taux_check_by_period( const ofoTaux *ref, sCheckMneBegEnd *candidate )
{
	const GDate *ref_begin, *ref_end;
	gboolean begin_ok, end_ok; /* TRUE if periods are compatible */

	/* do not check against the same record */
	if( ofo_taux_get_id( ref ) == candidate->id ){
		return( 1 );
	}

	if( g_utf8_collate( ofo_taux_get_mnemo( ref ), candidate->mnemo )){
		return( 1 ); /* anything but zero */
	}

	/* found another taux with the same mnemo
	 * does its validity period overlap ours ?
	 */
	ref_begin = ofo_taux_get_val_begin( ref );
	ref_end = ofo_taux_get_val_begin( ref );

	if( !g_date_valid( candidate->begin )){
		/* candidate begin is invalid => validity since the very
		 * beginning of the world : the reference must have a valid begin
		 * date greater that the candidate end date */
		begin_ok = ( g_date_valid( ref_begin ) &&
						g_date_valid( candidate->end ) &&
						g_date_compare( ref_begin, candidate->end ) > 0 );
	} else {
		/* valid candidate beginning date
		 * => the reference is either before or after the candidate
		 *  so either the reference ends before the candidate begins
		 *   or the reference begins after the candidate has ended */
		begin_ok = ( g_date_valid( ref_end ) &&
						g_date_compare( ref_end, candidate->begin ) < 0 ) ||
					( g_date_valid( ref_begin ) &&
						g_date_valid( candidate->end ) &&
						g_date_compare( ref_begin, candidate->end ) > 0 );
	}

	if( !g_date_valid( candidate->end )){
		/* candidate ending date is invalid => infinite validity is
		 * required - this is possible if reference as an ending
		 * validity before the beginning of the candidate */
		end_ok = ( g_date_valid( ref_end ) &&
					g_date_valid( candidate->begin ) &&
					g_date_compare( ref_end, candidate->begin ) < 0 );
	} else {
		/* candidate ending date valid
		 * => the reference is either before or after the candidate
		 * so the reference ends before the candidate begins
		 *  or the reference begins afer the candidate has ended */
		end_ok = ( g_date_valid( ref_end ) &&
						g_date_valid( candidate->begin ) &&
						g_date_compare( ref_end, candidate->begin ) < 0 ) ||
					( g_date_valid( ref_begin ) &&
							g_date_compare( ref_begin, candidate->end ) > 0 );
	}
	if( !begin_ok || !end_ok ){
		return( 0 );
	}
	return( 1 );
	return( 0 );
}
#endif
