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
#include "ui/ofo-account.h"

/* priv instance data
 */
struct _ofoAccountPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

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

static void
ofo_account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_finalize";
	ofoAccount *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_ACCOUNT( instance );

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
	static const gchar *thisfn = "ofo_account_dispose";
	ofoAccount *self;

	self = OFO_ACCOUNT( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				self->priv->number, self->priv->label );

		self->priv->dispose_has_run = TRUE;
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

	self->priv->dispose_has_run = FALSE;

	self->priv->devise = -1;
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
 * ofo_account_load_chart:
 *
 * Loads/reloads the ordered list of accounts
 */
GList *
ofo_account_load_chart( ofaSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_account_load_chart";
	GSList *result, *irow, *icol;
	ofoAccount *account;
	GList *chart;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofa_sgbd_query_ex( sgbd, NULL,
			"SELECT CPT_NUMBER,CPT_LABEL,CPT_DEV_ID,CPT_NOTES,CPT_TYPE,"
			"	CPT_MAJ_USER,CPT_MAJ_STAMP,"
			"	CPT_DEB_MNT,CPT_DEB_ECR,CPT_DEB_DATE,"
			"	CPT_CRE_MNT,CPT_CRE_ECR,CPT_CRE_DATE,"
			"	CPT_BRO_DEB_MNT,CPT_BRO_DEB_ECR,CPT_BRO_DEB_DATE,"
			"	CPT_BRO_CRE_MNT,CPT_BRO_CRE_ECR,CPT_BRO_CRE_DATE "
			"	FROM OFA_T_COMPTES "
			"	ORDER BY CPT_NUMBER ASC" );

	chart = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		account = ofo_account_new();
		ofo_account_set_number( account, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_account_set_label( account, ( gchar * ) icol->data );
		icol = icol->next;
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
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_deb_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		/*g_debug( "%s: deb_mnt str=%s mnt=%f obj=%f", thisfn, ( gchar * ) icol->data, g_ascii_strtod(( gchar * ) icol->data, NULL ), ofo_account_get_deb_mnt( account ));*/
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_deb_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_deb_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_cre_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_cre_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_cre_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_deb_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_deb_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_bro_deb_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_cre_mnt( account, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		if( icol->data ){
			ofo_account_set_bro_cre_ecr( account, atoi(( gchar * ) icol->data ));
		}
		icol = icol->next;
		ofo_account_set_bro_cre_date( account, my_utils_date_from_str(( gchar * ) icol->data ));
		chart = g_list_prepend( chart, account );
	}

	ofa_sgbd_free_result( result );

	return( g_list_reverse( chart ));
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
	gchar *number;
	gint class;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	class = 0;

	if( !account->priv->dispose_has_run ){

		number = g_strdup( account->priv->number );
		number[1] = '\0';
		class = atoi( number );
		g_free( number );
	}

	return( class );
}

/**
 * ofo_account_get_number:
 */
const gchar *
ofo_account_get_number( const ofoAccount *account )
{
	const gchar *number = NULL;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !account->priv->dispose_has_run ){

		number = account->priv->number;
	}

	return( number );
}

/**
 * ofo_account_get_label:
 */
const gchar *
ofo_account_get_label( const ofoAccount *account )
{
	const gchar *label = NULL;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !account->priv->dispose_has_run ){

		label = account->priv->label;
	}

	return( label );
}

/**
 * ofo_account_get_devise:
 */
gint
ofo_account_get_devise( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), -1 );

	if( !account->priv->dispose_has_run ){

		return( account->priv->devise );
	}

	return( -1 );
}

/**
 * ofo_account_get_notes:
 */
const gchar *
ofo_account_get_notes( const ofoAccount *account )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !account->priv->dispose_has_run ){

		notes = account->priv->notes;
	}

	return( notes );
}

/**
 * ofo_account_get_type_account:
 */
const gchar *
ofo_account_get_type_account( const ofoAccount *account )
{
	const gchar *type = NULL;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !account->priv->dispose_has_run ){

		type = account->priv->type;
	}

	return( type );
}

/**
 * ofo_account_get_deb_mnt:
 */
gdouble
ofo_account_get_deb_mnt( const ofoAccount *account )
{
	gdouble mnt = 0.0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !account->priv->dispose_has_run ){

		mnt = account->priv->deb_mnt;
	}

	return( mnt );
}

/**
 * ofo_account_get_deb_ecr:
 */
gint
ofo_account_get_deb_ecr( const ofoAccount *account )
{
	gint ecr = 0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !account->priv->dispose_has_run ){

		ecr = account->priv->deb_ecr;
	}

	return( ecr );
}

/**
 * ofo_account_get_deb_date:
 */
const GDate *
ofo_account_get_deb_date( const ofoAccount *account )
{
	const GDate *date;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	date = NULL;

	if( !account->priv->dispose_has_run ){

		date = ( const GDate * ) &account->priv->deb_date;
	}

	return( date );
}

/**
 * ofo_account_get_cre_mnt:
 */
gdouble
ofo_account_get_cre_mnt( const ofoAccount *account )
{
	gdouble mnt = 0.0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !account->priv->dispose_has_run ){

		mnt = account->priv->cre_mnt;
	}

	return( mnt );
}

/**
 * ofo_account_get_cre_ecr:
 */
gint
ofo_account_get_cre_ecr( const ofoAccount *account )
{
	gint ecr = 0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !account->priv->dispose_has_run ){

		ecr = account->priv->cre_ecr;
	}

	return( ecr );
}

/**
 * ofo_account_get_cre_date:
 */
const GDate *
ofo_account_get_cre_date( const ofoAccount *account )
{
	const GDate *date;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	date = NULL;

	if( !account->priv->dispose_has_run ){

		date = ( const GDate * ) &account->priv->cre_date;
	}

	return( date );
}

/**
 * ofo_account_get_bro_deb_mnt:
 */
gdouble
ofo_account_get_bro_deb_mnt( const ofoAccount *account )
{
	gdouble mnt = 0.0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !account->priv->dispose_has_run ){

		mnt = account->priv->bro_deb_mnt;
	}

	return( mnt );
}

/**
 * ofo_account_get_bro_deb_ecr:
 */
gint
ofo_account_get_bro_deb_ecr( const ofoAccount *account )
{
	gint ecr = 0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !account->priv->dispose_has_run ){

		ecr = account->priv->bro_deb_ecr;
	}

	return( ecr );
}

/**
 * ofo_account_get_bro_deb_date:
 */
const GDate *
ofo_account_get_bro_deb_date( const ofoAccount *account )
{
	const GDate *date;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	date = NULL;

	if( !account->priv->dispose_has_run ){

		date = ( const GDate * ) &account->priv->bro_deb_date;
	}

	return( date );
}

/**
 * ofo_account_get_bro_cre_mnt:
 */
gdouble
ofo_account_get_bro_cre_mnt( const ofoAccount *account )
{
	gdouble mnt = 0.0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0.0 );

	if( !account->priv->dispose_has_run ){

		mnt = account->priv->bro_cre_mnt;
	}

	return( mnt );
}

/**
 * ofo_account_get_bro_cre_ecr:
 */
gint
ofo_account_get_bro_cre_ecr( const ofoAccount *account )
{
	gint ecr = 0;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !account->priv->dispose_has_run ){

		ecr = account->priv->bro_cre_ecr;
	}

	return( ecr );
}

/**
 * ofo_account_get_bro_cre_date:
 */
const GDate *
ofo_account_get_bro_cre_date( const ofoAccount *account )
{
	const GDate *date;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	date = NULL;

	if( !account->priv->dispose_has_run ){

		date = ( const GDate * ) &account->priv->bro_cre_date;
	}

	return( date );
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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

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

	if( !account->priv->dispose_has_run ){

		memcpy( &account->priv->bro_deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_insert:
 *
 * we deal here with an update of publicly modifiable account properties
 * so it is not needed to check debit or credit agregats
 */
gboolean
ofo_account_insert( ofoAccount *account, ofaSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_COMPTES" );

	g_string_append_printf( query,
			"	(CPT_NUMBER, CPT_LABEL, CPT_TYPE, CPT_NOTES, CPT_DEV_ID, CPT_MAJ_USER, CPT_MAJ_STAMP)"
			"	VALUES ('%s','%s','%s',",
					ofo_account_get_number( account ),
					label,
					ofo_account_get_type_account( account ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "NULL," );
	} else {
		g_string_append_printf( query, "%d,", ofo_account_get_devise( account ));
	}

	g_string_append_printf( query, "'%s','%s')", user, stamp );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

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
ofo_account_update( ofoAccount *account, ofaSgbd *sgbd, const gchar *user, const gchar *prev_number )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	const gchar *new_number;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );
	g_return_val_if_fail( prev_number && g_utf8_strlen( prev_number, -1 ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	new_number = ofo_account_get_number( account );
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_COMPTES SET " );

	if( g_utf8_collate( new_number, prev_number )){
		g_string_append_printf( query, "CPT_NUMBER='%s',", new_number );
	}

	g_string_append_printf( query,
			"	CPT_LABEL='%s',CPT_TYPE='%s',",
					label,
					ofo_account_get_type_account( account ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "CPT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CPT_NOTES=NULL," );
	}

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "CPT_DEV_ID=NULL," );
	} else {
		g_string_append_printf( query, "CPT_DEV_ID=%d,", ofo_account_get_devise( account ));
	}

	g_string_append_printf( query,
			"	CPT_MAJ_USER='%s',CPT_MAJ_STAMP='%s'"
			"	WHERE CPT_NUMBER='%s'",
					user,
					stamp,
					prev_number );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

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
ofo_account_delete( ofoAccount *account, ofaSgbd *sgbd, const gchar *user )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_COMPTES"
			"	WHERE CPT_NUMBER='%s'",
					ofo_account_get_number( account ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
