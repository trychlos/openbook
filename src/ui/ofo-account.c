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
#include "ui/ofo-account.h"

/* priv instance data
 */
struct _ofoAccountPrivate {

	/* sgbd data
	 */
	gchar   *number;
	gchar   *label;
	gint     devise;
	gchar   *notes;
	gchar   *type;
	gchar   *maj_user;
	GTimeVal maj_stamp;
	gdouble  deb_mnt;
	gint     deb_ecr;
	GDate    deb_date;
	gdouble  cre_mnt;
	gint     cre_ecr;
	GDate    cre_date;
	gdouble  bro_deb_mnt;
	gint     bro_deb_ecr;
	GDate    bro_deb_date;
	gdouble  bro_cre_mnt;
	gint     bro_cre_ecr;
	GDate    bro_cre_date;
};

G_DEFINE_TYPE( ofoAccount, ofo_account, OFO_TYPE_BASE )

#define OFO_ACCOUNT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_ACCOUNT, ofoAccountPrivate))

OFO_BASE_DEFINE_GLOBAL( st_global, account )

static GList      *account_load_dataset( void );
static ofoAccount *account_find_by_number( GList *set, const gchar *number );
static gint        account_count_for_devise( ofoSgbd *sgbd, gint dev_id );
static gboolean    account_do_insert( ofoAccount *account, ofoSgbd *sgbd, const gchar *user );
static gboolean    account_do_update( ofoAccount *account, ofoSgbd *sgbd, const gchar *user, const gchar *prev_number );
static gboolean    account_do_delete( ofoAccount *account, ofoSgbd *sgbd );
static gint        account_cmp_by_number( const ofoAccount *a, const gchar *number );
static gint        account_cmp_by_ptr( const ofoAccount *a, const ofoAccount *b );

static void
ofo_account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_finalize";
	ofoAccount *self = OFO_ACCOUNT( instance );

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			self->priv->number, self->priv->label );

	g_free( self->priv->number );
	g_free( self->priv->label );
	g_free( self->priv->type );
	g_free( self->priv->notes );
	g_free( self->priv->maj_user );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_account_parent_class )->finalize( instance );
}

static void
ofo_account_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_ACCOUNT( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref member objects here */
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_account_parent_class )->dispose( instance );
}

static void
ofo_account_init( ofoAccount *self )
{
	static const gchar *thisfn = "ofo_account_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_ACCOUNT_GET_PRIVATE( self );

	self->priv->devise = OFO_BASE_UNSET_ID;
}

static void
ofo_account_class_init( ofoAccountClass *klass )
{
	static const gchar *thisfn = "ofo_account_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoAccountPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_account_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_account_finalize;
}

/**
 * ofo_account_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoAccount accounts, ordered by ascending
 * number. The returned list is owned by the #ofoAccount class, and
 * should not be freed by the caller.
 */
GList *
ofo_account_get_dataset( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	return( st_global->dataset );
}

/**
 * ofo_account_load_chart:
 *
 * Loads/reloads the ordered list of accounts
 */
static GList *
account_load_dataset( void )
{
	ofoSgbd *sgbd;
	GSList *result, *irow, *icol;
	ofoAccount *account;
	GList *dataset;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT CPT_NUMBER,CPT_LABEL,CPT_DEV_ID,CPT_NOTES,CPT_TYPE,"
			"	CPT_MAJ_USER,CPT_MAJ_STAMP,"
			"	CPT_DEB_ECR,CPT_DEB_DATE,CPT_DEB_MNT,"
			"	CPT_CRE_ECR,CPT_CRE_DATE,CPT_CRE_MNT,"
			"	CPT_BRO_DEB_ECR,CPT_BRO_DEB_DATE,CPT_BRO_DEB_MNT,"
			"	CPT_BRO_CRE_ECR,CPT_BRO_CRE_DATE,CPT_BRO_CRE_MNT "
			"	FROM OFA_T_COMPTES "
			"	ORDER BY CPT_NUMBER ASC" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		account = ofo_account_new();
		ofo_account_set_number( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_label( account, ( gchar * ) icol->data );
		icol = icol->next;
		/* devise may be left unset for root accounts, though is
		 * mandatory for detail ones */
		if( icol->data ){
			ofo_account_set_devise( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_notes( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_type( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_maj_user( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_maj_stamp( account, my_utils_stamp_from_str(( gchar * ) icol->data ));
		/*g_debug( "%s: deb_mnt str=%s mnt=%f obj=%f", thisfn, ( gchar * ) icol->data, g_ascii_strtod(( gchar * ) icol->data, NULL ), ofo_account_get_deb_mnt( account ));*/
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_deb_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_deb_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_deb_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_cre_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_cre_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_cre_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_deb_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_bro_deb_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_deb_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_cre_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_bro_cre_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_cre_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}

		g_debug( "ofo_account_load_dataset: %s - %s", account->priv->number, account->priv->label );
		dataset = g_list_prepend( dataset, account );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_account_get_by_number:
 *
 * Returns: the searched account, or %NULL.
 *
 * The returned object is owned by the #ofoAccount class, and should
 * not be unreffed by the caller.
 */
ofoAccount *
ofo_account_get_by_number( ofoDossier *dossier, const gchar *number )
{
	static const gchar *thisfn = "ofo_account_get_by_number";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( number && g_utf8_strlen( number, -1 ), NULL );

	g_debug( "%s: dossier=%p, number=%s", thisfn, ( void * ) dossier, number );

	OFO_BASE_SET_GLOBAL( st_global, dossier, account );

	return( account_find_by_number( st_global->dataset, number ));
}

static ofoAccount *
account_find_by_number( GList *set, const gchar *number )
{
	GList *found;

	found = g_list_find_custom(
				set, number, ( GCompareFunc ) account_cmp_by_number );
	if( found ){
		return( OFO_ACCOUNT( found->data ));
	}

	return( NULL );
}

/**
 * ofo_account_use_devise:
 *
 * Returns: %TRUE if a recorded account makes use of the specified currency.
 */
gboolean
ofo_account_use_devise( ofoDossier *dossier, gint dev_id )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( account_count_for_devise( ofo_dossier_get_sgbd( dossier ), dev_id ) > 0 );
}

static gint
account_count_for_devise( ofoSgbd *sgbd, gint dev_id )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	count = 0;
	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_COMPTES "
				"	WHERE CPT_DEV_ID=%d",
					dev_id );

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
 * ofo_account_new:
 */
ofoAccount *
ofo_account_new( void )
{
	ofoAccount *account;

	account = g_object_new( OFO_TYPE_ACCOUNT, NULL );

	return( account );
}

/**
 * ofo_account_dump_chart:
 */
void
ofo_account_dump_chart( GList *chart )
{
	static const gchar *thisfn = "ofo_account_dump_chart";
	ofoAccountPrivate *priv;
	GList *ic;

	for( ic=chart ; ic ; ic=ic->next ){
		priv = OFO_ACCOUNT( ic->data )->priv;
		g_debug( "%s: account %s - %s", thisfn, priv->number, priv->label );
	}
}

/**
 * ofo_account_get_class:
 */
gint
ofo_account_get_class( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( ofo_account_get_class_from_number( account->priv->number ));
	}

	return( 0 );
}

/**
 * ofo_account_get_class_from_number:
 *
 * TODO: make this UTF8-compliant....
 */
gint
ofo_account_get_class_from_number( const gchar *account_number )
{
	gchar *number;
	gint class;

	number = g_strdup( account_number );
	number[1] = '\0';
	class = atoi( number );
	g_free( number );

	return( class );
}

/**
 * ofo_account_get_number:
 */
const gchar *
ofo_account_get_number( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->number );
	}

	return( NULL );
}

/**
 * ofo_account_get_label:
 */
const gchar *
ofo_account_get_label( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->label );
	}

	return( NULL );
}

/**
 * ofo_account_get_devise:
 */
gint
ofo_account_get_devise( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->devise );
	}

	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_account_get_notes:
 */
const gchar *
ofo_account_get_notes( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->notes );
	}

	return( NULL );
}

/**
 * ofo_account_get_type_account:
 */
const gchar *
ofo_account_get_type_account( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->type );
	}

	return( NULL );
}

/**
 * ofo_account_get_maj_user:
 */
const gchar *
ofo_account_get_maj_user( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const gchar * ) account->priv->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_account_get_maj_stamp:
 */
const GTimeVal *
ofo_account_get_maj_stamp( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &account->priv->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_account_get_deb_mnt:
 */
gdouble
ofo_account_get_deb_mnt( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->deb_mnt );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_deb_ecr:
 */
gint
ofo_account_get_deb_ecr( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->deb_ecr );
	}

	return( 0 );
}

/**
 * ofo_account_get_deb_date:
 */
const GDate *
ofo_account_get_deb_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->deb_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_cre_mnt:
 */
gdouble
ofo_account_get_cre_mnt( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->cre_mnt );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_cre_ecr:
 */
gint
ofo_account_get_cre_ecr( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->cre_ecr );
	}

	return( 0 );
}

/**
 * ofo_account_get_cre_date:
 */
const GDate *
ofo_account_get_cre_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->cre_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_bro_deb_mnt:
 */
gdouble
ofo_account_get_bro_deb_mnt( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->bro_deb_mnt );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_bro_deb_ecr:
 */
gint
ofo_account_get_bro_deb_ecr( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->bro_deb_ecr );
	}

	return( 0 );
}

/**
 * ofo_account_get_bro_deb_date:
 */
const GDate *
ofo_account_get_bro_deb_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->bro_deb_date );
	}

	return( NULL );
}

/**
 * ofo_account_get_bro_cre_mnt:
 */
gdouble
ofo_account_get_bro_cre_mnt( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->bro_cre_mnt );
	}

	return( 0.0 );
}

/**
 * ofo_account_get_bro_cre_ecr:
 */
gint
ofo_account_get_bro_cre_ecr( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( account->priv->bro_cre_ecr );
	}

	return( 0 );
}

/**
 * ofo_account_get_bro_cre_date:
 */
const GDate *
ofo_account_get_bro_cre_date( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return(( const GDate * ) &account->priv->bro_cre_date );
	}

	return( NULL );
}

/**
 * ofo_account_is_deletable:
 *
 * A account is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its amounts must be
 * nuls
 */
gboolean
ofo_account_is_deletable( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return (!ofo_account_get_deb_mnt( account ) &&
				!ofo_account_get_cre_mnt( account ) &&
				!ofo_account_get_bro_deb_mnt( account ) &&
				!ofo_account_get_bro_cre_mnt( account ));
	}

	return( FALSE );
}

/**
 * ofo_account_is_root:
 */
gboolean
ofo_account_is_root( const ofoAccount *account )
{
	gboolean is_root;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	is_root = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		if( account->priv->type && !g_utf8_collate( account->priv->type, "R" )){
			is_root = TRUE;
		}
	}

	return( is_root );
}

/**
 * ofo_account_is_valid_data:
 */
gboolean
ofo_account_is_valid_data( const gchar *number, const gchar *label, gint devise, const gchar *type )
{
	gunichar code;
	gint value;
	gboolean is_root;

	/* is account number valid ?
	 * must begin with a digit, and be at least two chars
	 */
	if( !number || g_utf8_strlen( number, -1 ) < 2 ){
		return( FALSE );
	}
	code = g_utf8_get_char( number );
	value = g_unichar_digit_value( code );
	/*g_debug( "ofo_account_is_valid_data: number=%s, code=%c, value=%d", number, code, value );*/
	if( value < 1 ){
		return( FALSE );
	}

	/* label */
	if( !label || !g_utf8_strlen( label, -1 )){
		return( FALSE );
	}

	/* is root account ? */
	is_root = ( type && !g_utf8_collate( type, "R" ));

	/* devise must be set for detail account */
	if( !is_root && devise <= 0 ){
		return( FALSE );
	}

	return( TRUE );
}

/**
 * ofo_account_set_number:
 */
void
ofo_account_set_number( ofoAccount *account, const gchar *number )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->number );
		account->priv->number = g_strdup( number );
	}
}

/**
 * ofo_account_set_label:
 */
void
ofo_account_set_label( ofoAccount *account, const gchar *label )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->label );
		account->priv->label = g_strdup( label );
	}
}

/**
 * ofo_account_set_devise:
 */
void
ofo_account_set_devise( ofoAccount *account, gint devise )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->devise = devise;
	}
}

/**
 * ofo_account_set_notes:
 */
void
ofo_account_set_notes( ofoAccount *account, const gchar *notes )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->notes );
		account->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_account_set_type:
 */
void
ofo_account_set_type( ofoAccount *account, const gchar *type )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->type );
		account->priv->type = g_strdup( type );
	}
}

/**
 * ofo_account_set_maj_user:
 */
void
ofo_account_set_maj_user( ofoAccount *account, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_free( account->priv->maj_user );
		account->priv->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_account_set_maj_stamp:
 */
void
ofo_account_set_maj_stamp( ofoAccount *account, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		memcpy( &account->priv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_account_set_deb_mnt:
 */
void
ofo_account_set_deb_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->deb_mnt = mnt;
	}
}

/**
 * ofo_account_set_deb_ecr:
 */
void
ofo_account_set_deb_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->deb_ecr = num;
	}
}

/**
 * ofo_account_set_deb_date:
 */
void
ofo_account_set_deb_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		memcpy( &account->priv->deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_set_cre_mnt:
 */
void
ofo_account_set_cre_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->cre_mnt = mnt;
	}
}

/**
 * ofo_account_set_cre_ecr:
 */
void
ofo_account_set_cre_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->deb_ecr = num;
	}
}

/**
 * ofo_account_set_cre_date:
 */
void
ofo_account_set_cre_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		memcpy( &account->priv->deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_set_bro_deb_mnt:
 */
void
ofo_account_set_bro_deb_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->bro_deb_mnt = mnt;
	}
}

/**
 * ofo_account_set_bro_deb_ecr:
 */
void
ofo_account_set_bro_deb_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->bro_deb_ecr = num;
	}
}

/**
 * ofo_account_set_bro_deb_date:
 */
void
ofo_account_set_bro_deb_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		memcpy( &account->priv->bro_deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_set_bro_cre_mnt:
 */
void
ofo_account_set_bro_cre_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->bro_cre_mnt = mnt;
	}
}

/**
 * ofo_account_set_bro_cre_ecr:
 */
void
ofo_account_set_bro_cre_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account->priv->bro_deb_ecr = num;
	}
}

/**
 * ofo_account_set_bro_cre_date:
 */
void
ofo_account_set_bro_cre_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		memcpy( &account->priv->bro_deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_insert:
 * @account: the new #ofoAccount account to be inserted.
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * This function is only of use when the user modifies the public
 * properties of an account. It is no worth to deal here with amounts
 * and/or debit/credit agregates.
 *
 * Returns: %TRUE if the insertion has been successful.
 */
gboolean
ofo_account_insert( ofoAccount *account, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_insert";

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, dossier=%p",
				thisfn, ( void * ) account, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, account );

		if( account_do_insert(
					account,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_ADD_TO_DATASET( st_global, account );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_insert( ofoAccount *account, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	ok = FALSE;

	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_COMPTES" );

	g_string_append_printf( query,
			"	(CPT_NUMBER,CPT_LABEL,CPT_DEV_ID,CPT_NOTES,CPT_TYPE,"
			"	CPT_MAJ_USER, CPT_MAJ_STAMP)"
			"	VALUES ('%s','%s',",
					ofo_account_get_number( account ),
					label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "NULL," );
	} else {
		g_string_append_printf( query, "%d,", ofo_account_get_devise( account ));
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s','%s')",
			ofo_account_get_type_account( account ),
			user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_account_set_maj_user( account, user );
		ofo_account_set_maj_stamp( account, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_account_update:
 *
 * we deal here with an update of publicly modifiable account properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_account_update( ofoAccount *account, ofoDossier *dossier, const gchar *prev_number )
{
	static const gchar *thisfn = "ofo_account_update";

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( prev_number && g_utf8_strlen( prev_number, -1 ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, dossier=%p, prev_number=%s",
				thisfn, ( void * ) account, ( void * ) dossier, prev_number );

		OFO_BASE_SET_GLOBAL( st_global, dossier, account );

		if( account_do_update(
					account,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ),
					prev_number )){

			OFO_BASE_UPDATE_DATASET( st_global, account );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_update( ofoAccount *account, ofoSgbd *sgbd, const gchar *user, const gchar *prev_number )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	const gchar *new_number;
	gchar *stamp;

	ok = FALSE;

	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	new_number = ofo_account_get_number( account );
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_COMPTES SET " );

	if( g_utf8_collate( new_number, prev_number )){
		g_string_append_printf( query, "CPT_NUMBER='%s',", new_number );
	}

	g_string_append_printf( query, "CPT_LABEL='%s',", label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "CPT_DEV_ID=NULL," );
	} else {
		g_string_append_printf( query, "CPT_DEV_ID=%d,", ofo_account_get_devise( account ));
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "CPT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CPT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	CPT_TYPE='%s',",
					ofo_account_get_type_account( account ));

	g_string_append_printf( query,
			"	CPT_MAJ_USER='%s',CPT_MAJ_STAMP='%s'"
			"	WHERE CPT_NUMBER='%s'",
					user,
					stamp,
					prev_number );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_account_set_maj_user( account, user );
		ofo_account_set_maj_stamp( account, my_utils_stamp_from_str( stamp ));

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

/**
 * ofo_account_delete:
 */
gboolean
ofo_account_delete( ofoAccount *account, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_delete";

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_account_is_deletable( account ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, dossier=%p",
				thisfn, ( void * ) account, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, account );

		if( account_do_delete(
					account,
					ofo_dossier_get_sgbd( dossier ))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, account );
			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_delete( ofoAccount *account, ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_COMPTES"
			"	WHERE CPT_NUMBER='%s'",
					ofo_account_get_number( account ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
account_cmp_by_number( const ofoAccount *a, const gchar *number )
{
	return( g_utf8_collate( ofo_account_get_number( a ), number ));
}

static gint
account_cmp_by_ptr( const ofoAccount *a, const ofoAccount *b )
{
	return( g_utf8_collate( ofo_account_get_number( a ), ofo_account_get_number( b )));
}
