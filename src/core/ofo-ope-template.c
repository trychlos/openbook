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

#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"
#include "api/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoOpeTemplatePrivate {

	/* sgbd data
	 */
	gchar     *mnemo;
	gchar     *label;
	gchar     *ledger;
	gboolean   ledger_locked;
	gchar     *notes;
	gchar     *upd_user;
	GTimeVal   upd_stamp;

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

/* mnemonic max length */
#define MNEMO_LENGTH                    6

G_DEFINE_TYPE( ofoOpeTemplate, ofo_ope_template, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, model )

static void              on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, void *user_data );
static gboolean          do_update_ledger_mnemo( const ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id );
static gboolean          do_update_rate_mnemo( const ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id );
static GList            *model_load_dataset( void );
static ofoOpeTemplate *model_find_by_mnemo( GList *set, const gchar *mnemo );
static gint              model_count_for_ledger( const ofoSgbd *sgbd, const gchar *ledger );
static gint              model_count_for_rate( const ofoSgbd *sgbd, const gchar *mnemo );
static void              ope_template_set_upd_user( ofoOpeTemplate *model, const gchar *upd_user );
static void              ope_template_set_upd_stamp( ofoOpeTemplate *model, const GTimeVal *upd_stamp );
static gboolean          model_do_insert( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user );
static gboolean          model_insert_main( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user );
static gboolean          model_delete_details( ofoOpeTemplate *model, const ofoSgbd *sgbd );
static gboolean          model_insert_details_ex( ofoOpeTemplate *model, const ofoSgbd *sgbd );
static gboolean          model_insert_details( ofoOpeTemplate *model, const ofoSgbd *sgbd, gint rang, sModDetail *detail );
static gboolean          model_do_update( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo );
static gboolean          model_update_main( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo );
static gboolean          model_do_delete( ofoOpeTemplate *model, const ofoSgbd *sgbd );
static gint              model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo );
static ofoOpeTemplate *model_import_csv_model( GSList *fields, gint count, gint *errors );
static sModDetail       *model_import_csv_detail( GSList *fields, gint count, gint *errors, gchar **mnemo );
static gboolean          model_do_drop_content( const ofoSgbd *sgbd );

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
details_list_free( ofoOpeTemplate *model )
{
	ofoOpeTemplatePrivate *priv = model->priv;
	if( priv->details ){
		g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
		priv->details = NULL;
	}
}

static void
ope_template_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_ope_template_finalize";
	ofoOpeTemplatePrivate *priv;

	priv = OFO_OPE_TEMPLATE( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	g_return_if_fail( instance && OFO_IS_OPE_TEMPLATE( instance ));

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->ledger );
	g_free( priv->notes );
	g_free( priv->upd_user );

	if( priv->details ){
		details_list_free( OFO_OPE_TEMPLATE( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ope_template_parent_class )->finalize( instance );
}

static void
ope_template_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_OPE_TEMPLATE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_ope_template_parent_class )->dispose( instance );
}

static void
ofo_ope_template_init( ofoOpeTemplate *self )
{
	static const gchar *thisfn = "ofo_ope_template_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplatePrivate );
}

static void
ofo_ope_template_class_init( ofoOpeTemplateClass *klass )
{
	static const gchar *thisfn = "ofo_ope_template_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_finalize;

	g_type_class_add_private( klass, sizeof( ofoOpeTemplatePrivate ));
}

/**
 * ofo_ope_template_connect_handlers:
 *
 * As the signal connection is protected by a static variable, there is
 * no need here to handle signal disconnection
 */
void
ofo_ope_template_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ope_template_connect_handlers";

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_signal_connect( G_OBJECT( dossier ),
				OFA_SIGNAL_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );
}

static void
on_updated_object( const ofoDossier *dossier, ofoBase *object, const gchar *prev_id, void *user_data )
{
	static const gchar *thisfn = "ofo_ope_template_on_updated_object";
	const gchar *mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_LEDGER( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				do_update_ledger_mnemo( dossier, mnemo, prev_id );
			}
		}

	} else if( OFO_IS_RATE( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			mnemo = ofo_rate_get_mnemo( OFO_RATE( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				do_update_rate_mnemo( dossier, mnemo, prev_id );
			}
		}
	}
}

static gboolean
do_update_ledger_mnemo( const ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_do_update_ledger_mnemo";
	gchar *query;
	gboolean ok;

	g_debug( "%s: dossier=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) dossier, mnemo, prev_id );

	query = g_strdup_printf(
					"UPDATE OFA_T_OPE_TEMPLATES "
					"	SET OTE_LED_MNEMO='%s' WHERE OTE_LED_MNEMO='%s'",
								mnemo, prev_id );

	ok = ofo_sgbd_query( ofo_dossier_get_sgbd( dossier ), query, TRUE );

	g_free( query );

	g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
	st_global->dataset = NULL;
	g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_OPE_TEMPLATE );

	return( ok );
}

static gboolean
do_update_rate_mnemo( const ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_ope_template_do_update_rate_mnemo";
	gchar *query;
	const ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_debit, *det_credit;
	gint det_row;

	g_debug( "%s: dossier=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) dossier, mnemo, prev_id );

	sgbd = ofo_dossier_get_sgbd( dossier );

	query = g_strdup_printf(
					"SELECT OTE_MNEMO,OTE_DET_ROW,OTE_DET_DEBIT,OTE_DET_CREDIT "
					"	FROM OFA_T_OPE_TEMPLATES_DET "
					"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
							prev_id, prev_id );

	result = ofo_sgbd_query_ex( sgbd, query, TRUE );

	g_free( query );

	if( result ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			etp_mnemo = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			det_row = atoi(( gchar * ) icol->data );
			icol = icol->next;
			det_debit = my_utils_str_replace(( gchar * ) icol->data, prev_id, mnemo );
			icol = icol->next;
			det_credit = my_utils_str_replace(( gchar * ) icol->data, prev_id, mnemo );

			query = g_strdup_printf(
							"UPDATE OFA_T_OPE_TEMPLATES_DET "
							"	SET OTE_DET_DEBIT='%s',OTE_DET_CREDIT='%s' "
							"	WHERE OTE_MNEMO='%s' AND OTE_DET_ROW=%d",
									det_debit, det_credit,
									etp_mnemo, det_row );

			ofo_sgbd_query( sgbd, query, TRUE );

			g_free( query );
			g_free( det_credit );
			g_free( det_debit );
			g_free( etp_mnemo );
		}
	}

	g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
	st_global->dataset = NULL;
	g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_OPE_TEMPLATE );

	return( TRUE );
}

/**
 * ofo_ope_template_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoOpeTemplate models, ordered by ascending
 * mnemo. The returned list is owned by the #ofoOpeTemplate class, and
 * should not be freed by the caller.
 */
GList *
ofo_ope_template_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_ope_template_get_dataset";

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
	ofoOpeTemplate *model;
	GList *dataset, *im;
	gchar *query;
	sModDetail *detail;
	GList *details;
	GTimeVal timeval;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT OTE_MNEMO,OTE_LABEL,OTE_LED_MNEMO,OTE_LED_LOCKED,OTE_NOTES,"
			"	OTE_UPD_USER,OTE_UPD_STAMP "
			"	FROM OFA_T_OPE_TEMPLATES", TRUE );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		model = ofo_ope_template_new();
		ofo_ope_template_set_mnemo( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_ope_template_set_label( model, ( gchar * ) icol->data );
		icol = icol->next;
		if( icol->data ){
			ofo_ope_template_set_ledger( model, ( gchar * ) icol->data );
		}
		icol = icol->next;
		if( icol->data ){
			ofo_ope_template_set_ledger_locked( model, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_ope_template_set_notes( model, ( gchar * ) icol->data );
		icol = icol->next;
		ope_template_set_upd_user( model, ( gchar * ) icol->data );
		icol = icol->next;
		ope_template_set_upd_stamp( model,
				my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, model );
	}

	ofo_sgbd_free_result( result );

	for( im=dataset ; im ; im=im->next ){

		model = OFO_OPE_TEMPLATE( im->data );

		query = g_strdup_printf(
				"SELECT OTE_DET_COMMENT,"
				"	OTE_DET_ACCOUNT,OTE_DET_ACCOUNT_LOCKED,"
				"	OTE_DET_LABEL,OTE_DET_LABEL_LOCKED,"
				"	OTE_DET_DEBIT,OTE_DET_DEBIT_LOCKED,"
				"	OTE_DET_CREDIT,OTE_DET_CREDIT_LOCKED "
				"	FROM OFA_T_OPE_TEMPLATES_DET "
				"	WHERE OTE_MNEMO='%s' ORDER BY OTE_DET_ROW ASC",
						ofo_ope_template_get_mnemo( model ));

		result = ofo_sgbd_query_ex( sgbd, query, TRUE );
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
		model->priv->details = g_list_reverse( details );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_ope_template_get_by_mnemo:
 *
 * Returns: the searched model, or %NULL.
 *
 * The returned object is owned by the #ofoOpeTemplate class, and should
 * not be unreffed by the caller.
 */
ofoOpeTemplate *
ofo_ope_template_get_by_mnemo( const ofoDossier *dossier, const gchar *mnemo )
{
	/*static const gchar *thisfn = "ofo_ope_template_get_by_mnemo";*/

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	return( model_find_by_mnemo( st_global->dataset, mnemo ));
}

static ofoOpeTemplate *
model_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) model_cmp_by_mnemo );
	if( found ){
		return( OFO_OPE_TEMPLATE( found->data ));
	}

	return( NULL );
}

/**
 * ofo_ope_template_use_ledger:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_ope_template_use_ledger( const ofoDossier *dossier, const gchar *ledger )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ledger && g_utf8_strlen( ledger, -1 ), FALSE );

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	return( model_count_for_ledger( ofo_dossier_get_sgbd( dossier ), ledger ) > 0 );
}

static gint
model_count_for_ledger( const ofoSgbd *sgbd, const gchar *ledger )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES "
				"	WHERE OTE_LED_MNEMO='%s'",
					ledger );

	count = 0;
	result = ofo_sgbd_query_ex( sgbd, query, TRUE );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		ofo_sgbd_free_result( result );
	}

	return( count );
}

/**
 * ofo_ope_template_use_rate:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified rate.
 */
gboolean
ofo_ope_template_use_rate( const ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	return( model_count_for_rate( ofo_dossier_get_sgbd( dossier ), mnemo ) > 0 );
}

static gint
model_count_for_rate( const ofoSgbd *sgbd, const gchar *mnemo )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_OPE_TEMPLATES_DET "
				"	WHERE OTE_DET_DEBIT LIKE '%%%s%%' OR OTE_DET_CREDIT LIKE '%%%s%%'",
					mnemo, mnemo );

	count = 0;
	result = ofo_sgbd_query_ex( sgbd, query, TRUE );
	g_free( query );

	if( result ){
		icol = ( GSList * ) result->data;
		count = atoi(( gchar * ) icol->data );
		ofo_sgbd_free_result( result );
	}

	return( count );
}

/**
 * ofo_ope_template_new:
 */
ofoOpeTemplate *
ofo_ope_template_new( void )
{
	ofoOpeTemplate *model;

	model = g_object_new( OFO_TYPE_OPE_TEMPLATE, NULL );

	return( model );
}

/**
 * ofo_ope_template_new_from_template:
 * @model: the source #ofoOpeTemplate to be copied from.
 *
 * Allocates a new #ofoOpeTemplates object, initializing it with data
 * copied from @model #ofoOpeTemplate source.
 */
ofoOpeTemplate *
ofo_ope_template_new_from_template( const ofoOpeTemplate *model )
{
	ofoOpeTemplate *dest;
	gint count, i;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );

	dest = NULL;

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		dest = ofo_ope_template_new();

		ofo_ope_template_set_mnemo( dest, ofo_ope_template_get_mnemo( model ));
		ofo_ope_template_set_label( dest, ofo_ope_template_get_label( model ));
		ofo_ope_template_set_ledger( dest, ofo_ope_template_get_ledger( model ));
		ofo_ope_template_set_ledger_locked( dest, ofo_ope_template_get_ledger_locked( model ));
		ofo_ope_template_set_notes( dest, ofo_ope_template_get_notes( model ));

		count = ofo_ope_template_get_detail_count( model );
		for( i=0 ; i<count ; ++i ){
			ofo_ope_template_add_detail( dest,
					ofo_ope_template_get_detail_comment( model, i ),
					ofo_ope_template_get_detail_account( model, i ),
					ofo_ope_template_get_detail_account_locked( model, i ),
					ofo_ope_template_get_detail_label( model, i ),
					ofo_ope_template_get_detail_label_locked( model, i ),
					ofo_ope_template_get_detail_debit( model, i ),
					ofo_ope_template_get_detail_debit_locked( model, i ),
					ofo_ope_template_get_detail_credit( model, i ),
					ofo_ope_template_get_detail_credit_locked( model, i ));
		}
	}

	return( dest );
}

/**
 * ofo_ope_template_get_mnemo:
 */
const gchar *
ofo_ope_template_get_mnemo( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->priv->mnemo );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_mnemo_new_from:
 *
 * Returns a new mnemo derived from the given one, as a newly allocated
 * string that the caller should g_free().
 */
gchar *
ofo_ope_template_get_mnemo_new_from( const ofoOpeTemplate *model )
{
	const gchar *mnemo;
	gint len_mnemo;
	gchar *str;
	gint i, maxlen;

	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), NULL );

	str = NULL;

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		mnemo = ofo_ope_template_get_mnemo( model );
		len_mnemo = g_utf8_strlen( mnemo, -1 );
		for( i=2 ; ; ++i ){
			/* if we are greater than 9999, there is a problem... */
			maxlen = ( i < 10 ? MNEMO_LENGTH-1 :
						( i < 100 ? MNEMO_LENGTH-2 :
						( i < 1000 ? MNEMO_LENGTH-3 : MNEMO_LENGTH-4 )));
			if( maxlen < len_mnemo ){
				str = g_strdup_printf( "%*.*s%d", maxlen, maxlen, mnemo, i );
			} else {
				str = g_strdup_printf( "%s%d", mnemo, i );
			}
			if( !ofo_ope_template_get_by_mnemo( OFO_DOSSIER( st_global->dossier ), str )){
				break;
			}
			g_free( str );
		}
	}

	return( str );
}

/**
 * ofo_ope_template_get_label:
 */
const gchar *
ofo_ope_template_get_label( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( model && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->priv->label );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_ledger:
 */
const gchar *
ofo_ope_template_get_ledger( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return(( const gchar * ) model->priv->ledger );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_ledger_locked:
 */
gboolean
ofo_ope_template_get_ledger_locked( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->priv->ledger_locked );
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_notes:
 */
const gchar *
ofo_ope_template_get_notes( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( model->priv->notes );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_upd_user:
 */
const gchar *
ofo_ope_template_get_upd_user( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return(( const gchar * ) model->priv->upd_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ope_template_get_upd_stamp:
 */
const GTimeVal *
ofo_ope_template_get_upd_stamp( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &model->priv->upd_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_ope_template_is_deletable:
 */
gboolean
ofo_ope_template_is_deletable( const ofoOpeTemplate *model )
{
	ofoDossier *dossier;

	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		dossier = OFO_DOSSIER( st_global->dossier );

		return( !ofo_entry_use_ope_template( dossier, ofo_ope_template_get_mnemo( model )));
	}

	return( FALSE );
}

/**
 * ofo_ope_template_is_valid:
 */
gboolean
ofo_ope_template_is_valid( const gchar *mnemo, const gchar *label, const gchar *ledger )
{
	g_return_val_if_fail( st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	return( mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 ) &&
			ledger && g_utf8_strlen( ledger, -1 ) &&
			ofo_ledger_get_by_mnemo( OFO_DOSSIER( st_global->dossier ), ledger ));
}

/**
 * ofo_ope_template_set_mnemo:
 */
void
ofo_ope_template_set_mnemo( ofoOpeTemplate *model, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->priv->mnemo );
		model->priv->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_ope_template_set_label:
 */
void
ofo_ope_template_set_label( ofoOpeTemplate *model, const gchar *label )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->priv->label );
		model->priv->label = g_strdup( label );
	}
}

/**
 * ofo_ope_template_set_ledger:
 */
void
ofo_ope_template_set_ledger( ofoOpeTemplate *model, const gchar *ledger )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->priv->ledger );
		model->priv->ledger = g_strdup( ledger );
	}
}

/**
 * ofo_ope_template_set_ledger:
 */
void
ofo_ope_template_set_ledger_locked( ofoOpeTemplate *model, gboolean ledger_locked )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		model->priv->ledger_locked = ledger_locked;
	}
}

/**
 * ofo_ope_template_set_notes:
 */
void
ofo_ope_template_set_notes( ofoOpeTemplate *model, const gchar *notes )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->priv->notes );
		model->priv->notes = g_strdup( notes );
	}
}

/*
 * ofo_ope_template_set_upd_user:
 */
static void
ope_template_set_upd_user( ofoOpeTemplate *model, const gchar *upd_user )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_free( model->priv->upd_user );
		model->priv->upd_user = g_strdup( upd_user );
	}
}

/*
 * ofo_ope_template_set_upd_stamp:
 */
static void
ope_template_set_upd_stamp( ofoOpeTemplate *model, const GTimeVal *upd_stamp )
{
	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &model->priv->upd_stamp, upd_stamp );
	}
}

void
ofo_ope_template_add_detail( ofoOpeTemplate *model, const gchar *comment,
							const gchar *account, gboolean account_locked,
							const gchar *label, gboolean label_locked,
							const gchar *debit, gboolean debit_locked,
							const gchar *credit, gboolean credit_locked )
{
	sModDetail *detail;

	g_return_if_fail( OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		detail = g_new0( sModDetail, 1 );
		model->priv->details = g_list_append( model->priv->details, detail );

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
 * ofo_ope_template_free_detail_all:
 */
void
ofo_ope_template_free_detail_all( ofoOpeTemplate *model )
{
	g_return_if_fail( model && OFO_IS_OPE_TEMPLATE( model ));

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		details_list_free( model );
	}
}

/**
 * ofo_ope_template_get_detail_count:
 */
gint
ofo_ope_template_get_detail_count( const ofoOpeTemplate *model )
{
	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), -1 );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		return( g_list_length( model->priv->details ));
	}

	return( -1 );
}

/**
 * ofo_ope_template_get_detail_comment:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_comment( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->comment );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_account:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_account( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->account );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_account_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_account_locked( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->account_locked );
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_detail_label:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_label( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->label );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_label_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_label_locked( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->label_locked );
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_detail_debit:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_debit( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->debit );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_debit_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_debit_locked( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->debit_locked );
	}

	return( FALSE );
}

/**
 * ofo_ope_template_get_detail_credit:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_ope_template_get_detail_credit( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), NULL );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return(( const gchar * ) sdet->credit );
	}

	return( NULL );
}

/**
 * ofo_ope_template_get_detail_credit_locked:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_ope_template_get_detail_credit_locked( const ofoOpeTemplate *model, gint idx )
{
	GList *idet;
	sModDetail *sdet;

	g_return_val_if_fail( idx >= 0 && OFO_IS_OPE_TEMPLATE( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->credit_locked );
	}

	return( FALSE );
}

/**
 * ofo_ope_template_detail_is_formula:
 */
gboolean
ofo_ope_template_detail_is_formula( const gchar *str )
{
	return( str && str[0] == '=' );
}

/**
 * ofo_ope_template_insert:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_ope_template_insert( ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofo_ope_template_insert";

	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_debug( "%s: model=%p", thisfn, ( void * ) model );

		if( model_do_insert(
					model,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, model );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_insert( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user )
{
	return( model_insert_main( model, sgbd, user ) &&
			model_insert_details_ex( model, sgbd ));
}

static gboolean
model_insert_main( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes;
	gchar *stamp_str;
	GTimeVal stamp;

	label = my_utils_quote( ofo_ope_template_get_label( model ));
	notes = my_utils_quote( ofo_ope_template_get_notes( model ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_OPE_TEMPLATES" );

	g_string_append_printf( query,
			"	(OTE_MNEMO,OTE_LABEL,OTE_LED_MNEMO,OTE_LED_LOCKED,OTE_NOTES,"
			"	OTE_UPD_USER, OTE_UPD_STAMP) VALUES ('%s','%s','%s',%d,",
			ofo_ope_template_get_mnemo( model ),
			label,
			ofo_ope_template_get_ledger( model ),
			ofo_ope_template_get_ledger_locked( model ) ? 1:0 );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", user, stamp_str );

	ok = ofo_sgbd_query( sgbd, query->str, TRUE );

	ope_template_set_upd_user( model, user );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
model_delete_details( ofoOpeTemplate *model, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES_DET WHERE OTE_MNEMO='%s'",
			ofo_ope_template_get_mnemo( model ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
model_insert_details_ex( ofoOpeTemplate *model, const ofoSgbd *sgbd )
{
	gboolean ok;
	GList *idet;
	sModDetail *sdet;
	gint rang;

	ok = FALSE;

	if( model_delete_details( model, sgbd )){
		ok = TRUE;
		for( idet=model->priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
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
model_insert_details( ofoOpeTemplate *model, const ofoSgbd *sgbd, gint rang, sModDetail *detail )
{
	GString *query;
	gboolean ok;

	query = g_string_new( "INSERT INTO OFA_T_OPE_TEMPLATES_DET " );

	g_string_append_printf( query,
			"	(OTE_MNEMO,OTE_DET_ROW,OTE_DET_COMMENT,"
			"	OTE_DET_ACCOUNT,OTE_DET_ACCOUNT_LOCKED,"
			"	OTE_DET_LABEL,OTE_DET_LABEL_LOCKED,"
			"	OTE_DET_DEBIT,OTE_DET_DEBIT_LOCKED,"
			"	OTE_DET_CREDIT,OTE_DET_CREDIT_LOCKED) "
			"	VALUES('%s',%d,",
			ofo_ope_template_get_mnemo( model ), rang );

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

	ok = ofo_sgbd_query( sgbd, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_ope_template_update:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_ope_template_update( ofoOpeTemplate *model, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_ope_template_update";

	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_debug( "%s: model=%p, prev_mnemo=%s", thisfn, ( void * ) model, prev_mnemo );

		if( model_do_update(
					model,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )),
					prev_mnemo )){

			OFO_BASE_UPDATE_DATASET( st_global, model, prev_mnemo );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_update( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo )
{
	return( model_update_main( model, sgbd, user, prev_mnemo ) &&
			model_insert_details_ex( model, sgbd ));
}

static gboolean
model_update_main( ofoOpeTemplate *model, const ofoSgbd *sgbd, const gchar *user, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes;
	const gchar *new_mnemo;
	gchar *stamp_str;
	GTimeVal stamp;

	label = my_utils_quote( ofo_ope_template_get_label( model ));
	notes = my_utils_quote( ofo_ope_template_get_notes( model ));
	new_mnemo = ofo_ope_template_get_mnemo( model );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_OPE_TEMPLATES SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "OTE_MNEMO='%s',", new_mnemo );
	}

	g_string_append_printf( query, "OTE_LABEL='%s',", label );
	g_string_append_printf( query, "OTE_LED_MNEMO='%s',", ofo_ope_template_get_ledger( model ));
	g_string_append_printf( query, "OTE_LED_LOCKED=%d,", ofo_ope_template_get_ledger_locked( model ) ? 1:0 );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "OTE_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "OTE_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	OTE_UPD_USER='%s',OTE_UPD_STAMP='%s'"
			"	WHERE OTE_MNEMO='%s'",
					user,
					stamp_str,
					prev_mnemo );

	ok = ofo_sgbd_query( sgbd, query->str, TRUE );

	ope_template_set_upd_user( model, user );
	ope_template_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );

	return( ok );
}

/**
 * ofo_ope_template_delete:
 */
gboolean
ofo_ope_template_delete( ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofo_ope_template_delete";

	g_return_val_if_fail( OFO_IS_OPE_TEMPLATE( model ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_ope_template_is_deletable( model ), FALSE );

	if( !OFO_BASE( model )->prot->dispose_has_run ){

		g_debug( "%s: model=%p", thisfn, ( void * ) model );

		if( model_do_delete(
					model,
					ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, model );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
model_do_delete( ofoOpeTemplate *model, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_OPE_TEMPLATES"
			"	WHERE OTE_MNEMO='%s'",
					ofo_ope_template_get_mnemo( model ));

	ok = ofo_sgbd_query( sgbd, query, TRUE );

	g_free( query );

	ok &= model_delete_details( model, sgbd );

	return( ok );
}

static gint
model_cmp_by_mnemo( const ofoOpeTemplate *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_ope_template_get_mnemo( a ), mnemo ));
}

/**
 * ofo_ope_template_get_csv:
 *
 * Prepare for export as a csv file.
 *
 * Returns a #GSList of the csv lines.
 */
GSList *
ofo_ope_template_get_csv( const ofoDossier *dossier )
{
	GList *set, *det;
	GSList *lines;
	gchar *str, *notes, *stamp;
	ofoOpeTemplate *model;
	const gchar *muser;
	sModDetail *sdet;

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	lines = NULL;

	str = g_strdup_printf( "1;Mnemo;Label;Journal;JournalLocked;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "2;Mnemo;Comment;Account;AccountLocked;Label;LabelLocked;Debit;DebitLocked;Credit;CreditLocked" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		model = OFO_OPE_TEMPLATE( set->data );

		notes = my_utils_export_multi_lines( ofo_ope_template_get_notes( model ));
		muser = ofo_ope_template_get_upd_user( model );
		stamp = my_utils_stamp_to_str( ofo_ope_template_get_upd_stamp( model ), MY_STAMP_YYMDHMS );

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s;%s;%s",
				ofo_ope_template_get_mnemo( model ),
				ofo_ope_template_get_label( model ),
				ofo_ope_template_get_ledger( model ),
				ofo_ope_template_get_ledger_locked( model ) ? "True" : "False",
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( notes );
		g_free( stamp );

		lines = g_slist_prepend( lines, str );

		for( det=model->priv->details ; det ; det=det->next ){
			sdet = ( sModDetail * ) det->data;

			str = g_strdup_printf( "2;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
					ofo_ope_template_get_mnemo( model ),
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

/**
 * ofo_ope_template_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - model mnemo
 * - label
 * - notes (opt)
 *
 * - 2:
 * - model mnemo
 * - begin validity (opt)
 * - end validity (opt)
 * - rate
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'model' records, then all 'validity' records...
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_ope_template_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_ope_template_import_csv";
	gint type;
	GSList *ili, *ico;
	ofoOpeTemplate *model;
	sModDetail *sdet;
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

	OFO_BASE_SET_GLOBAL( st_global, dossier, model );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count <= 2 && with_header )){
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
					model = model_import_csv_model( ico, count, &errors );
					if( model ){
						new_set = g_list_prepend( new_set, model );
					}
					break;
				case 2:
					mnemo = NULL;
					sdet = model_import_csv_detail( ico, count, &errors, &mnemo );
					if( sdet ){
						model = model_find_by_mnemo( new_set, mnemo );
						if( model ){
							model->priv->details =
									g_list_append( model->priv->details, sdet );
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

		model_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			model_do_insert(
					OFO_OPE_TEMPLATE( ise->data ),
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ));
			g_debug( "%s: inserting into sgbd", thisfn );
		}

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_OPE_TEMPLATE );

		g_list_free( new_set );

		st_global->send_signal_new = TRUE;
	}
}

static ofoOpeTemplate *
model_import_csv_model( GSList *fields, gint count, gint *errors )
{
	static const gchar *thisfn = "ofo_ope_template_import_csv_model";
	ofoOpeTemplate *model;
	const gchar *str;
	GSList *ico;
	gboolean locked;
	gchar *splitted;

	model = ofo_ope_template_new();

	/* model mnemo */
	ico = fields->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty mnemo", thisfn, count );
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_mnemo( model, str );

	/* model label */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty label", thisfn, count );
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_label( model, str );

	/* model ledger */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty ledger", thisfn, count );
		*errors += 1;
		g_object_unref( model );
		return( NULL );
	}
	ofo_ope_template_set_ledger( model, str );

	/* model ledger locked
	 * default to false if not set, but must be valid if set */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	locked = my_utils_boolean_from_str( str );
	ofo_ope_template_set_ledger_locked( model, locked );

	/* notes
	 * we are tolerant on the last field... */
	ico = ico->next;
	if( ico ){
		str = ( const gchar * ) ico->data;
		if( str && g_utf8_strlen( str, -1 )){
			splitted = my_utils_import_multi_lines( str );
			ofo_ope_template_set_notes( model, splitted );
			g_free( splitted );
		}
	}

	return( model );
}

static sModDetail *
model_import_csv_detail( GSList *fields, gint count, gint *errors, gchar **mnemo )
{
	static const gchar *thisfn = "ofo_ope_template_import_csv_detail";
	sModDetail *detail;
	const gchar *str;
	GSList *ico;

	detail = g_new0( sModDetail, 1 );

	/* model mnemo */
	ico = fields->next;
	str = ( const gchar * ) ico->data;
	if( !str || !g_utf8_strlen( str, -1 )){
		g_warning( "%s: (line %d) empty mnemo", thisfn, count );
		*errors += 1;
		g_free( detail );
		return( NULL );
	}
	*mnemo = g_strdup( str );

	/* detail comment */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( str && g_utf8_strlen( str, -1 )){
		detail->comment = g_strdup( str );
	}

	/* detail label */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( str && g_utf8_strlen( str, -1 )){
		detail->label = g_strdup( str );
	}

	/* detail label locked */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	detail->label_locked = my_utils_boolean_from_str( str );

	/* detail debit */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( str && g_utf8_strlen( str, -1 )){
		detail->debit = g_strdup( str );
	}

	/* detail debit locked */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	detail->debit_locked = my_utils_boolean_from_str( str );

	/* detail credit */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	if( str && g_utf8_strlen( str, -1 )){
		detail->credit = g_strdup( str );
	}

	/* detail credit locked */
	ico = ico->next;
	str = ( const gchar * ) ico->data;
	detail->credit_locked = my_utils_boolean_from_str( str );

	return( detail );
}

static gboolean
model_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_OPE_TEMPLATES", TRUE ) &&
			ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_OPE_TEMPLATES_DET", TRUE ));
}
