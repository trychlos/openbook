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

/* private class data
 */
struct _ofoAccountClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
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
	gchar   *devise;
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

static ofoBaseClass *st_parent_class  = NULL;

static GType  register_type( void );
static void   class_init( ofoAccountClass *klass );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   instance_dispose( GObject *instance );
static void   instance_finalize( GObject *instance );

GType
ofo_account_get_type( void )
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
	static const gchar *thisfn = "ofo_account_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoAccountClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoAccount ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoAccount", &info, 0 );

	return( type );
}

static void
class_init( ofoAccountClass *klass )
{
	static const gchar *thisfn = "ofo_account_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoAccountClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofo_account_instance_init";
	ofoAccount *self;

	g_return_if_fail( OFO_IS_ACCOUNT( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFO_ACCOUNT( instance );

	self->private = g_new0( ofoAccountPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_instance_dispose";
	ofoAccountPrivate *priv;

	g_return_if_fail( OFO_IS_ACCOUNT( instance ));

	priv = ( OFO_ACCOUNT( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				priv->number, priv->label );

		priv->dispose_has_run = TRUE;

		g_free( priv->number );
		g_free( priv->label );
		g_free( priv->devise );
		g_free( priv->type );
		g_free( priv->notes );
		g_free( priv->maj_user );

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_instance_finalize";
	ofoAccountPrivate *priv;

	g_return_if_fail( OFO_IS_ACCOUNT( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_ACCOUNT( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
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
			"SELECT CPT_NUMBER,CPT_LABEL,CPT_DEV,CPT_NOTES,CPT_TYPE,"
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
		ofo_account_set_devise( account, ( gchar * ) icol->data );
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
		priv = OFO_ACCOUNT( ic->data )->private;
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

	if( !account->private->dispose_has_run ){

		number = g_strdup( account->private->number );
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

	if( !account->private->dispose_has_run ){

		number = account->private->number;
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

	if( !account->private->dispose_has_run ){

		label = account->private->label;
	}

	return( label );
}

/**
 * ofo_account_get_devise:
 */
const gchar *
ofo_account_get_devise( const ofoAccount *account )
{
	const gchar *devise = NULL;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !account->private->dispose_has_run ){

		devise = account->private->devise;
	}

	return( devise );
}

/**
 * ofo_account_get_notes:
 */
const gchar *
ofo_account_get_notes( const ofoAccount *account )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), NULL );

	if( !account->private->dispose_has_run ){

		notes = account->private->notes;
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

	if( !account->private->dispose_has_run ){

		type = account->private->type;
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

	if( !account->private->dispose_has_run ){

		mnt = account->private->deb_mnt;
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

	if( !account->private->dispose_has_run ){

		ecr = account->private->deb_ecr;
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

	if( !account->private->dispose_has_run ){

		date = ( const GDate * ) &account->private->deb_date;
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

	if( !account->private->dispose_has_run ){

		mnt = account->private->cre_mnt;
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

	if( !account->private->dispose_has_run ){

		ecr = account->private->cre_ecr;
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

	if( !account->private->dispose_has_run ){

		date = ( const GDate * ) &account->private->cre_date;
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

	if( !account->private->dispose_has_run ){

		mnt = account->private->bro_deb_mnt;
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

	if( !account->private->dispose_has_run ){

		ecr = account->private->bro_deb_ecr;
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

	if( !account->private->dispose_has_run ){

		date = ( const GDate * ) &account->private->bro_deb_date;
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

	if( !account->private->dispose_has_run ){

		mnt = account->private->bro_cre_mnt;
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

	if( !account->private->dispose_has_run ){

		ecr = account->private->bro_cre_ecr;
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

	if( !account->private->dispose_has_run ){

		date = ( const GDate * ) &account->private->bro_cre_date;
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

	if( !account->private->dispose_has_run ){

		if( account->private->type && !g_utf8_collate( account->private->type, "R" )){
			is_root = TRUE;
		}
	}

	return( is_root );
}

/**
 * ofo_account_is_valid_data:
 */
gboolean
ofo_account_is_valid_data( const gchar *number, const gchar *label, const gchar *devise, const gchar *type )
{
	gunichar code;
	gint value;
	gboolean is_root;

	/* is account number valid ? */
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
	if( !is_root && ( !devise || !g_utf8_strlen( devise, -1 ))){
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

	if( !account->private->dispose_has_run ){

		account->private->number = g_strdup( number );
	}
}

/**
 * ofo_account_set_label:
 */
void
ofo_account_set_label( ofoAccount *account, const gchar *label )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->label = g_strdup( label );
	}
}

/**
 * ofo_account_set_devise:
 */
void
ofo_account_set_devise( ofoAccount *account, const gchar *devise )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->devise = g_strdup( devise );
	}
}

/**
 * ofo_account_set_notes:
 */
void
ofo_account_set_notes( ofoAccount *account, const gchar *notes )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_account_set_type:
 */
void
ofo_account_set_type( ofoAccount *account, const gchar *type )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->type = g_strdup( type );
	}
}

/**
 * ofo_account_set_maj_user:
 */
void
ofo_account_set_maj_user( ofoAccount *account, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_account_set_maj_stamp:
 */
void
ofo_account_set_maj_stamp( ofoAccount *account, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		memcpy( &account->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_account_set_deb_mnt:
 */
void
ofo_account_set_deb_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->deb_mnt = mnt;
	}
}

/**
 * ofo_account_set_deb_ecr:
 */
void
ofo_account_set_deb_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->deb_ecr = num;
	}
}

/**
 * ofo_account_set_deb_date:
 */
void
ofo_account_set_deb_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		memcpy( &account->private->deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_set_cre_mnt:
 */
void
ofo_account_set_cre_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->cre_mnt = mnt;
	}
}

/**
 * ofo_account_set_cre_ecr:
 */
void
ofo_account_set_cre_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->deb_ecr = num;
	}
}

/**
 * ofo_account_set_cre_date:
 */
void
ofo_account_set_cre_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		memcpy( &account->private->deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_set_bro_deb_mnt:
 */
void
ofo_account_set_bro_deb_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->bro_deb_mnt = mnt;
	}
}

/**
 * ofo_account_set_bro_deb_ecr:
 */
void
ofo_account_set_bro_deb_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->bro_deb_ecr = num;
	}
}

/**
 * ofo_account_set_bro_deb_date:
 */
void
ofo_account_set_bro_deb_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		memcpy( &account->private->bro_deb_date, date, sizeof( GDate ));
	}
}

/**
 * ofo_account_set_bro_cre_mnt:
 */
void
ofo_account_set_bro_cre_mnt( ofoAccount *account, gdouble mnt )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->bro_cre_mnt = mnt;
	}
}

/**
 * ofo_account_set_bro_cre_ecr:
 */
void
ofo_account_set_bro_cre_ecr( ofoAccount *account, gint num )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		account->private->bro_deb_ecr = num;
	}
}

/**
 * ofo_account_set_bro_cre_date:
 */
void
ofo_account_set_bro_cre_date( ofoAccount *account, const GDate *date )
{
	g_return_if_fail( OFO_IS_ACCOUNT( account ));

	if( !account->private->dispose_has_run ){

		memcpy( &account->private->bro_deb_date, date, sizeof( GDate ));
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
			"	(CPT_NUMBER, CPT_LABEL, CPT_TYPE, CPT_NOTES, CPT_DEV, CPT_MAJ_USER, CPT_MAJ_STAMP)"
			"	VALUES ('%s','%s','%s',",
					ofo_account_get_number( account ),
					label,
					ofo_account_get_type_account( account ));

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s','%s')",
					ofo_account_get_devise( account ),
					user,
					stamp );

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

	g_string_append_printf( query,
			"	CPT_DEV='%s', "
			"	CPT_MAJ_USER='%s',CPT_MAJ_STAMP='%s'"
			"	WHERE CPT_NUMBER='%s'",
					ofo_account_get_devise( account ),
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
