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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-idataset.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"

#include "tva/ofo-tva-form.h"

/* priv instance data
 */
enum {
	TFO_MNEMO = 1,
	TFO_LABEL,
	TFO_HAS_CORRESPONDENCE,
	TFO_NOTES,
	TFO_UPD_USER,
	TFO_UPD_STAMP,
	TFO_BOOL_ROW,
	TFO_BOOL_LABEL,
	TFO_DET_ROW,
	TFO_DET_LEVEL,
	TFO_DET_CODE,
	TFO_DET_LABEL,
	TFO_DET_HAS_BASE,
	TFO_DET_BASE,
	TFO_DET_HAS_AMOUNT,
	TFO_DET_AMOUNT
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_HAS_CORRESPONDENCE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( TFO_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_details_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_DET_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_LEVEL ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_CODE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_BASE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_BASE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_HAS_AMOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_DET_AMOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_bools_defs[] = {
		{ OFA_BOX_CSV( TFO_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( TFO_BOOL_ROW ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( TFO_BOOL_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

struct _ofoTVAFormPrivate {

	/* the details of the form as a GList of GList fields
	 */
	GList     *bools;
	GList     *details;
};

/* mnemonic max length */
#define MNEMO_LENGTH                    10

static ofoBaseClass *ofo_tva_form_parent_class = NULL;

static void            on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, void *user_data );
static gboolean        do_update_account_identifier( ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id );
static GList          *tva_form_load_dataset( ofoDossier *dossier );
static ofoTVAForm     *form_find_by_mnemo( GList *set, const gchar *mnemo );
static guint           form_count_for_account( const ofaIDBConnect *cnx, const gchar *account );
static void            tva_form_set_upd_user( ofoTVAForm *form, const gchar *upd_user );
static void            tva_form_set_upd_stamp( ofoTVAForm *form, const GTimeVal *upd_stamp );
static gboolean        form_do_insert( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user );
static gboolean        form_insert_main( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user );
static gboolean        form_delete_details( ofoTVAForm *form, const ofaIDBConnect *cnx );
static gboolean        form_delete_bools( ofoTVAForm *form, const ofaIDBConnect *cnx );
static gboolean        form_insert_details_ex( ofoTVAForm *form, const ofaIDBConnect *cnx );
static gboolean        form_insert_details( ofoTVAForm *form, const ofaIDBConnect *cnx, guint rang, GList *details );
static gboolean        form_insert_bools( ofoTVAForm *form, const ofaIDBConnect *cnx, guint rang, GList *details );
static gboolean        form_do_update( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo );
static gboolean        form_update_main( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo );
static gboolean        form_do_delete( ofoTVAForm *form, const ofaIDBConnect *cnx );
static gint            form_cmp_by_mnemo( const ofoTVAForm *a, const gchar *mnemo );
static gint            tva_form_cmp_by_ptr( const ofoTVAForm *a, const ofoTVAForm *b );

OFA_IDATASET_LOAD( TVA_FORM, tva_form );

static void
details_list_free_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
details_list_free( ofoTVAForm *form )
{
	g_list_free_full( form->priv->details, ( GDestroyNotify ) details_list_free_detail );
	form->priv->details = NULL;
}

static void
bools_list_free_bool( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
bools_list_free( ofoTVAForm *form )
{
	g_list_free_full( form->priv->bools, ( GDestroyNotify ) bools_list_free_bool );
	form->priv->bools = NULL;
}

static void
tva_form_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_tva_form_finalize";
	ofoTVAFormPrivate *priv;

	priv = OFO_TVA_FORM( instance )->priv;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, TFO_MNEMO ));

	g_return_if_fail( instance && OFO_IS_TVA_FORM( instance ));

	/* free data members here */
	if( priv->bools ){
		bools_list_free( OFO_TVA_FORM( instance ));
	}
	if( priv->details ){
		details_list_free( OFO_TVA_FORM( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tva_form_parent_class )->finalize( instance );
}

static void
tva_form_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_TVA_FORM( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_tva_form_parent_class )->dispose( instance );
}

static void
ofo_tva_form_init( ofoTVAForm *self )
{
	static const gchar *thisfn = "ofo_tva_form_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_TVA_FORM, ofoTVAFormPrivate );
}

static void
ofo_tva_form_class_init( ofoTVAFormClass *klass )
{
	static const gchar *thisfn = "ofo_tva_form_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_tva_form_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_finalize;

	g_type_class_add_private( klass, sizeof( ofoTVAFormPrivate ));
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_tva_form_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoTVAFormClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) ofo_tva_form_class_init,
		NULL,
		NULL,
		sizeof( ofoTVAForm ),
		0,
		( GInstanceInitFunc ) ofo_tva_form_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoTVAForm", &info, 0 );

	return( type );
}

GType
ofo_tva_form_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/**
 * ofo_tva_form_connect_handlers:
 *
 * As the signal connection is protected by a static variable, there is
 * no need here to handle signal disconnection
 */
void
ofo_tva_form_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_tva_form_connect_handlers";

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, void *user_data )
{
	static const gchar *thisfn = "ofo_tva_form_on_updated_object";
	const gchar *mnemo;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			mnemo = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( g_utf8_collate( mnemo, prev_id )){
				do_update_account_identifier( dossier, mnemo, prev_id );
			}
		}
	}
}

static gboolean
do_update_account_identifier( ofoDossier *dossier, const gchar *mnemo, const gchar *prev_id )
{
	static const gchar *thisfn = "ofo_tva_form_do_update_account_identifier";
	gchar *query;
	const ofaIDBConnect *cnx;
	GSList *result, *irow, *icol;
	gchar *etp_mnemo, *det_amount;
	gint det_row;
	gboolean ok;

	g_debug( "%s: dossier=%p, mnemo=%s, prev_id=%s",
			thisfn, ( void * ) dossier, mnemo, prev_id );

	cnx = ofo_dossier_get_connect( dossier );

	query = g_strdup_printf(
					"SELECT TFO_MNEMO,TFO_DET_ROW,TFO_DET_AMOUNT "
					"	FROM TVA_T_FORMS_DET "
					"	WHERE TFO_DET_AMOUNT LIKE '%%%s%%'", prev_id );

	ok = ofa_idbconnect_query_ex( cnx, query, &result, TRUE );
	g_free( query );

	if( ok ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			etp_mnemo = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			det_row = atoi(( gchar * ) icol->data );
			icol = icol->next;
			det_amount = my_utils_str_replace(( gchar * ) icol->data, prev_id, mnemo );

			query = g_strdup_printf(
							"UPDATE TVA_T_FORMS_DET "
							"	SET TFO_DET_AMOUNT='%s' "
							"	WHERE TFO_MNEMO='%s' AND TFO_DET_ROW=%d",
									det_amount, etp_mnemo, det_row );

			ofa_idbconnect_query( cnx, query, TRUE );

			g_free( query );
			g_free( det_amount );
			g_free( etp_mnemo );
		}

		ofa_idataset_free_dataset( dossier, OFO_TYPE_TVA_FORM );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_TVA_FORM );
	}

	return( ok );
}

static GList *
tva_form_load_dataset( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_tva_form_load_dataset";
	GList *dataset, *it, *ir;
	ofoTVAForm *form;
	gchar *from;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_connect( dossier ),
					"TVA_T_FORMS ORDER BY TFO_MNEMO ASC",
					OFO_TYPE_TVA_FORM );

	for( it=dataset ; it ; it=it->next ){
		form = OFO_TVA_FORM( it->data );

		from = g_strdup_printf(
				"TVA_T_FORMS_DET WHERE TFO_MNEMO='%s' ORDER BY TFO_DET_ROW ASC",
				ofo_tva_form_get_mnemo( form ));
		form->priv->details =
				ofo_base_load_rows( st_details_defs, ofo_dossier_get_connect( dossier ), from );
		g_free( from );

		/* dump the detail rows */
		if( 0 ){
			for( ir=form->priv->details ; ir ; ir=ir->next ){
				ofa_box_dump_fields_list( thisfn, ir->data );
			}
		}

		from = g_strdup_printf(
				"TVA_T_FORMS_BOOL WHERE TFO_MNEMO='%s' ORDER BY TFO_BOOL_ROW ASC",
				ofo_tva_form_get_mnemo( form ));
		form->priv->bools =
				ofo_base_load_rows( st_bools_defs, ofo_dossier_get_connect( dossier ), from );
		g_free( from );
	}

	return( dataset );
}

/**
 * ofo_tva_form_get_by_mnemo:
 * @dossier:
 * @mnemo:
 *
 * Returns: the searched tva form, or %NULL.
 *
 * The returned object is owned by the #ofoTVAForm class, and should
 * not be unreffed by the caller.
 */
ofoTVAForm *
ofo_tva_form_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo )
{
	/*static const gchar *thisfn = "ofo_tva_form_get_by_mnemo";*/

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( my_strlen( mnemo ), NULL );

	/*g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );*/

	OFA_IDATASET_GET( dossier, TVA_FORM, tva_form );

	return( form_find_by_mnemo( tva_form_dataset, mnemo ));
}

static ofoTVAForm *
form_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) form_cmp_by_mnemo );
	if( found ){
		return( OFO_TVA_FORM( found->data ));
	}

	return( NULL );
}

/**
 * ofo_tva_form_use_account:
 * @dossier:
 * @account:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_tva_form_use_account( ofoDossier *dossier, const gchar *account )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( my_strlen( account ), FALSE );

	OFA_IDATASET_GET( dossier, TVA_FORM, tva_form );

	return( form_count_for_account( ofo_dossier_get_connect( dossier ), account ) > 0 );
}

static guint
form_count_for_account( const ofaIDBConnect *cnx, const gchar *account )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM TVA_T_FORMS_DET WHERE TFO_DET_AMOUNT LIKE '%%%s%%'", account );

	ofa_idbconnect_query_int( cnx, query, &count, TRUE );

	g_free( query );

	return( abs( count ));
}

/**
 * ofo_tva_form_new:
 */
ofoTVAForm *
ofo_tva_form_new( void )
{
	ofoTVAForm *form;

	form = g_object_new( OFO_TYPE_TVA_FORM, NULL );
	OFO_BASE( form )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( form );
}

/**
 * ofo_tva_form_new_from_form:
 * @form: the source #ofoTVAForm to be copied from.
 *
 * Allocates a new #ofoTVAForms object, initializing it with data
 * copied from @form #ofoTVAForm source.
 */
ofoTVAForm *
ofo_tva_form_new_from_form( const ofoTVAForm *form )
{
	ofoTVAForm *dest;
	gint count, i;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	dest = NULL;

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		dest = ofo_tva_form_new();

		ofo_tva_form_set_mnemo( dest, ofo_tva_form_get_mnemo( form ));
		ofo_tva_form_set_label( dest, ofo_tva_form_get_label( form ));
		ofo_tva_form_set_has_correspondence( dest, ofo_tva_form_get_has_correspondence( form ));
		ofo_tva_form_set_notes( dest, ofo_tva_form_get_notes( form ));

		count = ofo_tva_form_detail_get_count( form );
		for( i=0 ; i<count ; ++i ){
			ofo_tva_form_detail_add( dest,
					ofo_tva_form_detail_get_level( form, i ),
					ofo_tva_form_detail_get_code( form, i ),
					ofo_tva_form_detail_get_label( form, i ),
					ofo_tva_form_detail_get_has_base( form, i ),
					ofo_tva_form_detail_get_base( form, i ),
					ofo_tva_form_detail_get_has_amount( form, i ),
					ofo_tva_form_detail_get_amount( form, i ));
		}

		count = ofo_tva_form_boolean_get_count( form );
		for( i=0 ; i<count ; ++i ){
			ofo_tva_form_boolean_add( dest,
					ofo_tva_form_boolean_get_label( form, i ));
		}
	}

	return( dest );
}

/**
 * ofo_tva_form_get_mnemo:
 * @form:
 */
const gchar *
ofo_tva_form_get_mnemo( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_MNEMO );
}

/**
 * ofo_tva_form_get_mnemo_new_from:
 * @form:
 * @dossier:
 *
 * Returns a new mnemo derived from the given one, as a newly allocated
 * string which should be g_free() by the caller.
 */
gchar *
ofo_tva_form_get_mnemo_new_from( const ofoTVAForm *form, ofoDossier *dossier )
{
	const gchar *mnemo;
	gint len_mnemo;
	gchar *str;
	gint i, maxlen;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	str = NULL;

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		mnemo = ofo_tva_form_get_mnemo( form );
		len_mnemo = my_strlen( mnemo );
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
			if( !ofo_tva_form_get_by_mnemo( dossier, str )){
				break;
			}
			g_free( str );
		}
	}

	return( str );
}

/**
 * ofo_tva_form_get_label:
 */
const gchar *
ofo_tva_form_get_label( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_LABEL );
}

/**
 * ofo_tva_form_get_has_correspondence:
 */
gboolean
ofo_tva_form_get_has_correspondence( const ofoTVAForm *form )
{
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		cstr = ofa_box_get_string( OFO_BASE( form )->prot->fields, TFO_HAS_CORRESPONDENCE );
		return( my_utils_boolean_from_str( cstr ));
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofo_tva_form_get_notes:
 */
const gchar *
ofo_tva_form_get_notes( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_NOTES );
}

/**
 * ofo_tva_form_get_upd_user:
 */
const gchar *
ofo_tva_form_get_upd_user( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, string, NULL, TFO_UPD_USER );
}

/**
 * ofo_tva_form_get_upd_stamp:
 */
const GTimeVal *
ofo_tva_form_get_upd_stamp( const ofoTVAForm *form )
{
	ofo_base_getter( TVA_FORM, form, timestamp, NULL, TFO_UPD_STAMP );
}

/**
 * ofo_tva_form_is_deletable:
 * @form: the tva formular.
 * @dossier: the dossier.
 *
 * Returns: %TRUE if the tva formular is deletable.
 */
gboolean
ofo_tva_form_is_deletable( const ofoTVAForm *form, ofoDossier *dossier )
{
	gboolean is_current;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		is_current = ofo_dossier_is_current( dossier );

		return( is_current );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofo_tva_form_is_valid:
 * @dossier:
 * @mnemo:
 * @msgerr: [allow-none][out]:
 *
 * Returns: %TRUE if provided datas are enough to make the future
 * #ofoTVAForm valid, %FALSE else.
 */
gboolean
ofo_tva_form_is_valid( ofoDossier *dossier, const gchar *mnemo, gchar **msgerr )
{
	gboolean ok;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	ok = TRUE;
	if( msgerr ){
		*msgerr = NULL;
	}

	if( !my_strlen( mnemo )){
		ok = FALSE;
		if( msgerr ){
			*msgerr = g_strdup( _( "Empty mnemonic" ));
		}
	}

	return( ok );
}

/**
 * ofo_tva_form_compare_id:
 * @a:
 * @b:
 *
 * Returns: -1 if @a'ids are lesser than @b'ids, 0 if they are equal,
 * +1 if they are greater.
 */
gint
ofo_tva_form_compare_id( const ofoTVAForm *a, const ofoTVAForm *b )
{
	const gchar *ca, *cb;
	gint cmp;

	g_return_val_if_fail( a && OFO_IS_TVA_FORM( a ), 0 );
	g_return_val_if_fail( b && OFO_IS_TVA_FORM( b ), 0 );

	if( !OFO_BASE( a )->prot->dispose_has_run && !OFO_BASE( b )->prot->dispose_has_run ){
		ca = ofo_tva_form_get_mnemo( a );
		cb = ofo_tva_form_get_mnemo( b );
		cmp = g_utf8_collate( ca, cb );
		return( cmp );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_tva_form_set_mnemo:
 */
void
ofo_tva_form_set_mnemo( ofoTVAForm *form, const gchar *mnemo )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_MNEMO, mnemo );
}

/**
 * ofo_tva_form_set_label:
 */
void
ofo_tva_form_set_label( ofoTVAForm *form, const gchar *label )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_LABEL, label );
}

/**
 * ofo_tva_form_set_has_correspondence:
 */
void
ofo_tva_form_set_has_correspondence( ofoTVAForm *form, gboolean has_correspondence )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_HAS_CORRESPONDENCE, has_correspondence ? "Y":"N" );
}

/**
 * ofo_tva_form_set_notes:
 */
void
ofo_tva_form_set_notes( ofoTVAForm *form, const gchar *notes )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_NOTES, notes );
}

/*
 * ofo_tva_form_set_upd_user:
 */
static void
tva_form_set_upd_user( ofoTVAForm *form, const gchar *upd_user )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_UPD_USER, upd_user );
}

/*
 * ofo_tva_form_set_upd_stamp:
 */
static void
tva_form_set_upd_stamp( ofoTVAForm *form, const GTimeVal *upd_stamp )
{
	ofo_base_setter( TVA_FORM, form, string, TFO_UPD_STAMP, upd_stamp );
}

void
ofo_tva_form_detail_add( ofoTVAForm *form,
							guint level,
							const gchar *code, const gchar *label,
							gboolean has_base, const gchar *base,
							gboolean has_amount, const gchar *amount )
{
	GList *fields;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		fields = ofa_box_init_fields_list( st_details_defs );
		ofa_box_set_string( fields, TFO_MNEMO, ofo_tva_form_get_mnemo( form ));
		ofa_box_set_int( fields, TFO_DET_ROW, 1+ofo_tva_form_detail_get_count( form ));
		ofa_box_set_int( fields, TFO_DET_LEVEL, level );
		ofa_box_set_string( fields, TFO_DET_CODE, code );
		ofa_box_set_string( fields, TFO_DET_LABEL, label );
		ofa_box_set_string( fields, TFO_DET_HAS_BASE, has_base ? "Y":"N" );
		ofa_box_set_string( fields, TFO_DET_BASE, base );
		ofa_box_set_string( fields, TFO_DET_HAS_AMOUNT, has_amount ? "Y":"N" );
		ofa_box_set_string( fields, TFO_DET_AMOUNT, amount );

		form->priv->details = g_list_append( form->priv->details, fields );
	}
}

/**
 * ofo_tva_form_detail_free_all:
 */
void
ofo_tva_form_detail_free_all( ofoTVAForm *form )
{
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		details_list_free( form );
	}
}

/**
 * ofo_tva_form_detail_get_count:
 */
guint
ofo_tva_form_detail_get_count( const ofoTVAForm *form )
{
	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		return( g_list_length( form->priv->details ));
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_tva_form_detail_get_level:
 * @idx is the index in the details list, starting with zero
 */
guint
ofo_tva_form_detail_get_level( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	guint value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		value = nth ? ofa_box_get_int( nth->data, TFO_DET_LEVEL ) : 0;
		return( value );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_tva_form_detail_get_code:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_code( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_CODE ) : NULL;
		return( cstr );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_tva_form_detail_get_label:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_label( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_LABEL ) : NULL;
		return( cstr );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_tva_form_detail_get_has_base:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_form_detail_get_has_base( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_BASE ) : NULL;
		value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;
		return( value );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofo_tva_form_detail_get_base:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_base( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_BASE ) : NULL;
		return( cstr );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_tva_form_detail_get_has_amount:
 * @idx is the index in the details list, starting with zero
 */
gboolean
ofo_tva_form_detail_get_has_amount( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;
	gboolean value;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), FALSE );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_HAS_AMOUNT ) : NULL;
		value = my_strlen( cstr ) ? my_utils_boolean_from_str( cstr ) : FALSE;
		return( value );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofo_tva_form_detail_get_amount:
 * @idx is the index in the details list, starting with zero
 */
const gchar *
ofo_tva_form_detail_get_amount( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->details, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_DET_AMOUNT ) : NULL;
		return( cstr );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_tva_form_boolean_add:
 * @form:
 * @label:
 */
void
ofo_tva_form_boolean_add( ofoTVAForm *form, const gchar *label )
{
	GList *fields;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		fields = ofa_box_init_fields_list( st_bools_defs );
		ofa_box_set_string( fields, TFO_MNEMO, ofo_tva_form_get_mnemo( form ));
		ofa_box_set_int( fields, TFO_BOOL_ROW, 1+ofo_tva_form_boolean_get_count( form ));
		ofa_box_set_string( fields, TFO_BOOL_LABEL, label );

		form->priv->bools = g_list_append( form->priv->bools, fields );
	}
}

/**
 * ofo_tva_form_boolean_free_all:
 */
void
ofo_tva_form_boolean_free_all( ofoTVAForm *form )
{
	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		bools_list_free( form );
	}
}

/**
 * ofo_tva_form_boolean_get_count:
 */
guint
ofo_tva_form_boolean_get_count( const ofoTVAForm *form )
{
	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), 0 );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		return( g_list_length( form->priv->bools ));
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_tva_form_boolean_get_label:
 * @idx is the index in the booleans list, starting with zero
 */
const gchar *
ofo_tva_form_boolean_get_label( const ofoTVAForm *form, guint idx )
{
	GList *nth;
	const gchar *cstr;

	g_return_val_if_fail( form && OFO_IS_TVA_FORM( form ), NULL );

	if( !OFO_BASE( form )->prot->dispose_has_run ){

		nth = g_list_nth( form->priv->bools, idx );
		cstr = nth ? ofa_box_get_string( nth->data, TFO_BOOL_LABEL ) : NULL;
		return( cstr );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_tva_form_insert:
 */
gboolean
ofo_tva_form_insert( ofoTVAForm *tva_form, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_tva_form_insert";

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( tva_form )->prot->dispose_has_run ){

		g_debug( "%s: form=%p, dossier=%p",
				thisfn, ( void * ) tva_form, ( void * ) dossier );

		if( form_do_insert(
				tva_form,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, TVA_FORM, tva_form );

			return( TRUE );
		}
	}

	g_return_val_if_reached( FALSE );
}

static gboolean
form_do_insert( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user )
{
	return( form_insert_main( form, cnx, user ) &&
			form_insert_details_ex( form, cnx ));
}

static gboolean
form_insert_main( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes;
	gchar *stamp_str;
	GTimeVal stamp;

	label = my_utils_quote( ofo_tva_form_get_label( form ));
	notes = my_utils_quote( ofo_tva_form_get_notes( form ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO TVA_T_FORMS" );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_LABEL,TFO_HAS_CORRESPONDENCE,"
			"	 TFO_NOTES,TFO_UPD_USER, TFO_UPD_STAMP) VALUES ('%s',",
			ofo_tva_form_get_mnemo( form ));

	if( my_strlen( label )){
		g_string_append_printf( query, "'%s',", label );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofo_tva_form_get_has_correspondence( form ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", user, stamp_str );

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

	tva_form_set_upd_user( form, user );
	tva_form_set_upd_stamp( form, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
form_delete_details( ofoTVAForm *form, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM TVA_T_FORMS_DET WHERE TFO_MNEMO='%s'",
			ofo_tva_form_get_mnemo( form ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
form_delete_bools( ofoTVAForm *form, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM TVA_T_FORMS_BOOL WHERE TFO_MNEMO='%s'",
			ofo_tva_form_get_mnemo( form ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
form_insert_details_ex( ofoTVAForm *form, const ofaIDBConnect *cnx )
{
	gboolean ok;
	GList *idet;
	guint rang;

	ok = FALSE;

	if( form_delete_details( form, cnx ) && form_delete_bools( form, cnx )){
		ok = TRUE;
		for( idet=form->priv->details, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !form_insert_details( form, cnx, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
		for( idet=form->priv->bools, rang=1 ; idet ; idet=idet->next, rang+=1 ){
			if( !form_insert_bools( form, cnx, rang, idet->data )){
				ok = FALSE;
				break;
			}
		}
	}

	return( ok );
}

static gboolean
form_insert_details( ofoTVAForm *form, const ofaIDBConnect *cnx, guint rang, GList *details )
{
	GString *query;
	gboolean ok;
	gchar *code, *label, *base, *amount;
	const gchar *cstr;

	query = g_string_new( "INSERT INTO TVA_T_FORMS_DET " );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_DET_ROW,"
			"	 TFO_DET_LEVEL,TFO_DET_CODE,TFO_DET_LABEL,"
			"	 TFO_DET_HAS_BASE,TFO_DET_BASE,"
			"	 TFO_DET_HAS_AMOUNT,TFO_DET_AMOUNT) "
			"	VALUES('%s',%d,",
			ofo_tva_form_get_mnemo( form ), rang );

	g_string_append_printf( query, "%u,", ofa_box_get_int( details, TFO_DET_LEVEL ));

	code = my_utils_quote( ofa_box_get_string( details, TFO_DET_CODE ));
	if( my_strlen( code )){
		g_string_append_printf( query, "'%s',", code );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( code );

	label = my_utils_quote( ofa_box_get_string( details, TFO_DET_LABEL ));
	if( my_strlen( label )){
		g_string_append_printf( query, "'%s',", label );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( label );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_BASE );
	g_string_append_printf( query, "'%s',", cstr );

	base = my_utils_quote( ofa_box_get_string( details, TFO_DET_BASE ));
	if( my_strlen( base )){
		g_string_append_printf( query, "'%s',", base );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( base );

	cstr = ofa_box_get_string( details, TFO_DET_HAS_AMOUNT );
	g_string_append_printf( query, "'%s',", cstr );

	amount = my_utils_quote( ofa_box_get_string( details, TFO_DET_AMOUNT ));
	if( my_strlen( amount )){
		g_string_append_printf( query, "'%s'", amount );
	} else {
		query = g_string_append( query, "NULL" );
	}
	g_free( amount );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
form_insert_bools( ofoTVAForm *form, const ofaIDBConnect *cnx, guint rang, GList *fields )
{
	GString *query;
	gboolean ok;
	gchar *label;

	query = g_string_new( "INSERT INTO TVA_T_FORMS_BOOL " );

	g_string_append_printf( query,
			"	(TFO_MNEMO,TFO_BOOL_ROW,"
			"	 TFO_BOOL_LABEL) "
			"	VALUES('%s',%d",
			ofo_tva_form_get_mnemo( form ), rang );

	label = my_utils_quote( ofa_box_get_string( fields, TFO_BOOL_LABEL ));
	g_string_append_printf( query, ",'%s'", label );
	g_free( label );

	query = g_string_append( query, ")" );

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_tva_form_update:
 * @form:
 * @dossier:
 * @prev_mnemo:
 */
gboolean
ofo_tva_form_update( ofoTVAForm *tva_form, ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_tva_form_update";

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( my_strlen( prev_mnemo ), FALSE );

	if( !OFO_BASE( tva_form )->prot->dispose_has_run ){

		g_debug( "%s: form=%p, dossier=%p, prev_mnemo=%s",
				thisfn, ( void * ) tva_form, ( void * ) dossier, prev_mnemo );

		if( form_do_update(
				tva_form,
					ofo_dossier_get_connect( dossier ),
					ofo_dossier_get_user( dossier ),
					prev_mnemo )){

			OFA_IDATASET_UPDATE( dossier, TVA_FORM, tva_form, prev_mnemo );

			return( TRUE );
		}
	}

	g_return_val_if_reached( FALSE );
}

static gboolean
form_do_update( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo )
{
	return( form_update_main( form, cnx, user, prev_mnemo ) &&
			form_insert_details_ex( form, cnx ));
}

static gboolean
form_update_main( ofoTVAForm *form, const ofaIDBConnect *cnx, const gchar *user, const gchar *prev_mnemo )
{
	gboolean ok;
	GString *query;
	gchar *label, *notes;
	const gchar *new_mnemo;
	gchar *stamp_str;
	GTimeVal stamp;

	label = my_utils_quote( ofo_tva_form_get_label( form ));
	notes = my_utils_quote( ofo_tva_form_get_notes( form ));
	new_mnemo = ofo_tva_form_get_mnemo( form );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE TVA_T_FORMS SET " );

	if( g_utf8_collate( new_mnemo, prev_mnemo )){
		g_string_append_printf( query, "TFO_MNEMO='%s',", new_mnemo );
	}

	if( my_strlen( label )){
		g_string_append_printf( query, "TFO_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "TFO_LABEL=NULL," );
	}

	g_string_append_printf(
			query, "TFO_HAS_CORRESPONDENCE='%s',",
			ofo_tva_form_get_has_correspondence( form ) ? "Y":"N" );

	if( my_strlen( notes )){
		g_string_append_printf( query, "TFO_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "TFO_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	TFO_UPD_USER='%s',TFO_UPD_STAMP='%s'"
			"	WHERE TFO_MNEMO='%s'",
					user,
					stamp_str,
					prev_mnemo );

	ok = ofa_idbconnect_query( cnx, query->str, TRUE );

	tva_form_set_upd_user( form, user );
	tva_form_set_upd_stamp( form, &stamp );

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );
	g_free( label );

	return( ok );
}

/**
 * ofo_tva_form_delete:
 */
gboolean
ofo_tva_form_delete( ofoTVAForm *tva_form, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_tva_form_delete";

	g_return_val_if_fail( tva_form && OFO_IS_TVA_FORM( tva_form ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_tva_form_is_deletable( tva_form, dossier ), FALSE );

	if( !OFO_BASE( tva_form )->prot->dispose_has_run ){

		g_debug( "%s: form=%p, dossier=%p",
				thisfn, ( void * ) tva_form, ( void * ) dossier );

		if( form_do_delete(
					tva_form,
					ofo_dossier_get_connect( dossier ))){

			OFA_IDATASET_REMOVE( dossier, TVA_FORM, tva_form );

			return( TRUE );
		}
	}

	g_return_val_if_reached( FALSE );
}

static gboolean
form_do_delete( ofoTVAForm *form, const ofaIDBConnect *cnx )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM TVA_T_FORMS"
			"	WHERE TFO_MNEMO='%s'",
					ofo_tva_form_get_mnemo( form ));

	ok = ofa_idbconnect_query( cnx, query, TRUE );

	g_free( query );

	if( ok ){
		ok = form_delete_details( form, cnx ) && form_delete_bools( form, cnx );
	}

	return( ok );
}

static gint
form_cmp_by_mnemo( const ofoTVAForm *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_tva_form_get_mnemo( a ), mnemo ));
}

static gint
tva_form_cmp_by_ptr( const ofoTVAForm *a, const ofoTVAForm *b )
{
	return( form_cmp_by_mnemo( a, ofo_tva_form_get_mnemo( b )));
}
