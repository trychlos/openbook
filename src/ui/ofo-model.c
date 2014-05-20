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
#include "ui/ofo-model.h"

/* priv instance data
 */
struct _ofoModelPrivate {
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
	gint     journal;
	gchar   *notes;
	gchar   *maj_user;
	GTimeVal maj_stamp;

	/* lignes de détail
	 */
	GList   *details;					/* an (un-ordered) list of detail lines */
};

typedef struct {
	gchar   *comment;
	gchar   *account;					/* account */
	gboolean account_locked;			/* account is locked */
	gchar   *label;
	gboolean label_locked;
	gchar   *debit;
	gboolean debit_locked;
	gchar   *credit;
	gboolean credit_locked;
}
	sModDetail;

G_DEFINE_TYPE( ofoModel, ofo_model, OFO_TYPE_BASE )

#define OFO_MODEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_MODEL, ofoModelPrivate))

static void   details_list_free( ofoModel *model );
static void   details_list_free_detail( sModDetail *detail );

static void
ofo_model_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_model_finalize";
	ofoModel *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_MODEL( instance );

	g_free( self->priv->mnemo );
	g_free( self->priv->label );
	g_free( self->priv->notes );
	g_free( self->priv->maj_user );

	if( self->priv->details ){
		details_list_free( self );
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_model_parent_class )->finalize( instance );
}

static void
ofo_model_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_model_dispose";
	ofoModel *self;

	self = OFO_MODEL( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				self->priv->mnemo, self->priv->label );

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_model_parent_class )->dispose( instance );
}

static void
ofo_model_init( ofoModel *self )
{
	static const gchar *thisfn = "ofo_model_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_MODEL_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	self->priv->id = -1;
	self->priv->journal = -1;
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

static void
details_list_free( ofoModel *model )
{
	ofoModelPrivate *priv;

	g_return_if_fail( OFO_IS_MODEL( model ));

	priv = model->priv;

	if( priv->details ){
		g_list_free_full( priv->details, ( GDestroyNotify ) details_list_free_detail );
		priv->details = NULL;
	}
}

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
 * ofo_model_load_set:
 *
 * Loads/reloads the ordered list of models
 */
GList *
ofo_model_load_set( ofaSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_model_load_set";
	GSList *result, *irow, *icol;
	ofoModel *model;
	GList *set, *im;
	gchar *query;
	sModDetail *detail;
	GList *details;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofa_sgbd_query_ex( sgbd, NULL,
			"SELECT MOD_ID,MOD_MNEMO,MOD_LABEL,MOD_JOU_ID,MOD_NOTES,"
			"	MOD_MAJ_USER,MOD_MAJ_STAMP "
			"	FROM OFA_T_MODELES "
			"	ORDER BY MOD_MNEMO ASC" );

	set = NULL;

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
		ofo_model_set_notes( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_maj_user( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_maj_stamp( model, my_utils_stamp_from_str(( gchar * ) icol->data ));

		g_debug( "%s: mnemo=%s", thisfn, model->priv->mnemo );
		set = g_list_prepend( set, model );
	}

	ofa_sgbd_free_result( result );

	for( im=set ; im ; im=im->next ){

		model = OFO_MODEL( im->data );

		query = g_strdup_printf(
				"SELECT MOD_DET_COMMENT,"
				"	MOD_DET_ACCOUNT,MOD_DET_ACCOUNT_VER,"
				"	MOD_DET_LABEL,MOD_DET_LABEL_VER,"
				"	MOD_DET_DEBIT,MOD_DET_DEBIT_VER,"
				"	MOD_DET_CREDIT,MOD_DET_CREDIT_VER "
				"	FROM OFA_T_MODELES_DET "
				"	WHERE MOD_ID=%d ORDER BY MOD_DET_RANG ASC", ofo_model_get_id( model ));

		result = ofa_sgbd_query_ex( sgbd, NULL, query );
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

			g_debug( "%s: detail=%s", thisfn, detail->comment );
			details = g_list_prepend( details, detail );
		}

		ofa_sgbd_free_result( result );
		model->priv->details = g_list_reverse( details );
	}

	return( g_list_reverse( set ));
}

/**
 * ofo_model_dump_set:
 */
void
ofo_model_dump_set( GList *set )
{
	static const gchar *thisfn = "ofo_model_dump_set";
	ofoModelPrivate *priv;
	GList *ic;

	for( ic=set ; ic ; ic=ic->next ){
		priv = OFO_MODEL( ic->data )->priv;
		g_debug( "%s: model %s - %s", thisfn, priv->mnemo, priv->label );
	}
}

/**
 * ofo_model_get_id:
 */
gint
ofo_model_get_id( const ofoModel *model )
{
	gint id;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	id = -1;

	if( !model->priv->dispose_has_run ){

		id = model->priv->id;
	}

	return( id );
}

/**
 * ofo_model_get_mnemo:
 */
const gchar *
ofo_model_get_mnemo( const ofoModel *model )
{
	const gchar *mnemo = NULL;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		mnemo = model->priv->mnemo;
	}

	return( mnemo );
}

/**
 * ofo_model_get_label:
 */
const gchar *
ofo_model_get_label( const ofoModel *model )
{
	const gchar *label = NULL;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		label = model->priv->label;
	}

	return( label );
}

/**
 * ofo_model_get_journal:
 */
gint
ofo_model_get_journal( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), -1 );

	if( !model->priv->dispose_has_run ){

		return( model->priv->journal );
	}

	return( -1 );
}

/**
 * ofo_model_get_notes:
 */
const gchar *
ofo_model_get_notes( const ofoModel *model )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		notes = model->priv->notes;
	}

	return( notes );
}

/**
 * ofo_model_set_id:
 */
void
ofo_model_set_id( ofoModel *model, gint id )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		model->priv->id = id;
	}
}

/**
 * ofo_model_set_mnemo:
 */
void
ofo_model_set_mnemo( ofoModel *model, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		model->priv->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_model_set_label:
 */
void
ofo_model_set_label( ofoModel *model, const gchar *label )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		model->priv->label = g_strdup( label );
	}
}

/**
 * ofo_model_set_journal:
 */
void
ofo_model_set_journal( ofoModel *model, gint journal )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		model->priv->journal = journal;
	}
}

/**
 * ofo_model_set_notes:
 */
void
ofo_model_set_notes( ofoModel *model, const gchar *notes )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		model->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_model_set_maj_user:
 */
void
ofo_model_set_maj_user( ofoModel *model, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		model->priv->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_model_set_maj_stamp:
 */
void
ofo_model_set_maj_stamp( ofoModel *model, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		memcpy( &model->priv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_model_get_detail_count:
 */
gint
ofo_model_get_detail_count( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), -1 );

	if( !model->priv->dispose_has_run ){

		return( g_list_length( model->priv->details ));
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
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

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );

	if( !model->priv->dispose_has_run ){

		idet = g_list_nth( model->priv->details, idx );
		sdet = ( sModDetail * ) idet->data;
		return( sdet->credit_locked );
	}

	return( FALSE );
}

/**
 * ofo_model_set_detail:
 */
void
ofo_model_set_detail( const ofoModel *model, gint idx, const gchar *comment,
						const gchar *account, gboolean account_locked,
						const gchar *label, gboolean label_locked,
						const gchar *debit, gboolean debit_locked,
						const gchar *credit, gboolean credit_locked )
{
	gint count, i;
	sModDetail *detail;
	GList *idet;

	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->priv->dispose_has_run ){

		count = g_list_length( model->priv->details );
		if( idx >= count ){
			for( i=count ; i<=idx ; ++i ){
				detail = g_new0( sModDetail, 1 );
				model->priv->details = g_list_append( model->priv->details, detail );
			}
		}

		idet = g_list_nth( model->priv->details, idx );
		g_return_if_fail( idet );

		detail = ( sModDetail * ) idet->data;

		g_free( detail->comment );
		detail->comment = g_strdup( comment );

		g_free( detail->account );
		detail->account = g_strdup( account );
		detail->account_locked = account_locked;

		g_free( detail->label );
		detail->label = g_strdup( label );
		detail->label_locked = label_locked;

		g_free( detail->debit );
		detail->debit = g_strdup( debit );
		detail->debit_locked = debit_locked;

		g_free( detail->credit );
		detail->credit = g_strdup( credit );
		detail->credit_locked = credit_locked;
	}
}

static gboolean
ofo_model_insert_details( ofoModel *model, ofaSgbd *sgbd, gint rang, sModDetail *detail )
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

	ok = ofa_sgbd_query( sgbd, NULL, query->str );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
ofo_model_delete_details( ofoModel *model, ofaSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_MODELES_DET WHERE MOD_ID=%d",
			ofo_model_get_id( model ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}

static gboolean
ofo_model_insert_details_ex( ofoModel *model, ofaSgbd *sgbd )
{
	gboolean ok;
	GList *idet;
	sModDetail *sdet;
	gint rang;

	ok = FALSE;

	if( ofo_model_delete_details( model, sgbd )){

		ok = TRUE;

		for( idet=model->priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){

			sdet = ( sModDetail * ) idet->data;
			if( !ofo_model_insert_details( model, sgbd, rang, sdet )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

/**
 * ofo_model_insert:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_model_insert( ofoModel *model, ofaSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_model_get_label( model ));
	notes = my_utils_quote( ofo_model_get_notes( model ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_MODELES" );

	g_string_append_printf( query,
			"	(MOD_MNEMO,MOD_LABEL,MOD_JOU_ID,MOD_NOTES,"
			"	MOD_MAJ_USER, MOD_MAJ_STAMP) VALUES ('%s','%s',%d,",
			ofo_model_get_mnemo( model ),
			label,
			ofo_model_get_journal( model ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", user, stamp );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		ofo_model_set_maj_user( model, user );
		ofo_model_set_maj_stamp( model, my_utils_stamp_from_str( stamp ));

		g_string_printf( query,
				"SELECT MOD_ID FROM OFA_T_MODELES"
				"	WHERE MOD_MNEMO='%s'",
				ofo_model_get_mnemo( model ));

		result = ofa_sgbd_query_ex( sgbd, NULL, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_model_set_id( model, atoi(( gchar * ) icol->data ));

			ofa_sgbd_free_result( result );

			ok = ofo_model_insert_details_ex( model, sgbd );
		}
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_model_update:
 *
 * we deal here with an update of publicly modifiable model properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_model_update( ofoModel *model, ofaSgbd *sgbd, const gchar *user, const gchar *prev_mnemo )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	const gchar *new_mnemo;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	ok = FALSE;
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

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "MOD_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "MOD_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	MOD_MAJ_USER='%s',MOD_MAJ_STAMP='%s'"
			"	WHERE MOD_MNEMO='%s'",
					user,
					stamp,
					prev_mnemo );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		ofo_model_set_maj_user( model, user );
		ofo_model_set_maj_stamp( model, my_utils_stamp_from_str( stamp ));

		ok = ofo_model_insert_details_ex( model, sgbd );
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

/**
 * ofo_model_delete:
 */
gboolean
ofo_model_delete( ofoModel *model, ofaSgbd *sgbd, const gchar *user )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_MODEL( model ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_MODELES"
			"	WHERE MOD_MNEMO='%s'",
					ofo_model_get_mnemo( model ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
