/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
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

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofa-irecover.h"
#include "api/ofo-account.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "ebp-recovery/ofa-ebp-recover.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* from interface
	 */
	ofaIGetter      *getter;
	ofaStreamFormat *format;
	ofaIDBConnect   *connect;
	ofaMsgCb         msg_cb;
	void            *msg_data;
}
	ofaEbpRecoverPrivate;

static void     iident_iface_init( myIIdentInterface *iface );
static gchar   *iident_get_canon_name( const myIIdent *instance, void *msg_data );
static gchar   *iident_get_version( const myIIdent *instance, void *msg_data );
static void     irecover_iface_init( ofaIRecoverInterface *iface );
static guint    irecover_get_interface_version( void );
static gboolean irecover_import_uris( ofaIRecover *instance, ofaIGetter *getter, GList *uris, ofaStreamFormat *format, ofaIDBConnect *connect, ofaMsgCb msg_cb, void *msg_data );
static gboolean import_entries( ofaEbpRecover *self, GSList *slines );
static gboolean import_accounts( ofaEbpRecover *self, GSList *slines );
static gboolean create_account( ofaEbpRecover *self, const gchar *account );
static gboolean create_ledger( ofaEbpRecover *self, const gchar *ledger );

G_DEFINE_TYPE_EXTENDED( ofaEbpRecover, ofa_ebp_recover, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaEbpRecover )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IIDENT, iident_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IRECOVER, irecover_iface_init ))

static void
ebp_recover_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ebp_recover_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_EBP_RECOVER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ebp_recover_parent_class )->finalize( instance );
}

static void
ebp_recover_dispose( GObject *instance )
{
	ofaEbpRecoverPrivate *priv;

	g_return_if_fail( instance && OFA_IS_EBP_RECOVER( instance ));

	priv = ofa_ebp_recover_get_instance_private( OFA_EBP_RECOVER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ebp_recover_parent_class )->dispose( instance );
}

static void
ofa_ebp_recover_init( ofaEbpRecover *self )
{
	static const gchar *thisfn = "ofa_ebp_recover_init";
	ofaEbpRecoverPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_EBP_RECOVER( self ));

	priv = ofa_ebp_recover_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_ebp_recover_class_init( ofaEbpRecoverClass *klass )
{
	static const gchar *thisfn = "ofa_ebp_recover_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ebp_recover_dispose;
	G_OBJECT_CLASS( klass )->finalize = ebp_recover_finalize;
}

/*
 * myIIdent interface management
 */
static void
iident_iface_init( myIIdentInterface *iface )
{
	static const gchar *thisfn = "ofa_ebp_recover_iident_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_canon_name = iident_get_canon_name;
	iface->get_version = iident_get_version;
}

static gchar *
iident_get_canon_name( const myIIdent *instance, void *msg_data )
{
	return( g_strdup( "EBP Recovery Instance" ));
}

static gchar *
iident_get_version( const myIIdent *instance, void *msg_data )
{
	return( g_strdup( "2017.1-i" ));
}

/*
 * #ofaIRecover interface management
 */
static void
irecover_iface_init( ofaIRecoverInterface *iface )
{
	static const gchar *thisfn = "ofa_ebp_recover_irecover_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = irecover_get_interface_version;
	iface->import_uris = irecover_import_uris;
}

/*
 * the version of the #ofaIRecover interface implemented by the module
 */
static guint
irecover_get_interface_version( void )
{
	return( 1 );
}


static gboolean
irecover_import_uris( ofaIRecover *instance, ofaIGetter *getter, GList *uris, ofaStreamFormat *format, ofaIDBConnect *connect, ofaMsgCb msg_cb, void *msg_data )
{
	static const gchar *thisfn = "ofa_ebp_recover_irecover_import_uris";
	ofaEbpRecoverPrivate *priv;
	GList *it;
	ofsRecoverFile *sfile;
	gboolean ok;
	GSList *slines, *head;
	gchar *msgerr;
	guint errors, count, i;

	priv = ofa_ebp_recover_get_instance_private( OFA_EBP_RECOVER( instance ));

	priv->getter = getter;
	priv->format = format;
	priv->connect = connect;
	priv->msg_cb = msg_cb;
	priv->msg_data = msg_data;
	ok = FALSE;

	for( it=uris ; it ; it=it->next ){
		sfile = ( ofsRecoverFile * ) it->data;
		slines = my_utils_uri_get_lines( sfile->uri, ofa_stream_format_get_charmap( format ), &errors, &msgerr );

		if( errors ){
			msg_cb( msgerr, msg_data );
			g_free( msgerr );

		} else {
			count = ofa_stream_format_get_headers_count( format );
			head = slines;
			for( i=0 ; i<count ; ++i ){
				head = g_slist_next( head );
			}
			switch( sfile->nature ){
				case OFA_RECOVER_ENTRY:
					ok = import_entries( OFA_EBP_RECOVER( instance ), head );
					break;
				case OFA_RECOVER_ACCOUNT:
					ok = import_accounts( OFA_EBP_RECOVER( instance ), head );
					break;
				default:
					msgerr = g_strdup_printf( _( "%s: unknown or invalid file nature=%u" ), thisfn, sfile->nature );
					msg_cb( msgerr, msg_data );
					g_free( msgerr );
			}

			g_slist_free_full( slines, ( GDestroyNotify ) g_free );
		}
	}

	return( ok );
}

static gboolean
import_entries( ofaEbpRecover *self, GSList *slines )
{
	static const gchar *thisfn = "ofa_ebp_recover_import_entries";
	ofaEbpRecoverPrivate *priv;
	GSList *it;
	gchar **fields, **itf;
	ofoEntry *entry;
	gchar *fieldsep, *label, *account, *currency, *ledger, *ref;
	GDate deffect, dope, dreconcil;
	ofxAmount debit, credit;
	gboolean settled;
	gulong numecr, count, errors;

	priv = ofa_ebp_recover_get_instance_private( self );

	count = 0;
	errors = 0;
	fieldsep = g_strdup_printf( "%c", ofa_stream_format_get_field_sep( priv->format ));

	for( it=slines ; it ; it=it->next ){
		fields = g_strsplit(( const gchar * ) it->data, fieldsep, -1 );
		count += 1;
		/* numecr */
		itf = fields;
		numecr = atoi( *itf );
		/* journal */
		itf += 1;
		ledger = g_strdup( *itf );
		create_ledger( self, ledger );
		/* compte */
		itf += 1;
		account = g_strdup( *itf );
		create_account( self, account );
		/* date ecr = operation date */
		itf += 1;
		my_date_set_from_str( &dope, *itf, ofa_stream_format_get_date_format( priv->format ));
		/* jour */
		itf += 1;
		/* indexp */
		itf += 1;
		/* indexs */
		itf += 1;
		/* datval = reconciliation date */
		itf += 1;
		my_date_set_from_str( &dreconcil, *itf, ofa_stream_format_get_date_format( priv->format ));
		/* datsai */
		itf += 1;
		/* datech = effect date */
		itf += 1;
		my_date_set_from_str( &deffect, *itf, ofa_stream_format_get_date_format( priv->format ));
		/* poste */
		itf += 1;
		/* piece */
		itf += 1;
		ref = g_strdup( *itf );
		/* numdoc */
		itf += 1;
		/* libelle */
		itf += 1;
		label = g_strdup( *itf );
		/* debit */
		itf += 1;
		debit = my_double_set_from_str( *itf,
						ofa_stream_format_get_thousand_sep( priv->format ),
						ofa_stream_format_get_decimal_sep( priv->format ));
		/* credit */
		itf += 1;
		credit = my_double_set_from_str( *itf,
						ofa_stream_format_get_thousand_sep( priv->format ),
						ofa_stream_format_get_decimal_sep( priv->format ));
		/* solde */
		itf += 1;
		/* devise */
		itf += 1;
		currency = g_strdup( my_strlen( *itf ) ? *itf : "EUR" );
		/* taux devise */
		itf += 1;
		/* devdebit */
		itf += 1;
		/* devcredit */
		itf += 1;
		/* cdebit */
		itf += 1;
		/* ccdredit */
		itf += 1;
		/* lettre */
		itf += 1;
		/* rapp */
		itf += 1;
		/* numrap */
		itf += 1;
		/* typetva */
		itf += 1;
		/* brap */
		itf += 1;
		/* blettre */
		itf += 1;
		settled = my_collate( *itf, "oui" );
		/* bran */
		itf += 1;
		/* bsimu */
		itf += 1;
		/* bech */
		itf += 1;
		/* bana */
		itf += 1;
		/* banaex */
		itf += 1;
		/* bacompte */
		itf += 1;
		/* bfact */
		itf += 1;
		/* bavoir */
		itf += 1;
		/* bregl */
		itf += 1;
		/* bequi */
		itf += 1;
		/* brapnm1 */
		itf += 1;
		/* bcontrepartie */
		itf += 1;
		/* bmemo */
		itf += 1;
		/* abonne */
		itf += 1;
		/* codreg */
		itf += 1;
		/* cheque */
		itf += 1;
		/* cpttva */
		itf += 1;
		/* moistva */
		itf += 1;
		/* bencontrevaleur */
		itf += 1;
		/* bendevise */
		itf += 1;
		/* becartconv */
		itf += 1;
		g_strfreev( fields );

		entry = ofo_entry_new( priv->getter );
		ofo_entry_set_label( entry, label );
		ofo_entry_set_deffect( entry, &deffect );
		ofo_entry_set_dope( entry, &dope );
		ofo_entry_set_ref( entry, ref );
		ofo_entry_set_account( entry, account );
		ofo_entry_set_currency( entry, currency );
		ofo_entry_set_ledger( entry, ledger );
		ofo_entry_set_debit( entry, debit );
		ofo_entry_set_credit( entry, credit );
		g_debug( "%s: numecr=%lu, settled=%s", thisfn, numecr, settled ? "True":"False" );
		/* TODO: insert entry in a non-opened dossier... */
		g_object_unref( entry );

		g_free( label );
		g_free( account );
		g_free( currency );
		g_free( ledger );
		g_free( ref );
	}

	g_free( fieldsep );

	return( errors == 0 );
}

static gboolean
import_accounts( ofaEbpRecover *self, GSList *slines )
{
	return( TRUE );
}

static gboolean
create_account( ofaEbpRecover *self, const gchar *account )
{
	return( TRUE );
}

static gboolean
create_ledger( ofaEbpRecover *self, const gchar *ledger )
{
	return( TRUE );
}
