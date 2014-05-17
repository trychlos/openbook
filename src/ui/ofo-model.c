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

/* private class data
 */
struct _ofoModelClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
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
	gint     family;
	gchar   *journal;
	gchar   *notes;
	gchar   *maj_user;
	GTimeVal maj_stamp;

	/* lignes de détail
	 */
	GList   *details;					/* an (un-ordered) list of detail lines */
};

typedef struct {
	gint     rang;						/* the order of the entry, from 0 */
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

static ofoBaseClass *st_parent_class  = NULL;

static GType  register_type( void );
static void   class_init( ofoModelClass *klass );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   instance_dispose( GObject *instance );
static void   instance_finalize( GObject *instance );
static void   details_list_free( ofoModel *model );
static void   details_list_free_detail( sModDetail *detail );

GType
ofo_model_get_type( void )
{
	static GType st_type = 0;

	if( !st_type ){
		st_type = register_type();
	}

	return( st_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_model_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoModelClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoModel ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoModel", &info, 0 );

	return( type );
}

static void
class_init( ofoModelClass *klass )
{
	static const gchar *thisfn = "ofo_model_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoModelClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofo_model_instance_init";
	ofoModel *self;

	g_return_if_fail( OFO_IS_MODEL( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFO_MODEL( instance );

	self->private = g_new0( ofoModelPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->id = -1;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_model_instance_dispose";
	ofoModelPrivate *priv;

	g_return_if_fail( OFO_IS_MODEL( instance ));

	priv = ( OFO_MODEL( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				priv->mnemo, priv->label );

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
		g_free( priv->label );
		g_free( priv->journal );
		g_free( priv->notes );
		g_free( priv->maj_user );

		if( priv->details ){
			details_list_free( OFO_MODEL( instance ));
		}

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_model_instance_finalize";
	ofoModelPrivate *priv;

	g_return_if_fail( OFO_IS_MODEL( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_MODEL( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

static void
details_list_free( ofoModel *model )
{
	ofoModelPrivate *priv;

	g_return_if_fail( OFO_IS_MODEL( model ));

	priv = model->private;

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
			"SELECT MOD_ID,MOD_MNEMO,MOD_LABEL,MOD_FAM_ID,MOD_JOU_ID,MOD_NOTES,"
			"	MOD_MAJ_USER,MOD_MAJ_STAMP"
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
			ofo_model_set_family( model, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_model_set_journal( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_notes( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_maj_user( model, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_model_set_maj_stamp( model, my_utils_stamp_from_str(( gchar * ) icol->data ));

		set = g_list_prepend( set, model );
	}

	ofa_sgbd_free_result( result );

	for( im=set ; im ; im=im->next ){

		model = OFO_MODEL( im->data );

		query = g_strdup_printf(
				"SELECT MOD_DET_RANG,MODE_DET_COMMENT,"
				"	MOD_DET_CPT,MOD_DET_CPT_VER,"
				"	MOD_DET_LABEL,MOD_DET_LABEL_VER,"
				"	MOD_DET_MONTANT,MOD_DET_MONTANT_VER,"
				"	MOD_DET_SENS,MOD_DET_SENS_VER,"
				"	FROM OFA_T_MODELES_DET "
				"	WHERE MOD_ID=%d", ofo_model_get_id( model ));

		result = ofa_sgbd_query_ex( sgbd, NULL, query );
		details = NULL;

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			detail = g_new0( sModDetail, 1 );
			detail->rang = atoi(( gchar * ) icol->data );
			icol = icol->next;
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

		ofa_sgbd_free_result( result );
		model->private->details = details;
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
		priv = OFO_MODEL( ic->data )->private;
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

	if( !model->private->dispose_has_run ){

		id = model->private->id;
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

	if( !model->private->dispose_has_run ){

		mnemo = model->private->mnemo;
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

	if( !model->private->dispose_has_run ){

		label = model->private->label;
	}

	return( label );
}

/**
 * ofo_model_get_family:
 */
gint
ofo_model_get_family( const ofoModel *model )
{
	gint family;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	family = -1;

	if( !model->private->dispose_has_run ){

		family = model->private->family;
	}

	return( family );
}

/**
 * ofo_model_get_journal:
 */
const gchar *
ofo_model_get_journal( const ofoModel *model )
{
	const gchar *journal = NULL;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->private->dispose_has_run ){

		journal = model->private->journal;
	}

	return( journal );
}

/**
 * ofo_model_get_notes:
 */
const gchar *
ofo_model_get_notes( const ofoModel *model )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->private->dispose_has_run ){

		notes = model->private->notes;
	}

	return( notes );
}

/**
 * ofo_model_get_count:
 */
gint
ofo_model_get_count( const ofoModel *model )
{
	g_return_val_if_fail( OFO_IS_MODEL( model ), NULL );

	if( !model->private->dispose_has_run ){

		return( g_list_length( model->private->details ));
	}

	return( 0 );
}

/**
 * ofo_model_set_id:
 */
void
ofo_model_set_id( ofoModel *model, gint id )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->private->dispose_has_run ){

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

	if( !model->private->dispose_has_run ){

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

	if( !model->private->dispose_has_run ){

		model->private->label = g_strdup( label );
	}
}

/**
 * ofo_model_set_family:
 */
void
ofo_model_set_family( ofoModel *model, gint family )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->private->dispose_has_run ){

		model->private->family = family;
	}
}

/**
 * ofo_model_set_journal:
 */
void
ofo_model_set_journal( ofoModel *model, const gchar *journal )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->private->dispose_has_run ){

		model->private->journal = g_strdup( journal );
	}
}

/**
 * ofo_model_set_notes:
 */
void
ofo_model_set_notes( ofoModel *model, const gchar *notes )
{
	g_return_if_fail( OFO_IS_MODEL( model ));

	if( !model->private->dispose_has_run ){

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

	if( !model->private->dispose_has_run ){

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

	if( !model->private->dispose_has_run ){

		memcpy( &model->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
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
			"	(MOD_MNEMO,MOD_LABEL,MOD_JOU_ID,MOD_FAM_ID,MOD_NOTES,"
			"	MOD_MAJ_USER, MOD_MAJ_STAMP) VALUES ('%s','%s','%s',%d",
			ofo_model_get_mnemo( model ),
			label,
			ofo_model_get_journal( model ),
			ofo_model_get_family( model ));

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
				"	WHERE MOD_MNEMO='%s' AND MOD_MAJ_STAMP='%s'",
				ofo_model_get_mnemo( model ), stamp );

		result = ofa_sgbd_query_ex( sgbd, NULL, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_model_set_id( model, atoi(( gchar * ) icol->data ));

			ofa_sgbd_free_result( result );

			ok = TRUE;
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

	query = g_string_new( "UPDATE OFA_T_JOURNAUX SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "JOU_MNEMO='%s',", new_mnemo );
	}

	g_string_append_printf( query, "JOU_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "JOU_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "JOU_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	JOU_MAJ_USER='%s',JOU_MAJ_STAMP='%s'"
			"	WHERE JOU_MNEMO='%s'",
					user,
					stamp,
					prev_mnemo );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		ofo_model_set_maj_user( model, user );
		ofo_model_set_maj_stamp( model, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
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
			"DELETE FROM OFA_T_JOURNAUX"
			"	WHERE JOU_MNEMO='%s'",
					ofo_model_get_mnemo( model ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
