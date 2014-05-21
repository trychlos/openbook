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

#include <string.h>

#include "ui/my-utils.h"
#include "ui/ofo-entry.h"

/* priv instance data
 */
struct _ofoEntryPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	GDate          effect;
	gint           number;
	GDate          operation;
	gchar         *label;
	gchar         *ref;
	gchar         *account;
	gint           dev_id;
	gint           jou_id;
	gdouble        amount;
	ofaEntrySens   sens;
	ofaEntryStatus status;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
};

G_DEFINE_TYPE( ofoEntry, ofo_entry, OFO_TYPE_BASE )

#define OFO_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_ENTRY, ofoEntryPrivate))

static gboolean ofo_entry_insert( ofoEntry *entry, ofoDossier *dossier );

static void
ofo_entry_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_finalize";
	ofoEntry *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_ENTRY( instance );

	g_free( self->priv->label );
	g_free( self->priv->ref );
	g_free( self->priv->maj_user );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->finalize( instance );
}

static void
ofo_entry_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_dispose";
	ofoEntry *self;

	self = OFO_ENTRY( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->dispose( instance );
}

static void
ofo_entry_init( ofoEntry *self )
{
	static const gchar *thisfn = "ofo_entry_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_ENTRY_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	self->priv->number = -1;
	self->priv->dev_id = -1;
	self->priv->jou_id = -1;
	g_date_clear( &self->priv->effect, 1 );
	g_date_clear( &self->priv->operation, 1 );
}

static void
ofo_entry_class_init( ofoEntryClass *klass )
{
	static const gchar *thisfn = "ofo_entry_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoEntryPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_entry_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_entry_finalize;
}

/**
 * ofo_entry_new:
 */
ofoEntry *
ofo_entry_new( ofoDossier *dossier,
					const GDate *effet, const GDate *ope, const gchar *label, const gchar *ref,
					const gchar *account,
					gint dev_id, gint jou_id, gdouble amount, ofaEntrySens sens )
{
	ofoEntry *entry;

	entry = g_object_new( OFO_TYPE_ENTRY, NULL );

	if( effet && g_date_valid( effet )){
		memcpy( &entry->priv->effect, effet, sizeof( GDate ));
	}
	entry->priv->number = 0;
	if( ope && g_date_valid( ope )){
		memcpy( &entry->priv->operation, ope, sizeof( GDate ));
	}
	entry->priv->label = g_strdup( label );
	entry->priv->ref = g_strdup( ref );
	entry->priv->account = g_strdup( account );
	entry->priv->dev_id = dev_id;
	entry->priv->jou_id = jou_id;
	entry->priv->amount = amount;
	entry->priv->sens = sens;
	entry->priv->status = ENT_STATUS_ROUGH;

	if( !ofo_entry_insert( entry, dossier )){
		g_clear_object( &entry );
	}

	return( entry );
}

/**
 * ofo_entry_get_number:
 */
gint
ofo_entry_get_number( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), -1 );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->number );
	}

	return( -1 );
}

/**
 * ofo_entry_get_label:
 */
const gchar *
ofo_entry_get_label( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_ENTRY( entry ), NULL );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->label );
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

	if( !entry->priv->dispose_has_run ){

		return(( const GDate * ) &entry->priv->effect );
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

	if( !entry->priv->dispose_has_run ){

		return(( const GDate * ) &entry->priv->operation );
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

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->ref );
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

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->account );
	}

	return( NULL );
}

/**
 * ofo_entry_get_devise:
 */
gint
ofo_entry_get_devise( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_TAUX( entry ), -1 );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->dev_id );
	}

	return( -1 );
}

/**
 * ofo_entry_get_journal:
 */
gint
ofo_entry_get_journal( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_TAUX( entry ), -1 );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->jou_id );
	}

	return( -1 );
}

/**
 * ofo_entry_get_amount:
 */
gdouble
ofo_entry_get_amount( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_TAUX( entry ), 0.0 );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->amount );
	}

	return( 0.0 );
}

/**
 * ofo_entry_get_sens:
 */
ofaEntrySens
ofo_entry_get_sens( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_TAUX( entry ), -1 );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->sens );
	}

	return( -1 );
}

/**
 * ofo_entry_get_status:
 */
ofaEntryStatus
ofo_entry_get_status( const ofoEntry *entry )
{
	g_return_val_if_fail( OFO_IS_TAUX( entry ), -1 );

	if( !entry->priv->dispose_has_run ){

		return( entry->priv->status );
	}

	return( -1 );
}

/**
 * ofo_entry_set_maj_user:
 */
void
ofo_entry_set_maj_user( ofoEntry *entry, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !entry->priv->dispose_has_run ){

		entry->priv->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_entry_set_maj_stamp:
 */
void
ofo_entry_set_maj_stamp( ofoEntry *entry, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_ENTRY( entry ));

	if( !entry->priv->dispose_has_run ){

		memcpy( &entry->priv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_entry_insert:
 */
static gboolean
ofo_entry_insert( ofoEntry *entry, ofoDossier *dossier )
{
	GString *query;
	gchar *label, *ref;
	gchar *deff, *dope;
	gchar amount[1+G_ASCII_DTOSTR_BUF_SIZE];
	gboolean ok;
	gchar *stamp;
	const gchar *user;

	g_return_val_if_fail( OFO_IS_ENTRY( entry ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	/* first, get the next entry number available */
	entry->priv->number = ofo_dossier_get_next_entry_number( dossier );

	label = my_utils_quote( ofo_entry_get_label( entry ));
	ref = my_utils_quote( ofo_entry_get_ref( entry ));
	deff = my_utils_sql_from_date( ofo_entry_get_deffect( entry ));
	dope = my_utils_sql_from_date( ofo_entry_get_dope( entry ));
	user = ofo_dossier_get_user( dossier );
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_ECRITURES " );

	g_string_append_printf( query,
			"	(ECR_EFFET,ECR_NUMBER,ECR_OPE,ECR_LABEL,ECR_REF,ECR_COMPTE,"
			"	ECR_DEV_ID,ECR_JOU_ID,ECR_MONTANT,ECR_SENS,ECR_STATUS,"
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
				"%d,%d,%s,%d,%d,"
				"'%s','%s')",
				ofo_entry_get_account( entry ),
				ofo_entry_get_devise( entry ),
				ofo_entry_get_journal( entry ),
				g_ascii_dtostr( amount, G_ASCII_DTOSTR_BUF_SIZE, ofo_entry_get_amount( entry )),
				ofo_entry_get_sens( entry ),
				ofo_entry_get_status( entry ),
				user,
				stamp );

	if( ofa_sgbd_query( ofo_dossier_get_sgbd( dossier ), NULL, query->str )){

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

/**
 * ofo_entry_validate:
 */
gboolean
ofo_entry_validate( ofoEntry *entry, ofoDossier *dossier )
{
	return( FALSE );
}

/**
 * ofo_entry_delete:
 */
gboolean
ofo_entry_delete( ofoEntry *entry, ofoDossier *dossier )
{
	return( FALSE );
}
