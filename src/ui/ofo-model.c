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
#include "ui/ofo-base-prot.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-model.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoModelPrivate {

	/* sgbd data
	 */
	gint       id;
	gchar     *mnemo;
	gchar     *label;
	gint       journal;
	gboolean   journal_locked;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;

	/* lignes de détail
	 */
	GList     *details;					/* an (un-ordered) list of detail lines */
};

typedef struct {
	gchar     *comment;
	gchar     *account;					/* account */
	gboolean   account_locked;			/* account is locked */
	gchar     *label;
	gboolean   label_locked;
	gchar     *debit;
	gboolean   debit_locked;
	gchar     *credit;
	gboolean   credit_locked;
}
	sModDetail;

G_DEFINE_TYPE( ofoModel, ofo_model, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, model )

static GList         *model_load_dataset( void );
static ofoModel      *model_find_by_mnemo( GList *set, const gchar *mnemo );
static gint           model_count_for_journal( const ofoSgbd *sgbd, gint jou_id );
static gint           model_count_for_taux( const ofoSgbd *sgbd, const gchar *mnemo );
static gboolean       model_do_insert( ofoModel *model, const ofoSgbd *sgbd, const gchar *user );
static gboolean       model_insert_main( ofoModel *model, const ofoSgbd *sgbd, const gchar *user );
static gboolean       model_get_back_id( ofoModel *model, const ofoSgbd *sgbd );
static gboolean       model_delete_details( ofoModel *model, const ofoSgbd *sgbd );
static gboolean       model_insert_details_ex( ofoModel *model, const ofoSgbd *sgbd );
static gboolean       model_insert_details( ofoModel *model, const ofoSgbd *sgbd, gint rang, sModDetail *detail );
static gboolean       model_do_update( ofoModel *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo );
static gboolean       model_update_main( ofoModel *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo );
static gboolean       model_do_delete( ofoModel *model, const ofoSgbd *sgbd );
static gint           model_cmp_by_mnemo( const ofoModel *a, const gchar *mnemo );
static gint           model_cmp_by_ptr( const ofoModel *a, const ofoModel *b );

static void
details_list_free_detail( sModDetail *detail )
{
	g_free( detail->comment );
	g_free( detail->account );
	g_free( detail->label );
	g_free( detail->debit );
	g_free( detail->credit );
	g_free( detail );
}

static void
details_list_free( ofoModel *model )
{
	ofoModelPrivate *priv = model->private;
	if( priv->details ){
		g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
		priv->details = NULL;
	}
}

static void
ofo_model_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_model_finalize";
	ofoModelPrivate *priv;

	priv = OFO_MODEL( instance )->private;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->maj_user );

	if( priv->details ){
		details_list_free( OFO_MODEL( instance ));
	}
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_model_parent_class )->finalize( instance );
}

static void
ofo_model_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_MODEL( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_model_parent_class )->dispose( instance );
}

static void
ofo_model_init( ofoModel *self )
{
	static const gchar *thisfn = "ofo_model_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoModelPrivate, 1 );

	self->private->id = OFO_BASE_UNSET_ID;
	self->private->journal = OFO_BASE_UNSET_ID;
}

static void
ofo_model_class_init( ofoModelClass *klass )
{
	static const gchar *thisfn = "ofo_model_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoModelPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_model_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_model_finalize;
}

/**
 * ofo_model_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoModel models, ordered by ascending
 * mnemo. The returned list is owned by the #ofoModel class, and
 * should not be freed by the caller.
 */
GList *
ofo_model_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_model_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	return( st_global->dataset );
}

static GList *
model_load_dataset( void )
{
	const ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	ofoModel *model;
	GList *dataset, *im;
	gchar *query;
	sModDetail *detail;
	GList *details;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT MOD_ID,MOD_MNEMO,MOD_LABEL,MOD_JOU_ID,MOD_JOU_VER,MOD_NOTES,"
			"	MOD_MAJ_USER,MOD_MAJ_STAMP "
			"	FROM OFA_T_MODELES "
			"	ORDER BY MOD_MNEMO ASC" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		model = ofo_model_new();
		ofo_model_set_id( model, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_model_set_mnemo( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_label( model, ( gchar * ) icol->data );
		icol = icol->next;
		if( icol->data ){
			ofo_model_set_journal( model, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_model_set_journal_locked( model, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_model_set_notes( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_maj_user( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_maj_stamp( model, my_utils_stamp_from_str(( gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, model );
	}

	ofo_sgbd_free_result( result );

	for( im=dataset ; im ; im=im->next ){

		model = OFO_MODEL( im->data );

		query = g_strdup_printf(
				"SELECT MOD_DET_COMMENT,"
				"	MOD_DET_ACCOUNT,MOD_DET_ACCOUNT_VER,"
				"	MOD_DET_LABEL,MOD_DET_LABEL_VER,"
				"	MOD_DET_DEBIT,MOD_DET_DEBIT_VER,"
				"	MOD_DET_CREDIT,MOD_DET_CREDIT_VER "
				"	FROM OFA_T_MODELES_DET "
				"	WHERE MOD_ID=%d ORDER BY MOD_DET_RANG ASC", ofo_model_get_id( model ));

		result = ofo_sgbd_query_ex( sgbd, query );
		details = NULL;

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			detail = g_new0( sModDetail, 1 );
			detail->comment = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			detail->account = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				detail->account_locked = atoi(( gchar * ) icol->data );
			}
			icol = icol->next;
			detail->label = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				detail->label_locked = atoi(( gchar * ) icol->data );
			}
			icol = icol->next;
			detail->debit = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				detail->debit_locked = atoi(( gchar * ) icol->data );
			}
			icol = icol->next;
			detail->credit = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				detail->credit_locked = atoi(( gchar * ) icol->data );
			}

			details = g_list_prepend( details, detail );
		}

		ofo_sgbd_free_result( result );
		model->private->details = g_list_reverse( details );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_model_get_by_mnemo:
 *
 * Returns: the searched model, or %NULL.
 *
 * The returned object is owned by the #ofoModel class, and should
 * not be unreffed by the caller.
 */
ofoModel *
ofo_model_get_by_mnemo( const ofoDossier *dossier, const gchar *mnemo )
{
	static const gchar *thisfn = "ofo_model_get_by_mnemo";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	return( model_find_by_mnemo( st_global->dataset, mnemo ));
}

static ofoModel *
model_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) model_cmp_by_mnemo );
	if( found ){
		return( OFO_MODEL( found->data ));
	}

	return( NULL );
}

/**
 * ofo_model_use_journal:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_model_use_journal( const ofoDossier *dossier, gint jou_id )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( model_count_for_journal( ofo_dossier_get_sgbd( dossier ), jou_id ) > 0 );
}

static gint
model_count_for_journal( const ofoSgbd *sgbd, gint jou_id )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_MODELES "
				"	WHERE MOD_JOU_ID=%d",
					jou_id );

	count = 0;
	result = ofo_sgbd_query_ex( sgbd, query );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		ofo_sgbd_free_result( result );
	}

	return( count );
}

/**
 * ofo_model_use_taux:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified rate.
 */
gboolean
ofo_model_use_taux( const ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( model_count_for_taux( ofo_dossier_get_sgbd( dossier ), mnemo ) > 0 );
}

static gint
model_count_for_taux( const ofoSgbd *sgbd, const gchar *mnemo )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_MODELES_DET "
				"	WHERE MOD_DET_DEBIT LIKE '%%%s%%' OR MOD_DET_CREDIT LIKE '%%%s%%'",
					mnemo, mnemo );

	count = 0;
	result = ofo_sgbd_query_ex( sgbd, query );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		ofo_sgbd_free_result( result );
	}

	return( count );
}

/**
 * ofo_model_new:
 */
ofoModel *
ofo_model_new( void )
{
	ofoModel *model;

	model = g_object_new( OFO_TYPE_MODEL, NULL );

	return( model );
}

/**
 * ofo_model_get_id:
 */
gint
ofo_model_get_id( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->private->id );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_model_get_mnemo:
 */
const gchar *
ofo_model_get_mnemo( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->private->mnemo );
	}

	return( NULL );
}

/**
 * ofo_model_get_label:
 */
const gchar *
ofo_model_get_label( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->private->label );
	}

	return( NULL );
}

/**
 * ofo_model_get_journal:
 */
gint
ofo_model_get_journal( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->private->journal );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_model_get_journal_locked:
 */
gboolean
ofo_model_get_journal_locked( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->private->journal_locked );
	}

	return( FALSE );
}

/**
 * ofo_model_get_notes:
 */
const gchar *
ofo_model_get_notes( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->private->notes );
	}

	return( NULL );
}

/**
 * ofo_model_get_maj_user:
 */
const gchar *
ofo_model_get_maj_user( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return(( const gchar * ) model->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_model_get_maj_stamp:
 */
const GTimeVal *
ofo_model_get_maj_stamp( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &model->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_model_is_deletable:
 */
gboolean
ofo_model_is_deletable( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( TRUE );
	}

	return( FALSE );
}

/**
 * ofo_model_is_valid:
 */
gboolean
ofo_model_is_valid( const gchar *mnemo, const gchar *label, gint journal_id )
{
	g_return_val_if_fail( st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	return( mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 ) &&
			journal_id > 0 &&
			ofo_journal_get_by_id( OFO_DOSSIER( st_global->dossier ), journal_id ) != NULL );
}

/**
 * ofo_model_set_id:
 */
void
ofo_model_set_id( ofoModel *model, gint id )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		model->private->id = id;
	}
}

/**
 * ofo_model_set_mnemo:
 */
void
ofo_model_set_mnemo( ofoModel *model, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->private->mnemo );
		model->private->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_model_set_label:
 */
void
ofo_model_set_label( ofoModel *model, const gchar *label )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->private->label );
		model->private->label = g_strdup( label );
	}
}

/**
 * ofo_model_set_journal:
 */
void
ofo_model_set_journal( ofoModel *model, gint journal )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		model->private->journal = journal;
	}
}

/**
 * ofo_model_set_journal:
 */
void
ofo_model_set_journal_locked( ofoModel *model, gboolean journal_locked )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		model->private->journal_locked = journal_locked;
	}
}

/**
 * ofo_model_set_notes:
 */
void
ofo_model_set_notes( ofoModel *model, const gchar *notes )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->private->notes );
		model->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_model_set_maj_user:
 */
void
ofo_model_set_maj_user( ofoModel *model, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->private->maj_user );
		model->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_model_set_maj_stamp:
 */
void
ofo_model_set_maj_stamp( ofoModel *model, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		memcpy( &model->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

void
ofo_model_add_detail( ofoModel *model, const gchar *comment,
							const gchar *account, gboolean account_locked,
							const gchar *label, gboolean label_locked,
							const gchar *debit, gboolean debit_locked,
							const gchar *credit, gboolean credit_locked )
{
	sModDetail *detail;

	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		detail = g_new0( sModDetail, 1 );
		model->private->details = g_list_append( model->private->details, detail );

		detail->comment = g_strdup( comment );
		detail->account = g_strdup( account );
		detail->account_locked = account_locked;
		detail->label = g_strdup( label );
		detail->label_locked = label_locked;
		detail->debit = g_strdup( debit );
		detail->debit_locked = debit_locked;
		detail->credit = g_strdup( credit );
		detail->credit_locked = credit_locked;
	}
}

/**
 * ofo_model_free_detail_all:
 */
void
ofo_model_free_detail_all( ofoModel *model )
{
	g_return_if_fail( model && OFO_IS_MODEL( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		details_list_free( model );
	}
}

/**
 * ofo_model_get_detail_count:
 */
gint
ofo_model_get_detail_count( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), -1 );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( g_list_length( model->private->details ));
	}

	return( -1 );
}

/**
 * ofo_model_get_detail_comment:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_model_get_detail_comment( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->comment );
	}

	return( NULL );
}

/**
 * ofo_model_get_detail_account:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_model_get_detail_account( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->account );
	}

	return( NULL );
}

/**
 * ofo_model_get_detail_account_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_model_get_detail_account_locked( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->account_locked );
	}

	return( FALSE );
}

/**
 * ofo_model_get_detail_label:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_model_get_detail_label( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->label );
	}

	return( NULL );
}

/**
 * ofo_model_get_detail_label_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_model_get_detail_label_locked( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->label_locked );
	}

	return( FALSE );
}

/**
 * ofo_model_get_detail_debit:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_model_get_detail_debit( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->debit );
	}

	return( NULL );
}

/**
 * ofo_model_get_detail_debit_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_model_get_detail_debit_locked( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->debit_locked );
	}

	return( FALSE );
}

/**
 * ofo_model_get_detail_credit:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_model_get_detail_credit( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->credit );
	}

	return( NULL );
}

/**
 * ofo_model_get_detail_credit_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_model_get_detail_credit_locked( const ofoModel *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_MODEL( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->private->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->credit_locked );
	}

	return( FALSE );
}

/**
 * ofo_model_detail_is_formula:
 */
gboolean
ofo_model_detail_is_formula( const gchar *str )
{
	return( str && str[0] == '=' );
}

/**
 * ofo_model_insert:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_model_insert( ofoModel *model, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_model_insert";

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_debug( "%s: model=%p, dossier=%p",
				thisfn, ( void * ) model, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, model );

		if( model_do_insert(
					model,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_ADD_TO_DATASET( st_global, model );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_insert( ofoModel *model, const ofoSgbd *sgbd, const gchar *user )
{
	return( model_insert_main( model, sgbd, user ) &&
			model_get_back_id( model, sgbd ) &&
			model_delete_details( model, sgbd ) &&
			model_insert_details_ex( model, sgbd ));
}

static gboolean
model_insert_main( ofoModel *model, const ofoSgbd *sgbd, const gchar *user )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes;
	gchar *stamp;

	label = my_utils_quote( ofo_model_get_label( model ));
	notes = my_utils_quote( ofo_model_get_notes( model ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_MODELES" );

	g_string_append_printf( query,
			"	(MOD_MNEMO,MOD_LABEL,MOD_JOU_ID,MOD_JOU_VER,MOD_NOTES,"
			"	MOD_MAJ_USER, MOD_MAJ_STAMP) VALUES ('%s','%s',%d,%d,",
			ofo_model_get_mnemo( model ),
			label,
			ofo_model_get_journal( model ),
			ofo_model_get_journal_locked( model ) ? 1:0 );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", user, stamp );

	ok = ofo_sgbd_query( sgbd, query->str );

	ofo_model_set_maj_user( model, user );
	ofo_model_set_maj_stamp( model, my_utils_stamp_from_str( stamp ));

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

static gboolean
model_get_back_id( ofoModel *model, const ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;
	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()" );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_model_set_id( model, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_delete_details( ofoModel *model, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_MODELES_DET WHERE MOD_ID=%d",
			ofo_model_get_id( model ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gboolean
model_insert_details_ex( ofoModel *model, const ofoSgbd *sgbd )
{
	gboolean ok;
	GList *idet;
	sModDetail *sdet;
	gint rang;

	ok = FALSE;

	if( model_delete_details( model, sgbd )){
		ok = TRUE;
		for( idet=model->private->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			sdet = ( sModDetail * ) idet->data;
			if( !model_insert_details( model, sgbd, rang, sdet )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

static gboolean
model_insert_details( ofoModel *model, const ofoSgbd *sgbd, gint rang, sModDetail *detail )
{
	GString *query;
	gboolean ok;

	query = g_string_new( "INSERT INTO OFA_T_MODELES_DET " );

	g_string_append_printf( query,
			"	(MOD_ID,MOD_DET_RANG,MOD_DET_COMMENT,"
			"	MOD_DET_ACCOUNT,MOD_DET_ACCOUNT_VER,"
			"	MOD_DET_LABEL,MOD_DET_LABEL_VER,"
			"	MOD_DET_DEBIT,MOD_DET_DEBIT_VER,"
			"	MOD_DET_CREDIT,MOD_DET_CREDIT_VER) "
			"	VALUES(%d,%d,",
			ofo_model_get_id( model ), rang );

	if( detail->comment && g_utf8_strlen( detail->comment, -1 )){
		g_string_append_printf( query, "'%s',", detail->comment );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( detail->account && g_utf8_strlen( detail->account, -1 )){
		g_string_append_printf( query, "'%s',", detail->account );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%d,", detail->account_locked ? 1:0 );

	if( detail->label && g_utf8_strlen( detail->label, -1 )){
		g_string_append_printf( query, "'%s',", detail->label );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%d,", detail->label_locked ? 1:0 );

	if( detail->debit && g_utf8_strlen( detail->debit, -1 )){
		g_string_append_printf( query, "'%s',", detail->debit );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%d,", detail->debit_locked ? 1:0 );

	if( detail->credit && g_utf8_strlen( detail->credit, -1 )){
		g_string_append_printf( query, "'%s',", detail->credit );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%d", detail->credit_locked ? 1:0 );

	query = g_string_append( query, ")" );

	ok = ofo_sgbd_query( sgbd, query->str );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_model_update:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_model_update( ofoModel *model, const ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_model_update";

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_debug( "%s: model=%p, dossier=%p, prev_mnemo=%s",
				thisfn, ( void * ) model, ( void * ) dossier, prev_mnemo );

		OFO_BASE_SET_GLOBAL( st_global, dossier, model );

		if( model_do_update(
					model,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ),
					prev_mnemo )){

			OFO_BASE_UPDATE_DATASET( st_global, model );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_update( ofoModel *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo )
{
	return( model_update_main( model, sgbd, user, prev_mnemo ) &&
			model_delete_details( model, sgbd ) &&
			model_insert_details_ex( model, sgbd ));
}

static gboolean
model_update_main( ofoModel *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes;
	const gchar *new_mnemo;
	gchar *stamp;

	label = my_utils_quote( ofo_model_get_label( model ));
	notes = my_utils_quote( ofo_model_get_notes( model ));
	new_mnemo = ofo_model_get_mnemo( model );
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_MODELES SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "MOD_MNEMO='%s',", new_mnemo );
	}

	g_string_append_printf( query, "MOD_LABEL='%s',", label );
	g_string_append_printf( query, "MOD_JOU_ID=%d,", ofo_model_get_journal( model ));
	g_string_append_printf( query, "MOD_JOU_VER=%d,", ofo_model_get_journal_locked( model ) ? 1:0 );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "MOD_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "MOD_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	MOD_MAJ_USER='%s',MOD_MAJ_STAMP='%s'"
			"	WHERE MOD_ID=%d",
					user,
					stamp,
					ofo_model_get_id( model ));

	ok = ofo_sgbd_query( sgbd, query->str );

	ofo_model_set_maj_user( model, user );
	ofo_model_set_maj_stamp( model, my_utils_stamp_from_str( stamp ));

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

/**
 * ofo_model_delete:
 */
gboolean
ofo_model_delete( ofoModel *model, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_model_delete";

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_model_is_deletable( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_debug( "%s: model=%p, dossier=%p",
				thisfn, ( void * ) model, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, model );

		if( model_do_delete(
					model,
					ofo_dossier_get_sgbd( dossier ))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, model );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_delete( ofoModel *model, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_MODELES"
			"	WHERE MOD_ID=%d",
					ofo_model_get_id( model ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	ok &= model_delete_details( model, sgbd );

	return( ok );
}

static gint
model_cmp_by_mnemo( const ofoModel *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_model_get_mnemo( a ), mnemo ));
}

static gint
model_cmp_by_ptr( const ofoModel *a, const ofoModel *b )
{
	return( g_utf8_collate( ofo_model_get_mnemo( a ), ofo_model_get_mnemo( b )));
}

/**
 * ofo_model_get_csv:
 */
GSList *
ofo_model_get_csv( const ofoDossier *dossier )
{
	GList *set, *det;
	GSList *lines;
	gchar *str, *stamp;
	ofoModel *model;
	ofoJournal *journal;
	const gchar *notes, *muser;
	sModDetail *sdet;

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	lines = NULL;

	str = g_strdup_printf( "1;Mnemo;Label;Journal;JournalLocked;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "2;Mnemo;Comment;Account;AccountLocked;Label;LabelLocked;Debit;DebitLocked;Credit;CreditLocked" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		model = OFO_MODEL( set->data );

		journal = ofo_journal_get_by_id( dossier, ofo_model_get_journal( model ));
		notes = ofo_model_get_notes( model );
		muser = ofo_model_get_maj_user( model );
		stamp = my_utils_str_from_stamp( ofo_model_get_maj_stamp( model ));

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s;%s;%s",
				ofo_model_get_mnemo( model ),
				ofo_model_get_label( model ),
				journal ? ofo_journal_get_mnemo( journal ) : "",
				ofo_model_get_journal_locked( model ) ? "True" : "False",
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( stamp );

		lines = g_slist_prepend( lines, str );

		for( det=model->private->details ; det ; det=det->next ){
			sdet = ( sModDetail * ) det->data;

			str = g_strdup_printf( "2;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
					ofo_model_get_mnemo( model ),
					sdet->comment ? sdet->comment : "",
					sdet->account ? sdet->account : "",
					sdet->account_locked ? "True" : "False",
					sdet->label ? sdet->label : "",
					sdet->label_locked ? "True" : "False",
					sdet->debit ? sdet->debit : "",
					sdet->debit_locked ? "True" : "False",
					sdet->credit ? sdet->credit : "",
					sdet->credit_locked ? "True" : "False" );

			lines = g_slist_prepend( lines, str );
		}
	}

	return( g_slist_reverse( lines ));
}
