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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "ui/my-utils.h"
#include "ui/ofo-base.h"
#include "ui/ofo-base-prot.h"
#include "ui/ofo-account.h"
#include "ui/ofo-devise.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoEntryPrivate {

	/* sgbd data
	 */
	GDate          effect;
	gint           number;
	GDate          operation;
	gchar         *label;
	gchar         *ref;
	gchar         *account;
	gchar         *devise;
	gint           jou_id;
	gdouble        amount;
	ofaEntrySens   sens;
	ofaEntryStatus status;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
	GDate          rappro;
};

G_DEFINE_TYPE( ofoEntry, ofo_entry, OFO_TYPE_BASE )

static GList   *entry_load_dataset( const ofoSgbd *sgbd, const gchar *account, ofaEntryConcil mode );
static gint     entry_count_for_devise( const ofoSgbd *sgbd, const gchar *devise );
static gint     entry_count_for_journal( const ofoSgbd *sgbd, gint jou_id );
static gboolean entry_do_insert( ofoEntry *entry, const ofoSgbd *sgbd, const gchar *user );
static void     error_journal( gint jou_id );
static void     error_currency( const gchar *devise );
static void     error_acc_number( void );
static void     error_account( const gchar *number );
static void     error_acc_currency( const ofoDossier *dossier, const gchar *devise, ofoAccount *account );
static void     error_entry( const gchar *message );
static gboolean do_update_rappro( ofoEntry *entry, const ofoSgbd *sgbd );

static void
ofo_entry_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_finalize";
	ofoEntryPrivate *priv;

	priv = OFO_ENTRY( instance )->private;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->label );

	/* free data members here */
	g_free( priv->label );
	g_free( priv->devise );
	g_free( priv->ref );
	g_free( priv->maj_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->finalize( instance );
}

static void
ofo_entry_dispose( GObject *instance )
{
	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->dispose( instance );
}

static void
ofo_entry_init( ofoEntry *self )
{
	static const gchar *thisfn = "ofo_entry_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoEntryPrivate, 1 );

	self->private->number = OFO_BASE_UNSET_ID;
	self->private->jou_id = OFO_BASE_UNSET_ID;
	g_date_clear( &self->private->effect, 1 );
	g_date_clear( &self->private->operation, 1 );
	g_date_clear( &self->private->rappro, 1 );
}

static void
ofo_entry_class_init( ofoEntryClass *klass )
{
	static const gchar *thisfn = "ofo_entry_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_entry_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_entry_finalize;
}

/**
 * ofo_entry_new:
 */
ofoEntry *
ofo_entry_new( void )
{
	return( g_object_new( OFO_TYPE_ENTRY, NULL ));
}

/**
 * ofo_entry_get_dataset:
 */
GList *
ofo_entry_get_dataset( const ofoDossier *dossier, const gchar *account, ofaEntryConcil mode )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	return( entry_load_dataset( ofo_dossier_get_sgbd( dossier ), account, mode ));
}

static GList *
entry_load_dataset( const ofoSgbd *sgbd, const gchar *account, ofaEntryConcil mode )
{
	GList *dataset;
	GString *query;
	GSList *result, *irow, *icol;
	ofoEntry *entry;

	dataset = NULL;
	query = g_string_new( "" );
	g_string_printf( query,
				"SELECT ECR_DOPE,ECR_DEFFET,ECR_NUMBER,ECR_LABEL,ECR_REF,"
				"	ECR_DEV_CODE,ECR_JOU_ID,ECR_MONTANT,ECR_SENS,"
				"	ECR_STATUS,ECR_MAJ_USER,ECR_MAJ_STAMP,ECR_RAPPRO "
				"	FROM OFA_T_ECRITURES "
				"	WHERE ECR_COMPTE='%s' AND ECR_STATUS!=2 ",
						account );
	switch( mode ){
		case ENT_CONCILED_YES:
			g_string_append_printf( query,
				"	AND ECR_RAPPRO!=0" );
			break;
		case ENT_CONCILED_NO:
			g_string_append_printf( query,
				"	AND ECR_RAPPRO=0" );
			break;
		case ENT_CONCILED_ALL:
			break;
	}

	query = g_string_append( query, " ORDER BY ECR_DOPE ASC,ECR_DEFFET ASC,ECR_NUMBER ASC" );
	result = ofo_sgbd_query_ex( sgbd, query->str );
	g_string_free( query, TRUE );

	if( result ){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			entry = ofo_entry_new();
			ofo_entry_set_dope( entry, my_utils_date_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_entry_set_deffect( entry, my_utils_date_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_entry_set_number( entry, atoi(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_entry_set_label( entry, ( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				ofo_entry_set_ref( entry, ( gchar * ) icol->data );
			}
			icol = icol->next;
			ofo_entry_set_devise( entry, ( gchar * ) icol->data );
			icol = icol->next;
			ofo_entry_set_journal( entry, atoi(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_entry_set_amount( entry, g_ascii_strtod(( gchar * ) icol->data, NULL ));
			icol = icol->next;
			ofo_entry_set_sens( entry, atoi(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_entry_set_status( entry, atoi(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_entry_set_maj_user( entry, ( gchar * ) icol->data );
			icol = icol->next;
			ofo_entry_set_maj_stamp( entry, my_utils_stamp_from_str(( gchar * ) icol->data ));
			icol = icol->next;
			if( icol->data ){
				ofo_entry_set_rappro( entry, my_utils_date_from_str(( gchar * ) icol->data ));
			}

			dataset = g_list_prepend( dataset, entry );
		}

		ofo_sgbd_free_result( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_entry_free_dataset:
 */
void
ofo_entry_free_dataset( GList *dataset )
{
	g_list_free_full( dataset, g_object_unref );
}

/**
 * ofo_entry_use_devise:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_entry_use_devise( const ofoDossier *dossier, const gchar *devise )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( entry_count_for_devise( ofo_dossier_get_sgbd( dossier ), devise ) > 0 );
}

static gint
entry_count_for_devise( const ofoSgbd *sgbd, const gchar *devise )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ECRITURES "
				"	WHERE ECR_DEV_CODE='%s'",
					devise );

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
 * ofo_entry_use_journal:
 *
 * Returns: %TRUE if a recorded entry makes use of the specified currency.
 */
gboolean
ofo_entry_use_journal( const ofoDossier *dossier, gint jou_id )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( entry_count_for_journal( ofo_dossier_get_sgbd( dossier ), jou_id ) > 0 );
}

static gint
entry_count_for_journal( const ofoSgbd *sgbd, gint jou_id )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ECRITURES "
				"	WHERE ECR_JOU_ID=%d",
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
 * ofo_entry_get_number:
 */
gint
ofo_entry_get_number( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->number );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_entry_get_label:
 */
const gchar *
ofo_entry_get_label( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->label );
	}

	return( NULL );
}

/**
 * ofo_entry_get_deffect:
 */
const GDate *
ofo_entry_get_deffect( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GDate * ) &entry->private->effect );
	}

	return( NULL );
}

/**
 * ofo_entry_get_dope:
 */
const GDate *
ofo_entry_get_dope( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GDate * ) &entry->private->operation );
	}

	return( NULL );
}

/**
 * ofo_entry_get_ref:
 */
const gchar *
ofo_entry_get_ref( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->ref );
	}

	return( NULL );
}

/**
 * ofo_entry_get_account:
 */
const gchar *
ofo_entry_get_account( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->account );
	}

	return( NULL );
}

/**
 * ofo_entry_get_devise:
 */
const gchar *
ofo_entry_get_devise( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->private->devise );
	}

	return( NULL );
}

/**
 * ofo_entry_get_journal:
 */
gint
ofo_entry_get_journal( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->jou_id );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_entry_get_amount:
 */
gdouble
ofo_entry_get_amount( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), 0.0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->amount );
	}

	return( 0.0 );
}

/**
 * ofo_entry_get_sens:
 */
ofaEntrySens
ofo_entry_get_sens( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->sens );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_entry_get_status:
 */
ofaEntryStatus
ofo_entry_get_status( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( entry->private->status );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_entry_get_rappro:
 */
const GDate *
ofo_entry_get_rappro( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GDate * ) &entry->private->rappro );
	}

	return( NULL );
}

/**
 * ofo_entry_get_maj_user:
 */
const gchar *
ofo_entry_get_maj_user( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const gchar * ) entry->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_get_maj_stamp:
 */
const GTimeVal *
ofo_entry_get_maj_stamp( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &entry->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_entry_set_number:
 */
void
ofo_entry_set_number( ofoEntry *entry, gint number )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( number > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->private->number = number;
	}
}

/**
 * ofo_entry_set_label:
 */
void
ofo_entry_set_label( ofoEntry *entry, const gchar *label )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( label && g_utf8_strlen( label, -1 ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->private->label );
		entry->private->label = g_strdup( label );
	}
}

/**
 * ofo_entry_set_deffect:
 */
void
ofo_entry_set_deffect( ofoEntry *entry, const GDate *deffect )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( deffect && g_date_valid( deffect ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		memcpy( &entry->private->effect, deffect, sizeof( GDate ));
	}
}

/**
 * ofo_entry_set_dope:
 */
void
ofo_entry_set_dope( ofoEntry *entry, const GDate *dope )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( dope && g_date_valid( dope ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		memcpy( &entry->private->operation, dope, sizeof( GDate ));
	}
}

/**
 * ofo_entry_set_ref:
 */
void
ofo_entry_set_ref( ofoEntry *entry, const gchar *ref )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->private->ref );
		entry->private->ref = g_strdup( ref );
	}
}

/**
 * ofo_entry_set_account:
 */
void
ofo_entry_set_account( ofoEntry *entry, const gchar *account )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( account && g_utf8_strlen( account, -1 ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->private->account );
		entry->private->account = g_strdup( account );
	}
}

/**
 * ofo_entry_set_devise:
 */
void
ofo_entry_set_devise( ofoEntry *entry, const gchar *devise )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( devise > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->private->devise );
		entry->private->devise = g_strdup( devise );
	}
}

/**
 * ofo_entry_set_journal:
 */
void
ofo_entry_set_journal( ofoEntry *entry, gint journal )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( journal > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->private->jou_id = journal;
	}
}

/**
 * ofo_entry_set_amount:
 */
void
ofo_entry_set_amount( ofoEntry *entry, gdouble amount )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( amount > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->private->amount = amount;
	}
}

/**
 * ofo_entry_set_sens:
 */
void
ofo_entry_set_sens( ofoEntry *entry, ofaEntrySens sens )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( sens > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->private->sens = sens;
	}
}

/**
 * ofo_entry_set_status:
 */
void
ofo_entry_set_status( ofoEntry *entry, ofaEntryStatus status )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));
	g_return_if_fail( status > 0 );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		entry->private->status = status;
	}
}

/**
 * ofo_entry_set_maj_user:
 */
void
ofo_entry_set_maj_user( ofoEntry *entry, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		g_free( entry->private->maj_user );
		entry->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_entry_set_maj_stamp:
 */
void
ofo_entry_set_maj_stamp( ofoEntry *entry, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		memcpy( &entry->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_entry_set_rappro:
 *
 * The reconciliation may be unset by setting @drappro to %NULL.
 */
void
ofo_entry_set_rappro( ofoEntry *entry, const GDate *drappro )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		if( drappro && g_date_valid( drappro )){
			memcpy( &entry->private->rappro, drappro, sizeof( GDate ));
		} else {
			g_date_clear( &entry->private->rappro, 1 );
		}
	}
}

/**
 * ofo_entry_new_with_data:
 *
 * Create a new entry with the provided data.
 * The entry is - at this time - unnumbered and does not have sent any
 * advertising message. For the moment, this is only a 'project' of
 * entry...
 *
 * Returns: the #ofoEntry entry object, of %NULL in case of an error.
 */
ofoEntry *
ofo_entry_new_with_data( const ofoDossier *dossier,
							const GDate *effet, const GDate *ope, const gchar *label,
							const gchar *ref, const gchar *acc_number,
							const gchar *devise, gint jou_id, gdouble amount, ofaEntrySens sens )
{
	ofoEntry *entry;
	ofoAccount *account;

	if( jou_id <= 0 || !ofo_journal_get_by_id( dossier, jou_id )){
		error_journal( jou_id );
		return( NULL );
	}
	if( !devise || !g_utf8_strlen( devise, -1 ) || !ofo_devise_get_by_code( dossier, devise )){
		error_currency( devise );
		return( NULL );
	}
	if( !acc_number || !g_utf8_strlen( acc_number, -1 )){
		error_acc_number();
		return( NULL );
	}
	account = ofo_account_get_by_number( dossier, acc_number );
	if( !account ){
		error_account( acc_number );
		return( NULL );
	}
	if( g_utf8_collate( devise, ofo_account_get_devise( account ))){
		error_acc_currency( dossier, devise, account );
		return( NULL );
	}

	entry = g_object_new( OFO_TYPE_ENTRY, NULL );

	memcpy( &entry->private->effect, effet, sizeof( GDate ));
	memcpy( &entry->private->operation, ope, sizeof( GDate ));
	entry->private->label = g_strdup( label );
	entry->private->ref = g_strdup( ref );
	entry->private->account = g_strdup( acc_number );
	entry->private->devise = g_strdup( devise );
	entry->private->jou_id = jou_id;
	entry->private->amount = amount;
	entry->private->sens = sens;
	entry->private->status = ENT_STATUS_ROUGH;

	return( entry );
}

/**
 * ofo_entry_insert:
 *
 * Allocates a sequential number to the entry, and records it in the
 * sgbd. Send the corresponding advertising messages if no error occurs.
 */
gboolean
ofo_entry_insert( ofoEntry *entry, ofoDossier *dossier )
{
	gboolean ok;

	ok = FALSE;
	entry->private->number = ofo_dossier_get_next_entry_number( dossier );

	if( entry_do_insert( entry,
				ofo_dossier_get_sgbd( dossier ),
				ofo_dossier_get_user( dossier ))){

		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_NEW_OBJECT, g_object_ref( entry ));

		ok = TRUE;
	}

	return( ok );
}

static gboolean
entry_do_insert( ofoEntry *entry, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *ref;
	gchar *deff, *dope;
	gchar amount[1+G_ASCII_DTOSTR_BUF_SIZE];
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	label = my_utils_quote( ofo_entry_get_label( entry ));
	ref = my_utils_quote( ofo_entry_get_ref( entry ));
	deff = my_utils_sql_from_date( ofo_entry_get_deffect( entry ));
	dope = my_utils_sql_from_date( ofo_entry_get_dope( entry ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_ECRITURES " );

	g_string_append_printf( query,
			"	(ECR_DEFFET,ECR_NUMBER,ECR_DOPE,ECR_LABEL,ECR_REF,ECR_COMPTE,"
			"	ECR_DEV_CODE,ECR_JOU_ID,ECR_MONTANT,ECR_SENS,ECR_STATUS,"
			"	ECR_MAJ_USER, ECR_MAJ_STAMP) "
			"	VALUES ('%s',%d,'%s','%s',",
			deff,
			ofo_entry_get_number( entry ),
			dope,
			label );

	if( ref && g_utf8_strlen( ref, -1 )){
		g_string_append_printf( query, "'%s',", ref );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
				"'%s',"
				"'%s',%d,%s,%d,%d,"
				"'%s','%s')",
				ofo_entry_get_account( entry ),
				ofo_entry_get_devise( entry ),
				ofo_entry_get_journal( entry ),
				g_ascii_dtostr( amount, G_ASCII_DTOSTR_BUF_SIZE, ofo_entry_get_amount( entry )),
				ofo_entry_get_sens( entry ),
				ofo_entry_get_status( entry ),
				user,
				stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_entry_set_maj_user( entry, user );
		ofo_entry_set_maj_stamp( entry, my_utils_stamp_from_str( stamp ));

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( deff );
	g_free( dope );
	g_free( ref );
	g_free( label );
	g_free( stamp );

	return( ok );
}

static void
error_journal( gint jou_id )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid journal identifier: %d" ), jou_id );
	error_entry( str );

	g_free( str );
}

static void
error_currency( const gchar *devise )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid currency ISO 3A code: %s" ), devise );
	error_entry( str );

	g_free( str );
}

static void
error_acc_number( void )
{
	gchar *str;

	str = g_strdup( _( "Empty account number" ));
	error_entry( str );

	g_free( str );
}

static void
error_account( const gchar *number )
{
	gchar *str;

	str = g_strdup_printf( _( "Invalid account number: %s" ), number );
	error_entry( str );

	g_free( str );
}

static void
error_acc_currency( const ofoDossier *dossier, const gchar *devise, ofoAccount *account )
{
	gchar *str;
	const gchar *acc_dev_code;
	ofoDevise *acc_dev, *ent_dev;

	acc_dev_code = ofo_account_get_devise( account );
	acc_dev = ofo_devise_get_by_code( dossier, acc_dev_code );
	ent_dev = ofo_devise_get_by_code( dossier, devise );

	if( !acc_dev ){
		str = g_strdup_printf( "Invalid currency '%s' for the account '%s'",
					acc_dev_code, ofo_account_get_number( account ));
	} else if( !ent_dev ){
		str = g_strdup_printf( "Candidate entry makes use of invalid '%s' currency", devise );
	} else {
		str = g_strdup_printf( _( "Account %s is configured for accepting %s currency. "
				"But the candidate entry makes use of %s" ),
					ofo_account_get_number( account ),
					acc_dev_code,
					devise );
	}
	error_entry( str );

	g_free( str );
}

static void
error_entry( const gchar *message )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_CLOSE,
			"%s", message );

	gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );
}

/**
 * ofo_entry_update_rappro:
 */
gboolean
ofo_entry_update_rappro( ofoEntry *entry, const ofoDossier *dossier )
{
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( entry )->prot->dispose_has_run ){

		return( do_update_rappro( entry, ofo_dossier_get_sgbd( dossier )));
	}

	return( FALSE );
}

static gboolean
do_update_rappro( ofoEntry *entry, const ofoSgbd *sgbd )
{
	gchar *query, *srappro;
	const GDate *rappro;
	gboolean ok;

	rappro = ofo_entry_get_rappro( entry );
	if( g_date_valid( rappro )){

		srappro = my_utils_sql_from_date( rappro );
		query = g_strdup_printf(
					"UPDATE OFA_T_ECRITURES"
					"	SET ECR_RAPPRO='%s'"
					"	WHERE ECR_NUMBER=%d",
							srappro, ofo_entry_get_number( entry ));
		g_free( srappro );
	} else {
		query = g_strdup_printf(
					"UPDATE OFA_T_ECRITURES"
					"	SET ECR_RAPPRO=0"
					"	WHERE ECR_NUMBER=%d",
							ofo_entry_get_number( entry ));
	}

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

/**
 * ofo_entry_validate:
 */
gboolean
ofo_entry_validate( ofoEntry *entry, const ofoDossier *dossier )
{
	return( FALSE );
}

/**
 * ofo_entry_delete:
 */
gboolean
ofo_entry_delete( ofoEntry *entry, const ofoDossier *dossier )
{
	return( FALSE );
}

/*
 * as a first - bad - approach, we load all the entries in memory !!!
 *
 * the alternative should be to define here a callback, being called
 * from the exporter until the end...
 *
 * another alternative may be to use a cursor - but no doc here at the
 * moment :(
 */
GSList *
ofo_entry_get_csv( const ofoDossier *dossier )
{
	GSList *result, *irow, *icol;
	GSList *lines;
	ofoEntry *entry;
	gchar *str;
	gchar *sdope, *sdeffet, *sdrappro, *stamp;
	const gchar *sref, *muser;
	ofoJournal *journal;
	const GDate *date;

	result = ofo_sgbd_query_ex( ofo_dossier_get_sgbd( dossier ),
					"SELECT ECR_DOPE,ECR_DEFFET,ECR_NUMBER,ECR_LABEL,ECR_REF,"
					"	ECR_DEV_CODE,ECR_JOU_ID,ECR_COMPTE,ECR_MONTANT,ECR_SENS,"
					"	ECR_MAJ_USER,ECR_MAJ_STAMP,ECR_STATUS,ECR_RAPPRO "
					"	FROM OFA_T_ECRITURES "
					"	ORDER BY ECR_NUMBER ASC" );

	lines = NULL;

	str = g_strdup( "Dope;Deffect;Number;Label;Ref;Currency;Journal;Account;Amount;Sens;MajUser;MajStamp;Status;Drappro" );
	lines = g_slist_prepend( lines, str );

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		entry = ofo_entry_new();

		ofo_entry_set_dope( entry, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_deffect( entry, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_number( entry, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_label( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_ref( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_devise( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_journal( entry, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_account( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_amount( entry, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		icol = icol->next;
		ofo_entry_set_sens( entry, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_maj_user( entry, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_entry_set_maj_stamp( entry, my_utils_stamp_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_status( entry, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_entry_set_rappro( entry, my_utils_date_from_str(( gchar * ) icol->data ));

		sdope = my_utils_sql_from_date( ofo_entry_get_dope( entry ));
		sdeffet = my_utils_sql_from_date( ofo_entry_get_deffect( entry ));
		sref = ofo_entry_get_ref( entry );
		journal = ofo_journal_get_by_id( dossier, ofo_entry_get_journal( entry ));
		muser = ofo_entry_get_maj_user( entry );
		stamp = my_utils_str_from_stamp( ofo_entry_get_maj_stamp( entry ));

		date = ofo_entry_get_rappro( entry );
		if( date && g_date_valid( date )){
			sdrappro = my_utils_sql_from_date( date );
		} else {
			sdrappro = g_strdup( "" );
		}

		str = g_strdup_printf( "%s;%s;%d;%s;%s;%s;%s;%s;%.2lf;%d;%s;%s;%d;%s",
				sdope,
				sdeffet,
				ofo_entry_get_number( entry ),
				ofo_entry_get_label( entry ),
				sref ? sref : "",
				ofo_entry_get_devise( entry ),
				journal ? ofo_journal_get_mnemo( journal ) : "",
				ofo_entry_get_account( entry ),
				ofo_entry_get_amount( entry ),
				ofo_entry_get_sens( entry ),
				muser ? muser : "",
				muser ? stamp : "",
				ofo_entry_get_status( entry ),
				sdrappro );

		lines = g_slist_prepend( lines, str );

		g_object_unref( entry );
		g_free( sdrappro );
		g_free( sdeffet );
		g_free( sdope );
	}

	return( g_slist_reverse( lines ));
}
