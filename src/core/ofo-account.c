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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-dbms.h"
#include "api/ofa-file-format.h"
#include "api/ofa-idataset.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-class.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/ofa-preferences.h"

/* priv instance data
 */
enum {
	ACC_NUMBER = 1,
	ACC_LABEL,
	ACC_CURRENCY,
	ACC_TYPE,
	ACC_SETTLEABLE,
	ACC_RECONCILIABLE,
	ACC_FORWARD,
	ACC_NOTES,
	ACC_UPD_USER,
	ACC_UPD_STAMP,
	ACC_VAL_DEBIT,
	ACC_VAL_CREDIT,
	ACC_ROUGH_DEBIT,
	ACC_ROUGH_CREDIT,
	ACC_OPEN_DEBIT,
	ACC_OPEN_CREDIT,
	ACC_FUT_DEBIT,
	ACC_FUT_CREDIT,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( ACC_NUMBER ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( ACC_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_TYPE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_SETTLEABLE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_RECONCILIABLE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FORWARD ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( ACC_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
										/* below data are not imported */
		{ OFA_BOX_CSV( ACC_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_VAL_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_VAL_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ROUGH_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_ROUGH_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_OPEN_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_OPEN_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FUT_DEBIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( ACC_FUT_CREDIT ),
				OFA_TYPE_AMOUNT,
				FALSE,
				FALSE },
		{ 0 }
};

struct _ofoAccountPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

#define account_get_amount(I)           ofo_base_getter(ACCOUNT,account,amount,0,(I))
#define account_get_counter(I)          ofo_base_getter(ACCOUNT,account,counter,0,(I))
#define account_get_date(I)             ofo_base_getter(ACCOUNT,account,date,NULL,(I))
#define account_get_string(I)           ofo_base_getter(ACCOUNT,account,string,NULL,(I))
#define account_get_timestamp(I)        ofo_base_getter(ACCOUNT,account,timestamp,NULL,(I))

#define account_set_amount(I,V)         ofo_base_setter(ACCOUNT,account,amount,(I),(V))
#define account_set_counter(I,V)        ofo_base_setter(ACCOUNT,account,counter,(I),(V))
#define account_set_date(I,V)           ofo_base_setter(ACCOUNT,account,date,(I),(V))
#define account_set_string(I,V)         ofo_base_setter(ACCOUNT,account,string,(I),(V))
#define account_set_timestamp(I,V)      ofo_base_setter(ACCOUNT,account,timestamp,(I),(V))

/* whether a root account has children, and wich are they ?
 */
typedef struct {
	const gchar *number;
	gint         children_count;
	GList       *children_list;
}
	sChildren;

static ofoBaseClass *ofo_account_parent_class = NULL;

static void         on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data );
static void         on_new_object_entry( ofoDossier *dossier, ofoEntry *entry );
static void         on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data );
static void         on_updated_object_currency_code( ofoDossier *dossier, const gchar *prev_id, const gchar *code );
static void         on_future_to_rough_entry( ofoDossier *dossier, ofoEntry *entry, void *empty );
static void         on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data );
static GList       *account_load_dataset( ofoDossier *dossier );
static ofoAccount  *account_find_by_number( GList *set, const gchar *number );
static gint         account_count_for_currency( const ofaDbms *dbms, const gchar *currency );
static gint         account_count_for( const ofaDbms *dbms, const gchar *field, const gchar *mnemo );
static gint         account_count_for_like( const ofaDbms *dbms, const gchar *field, gint number );
static const gchar *account_get_string_ex( const ofoAccount *account, gint data_id );
static void         account_get_children( const ofoAccount *account, sChildren *child_str, ofoDossier *dossier );
static void         account_iter_children( const ofoAccount *account, sChildren *child_str );
static gboolean     do_archive_open_balance( ofoAccount *account, const ofaDbms *dbms );
static void         account_set_upd_user( ofoAccount *account, const gchar *user );
static void         account_set_upd_stamp( ofoAccount *account, const GTimeVal *stamp );
static void         account_set_val_debit( ofoAccount *account, ofxAmount amount );
static void         account_set_val_credit( ofoAccount *account, ofxAmount amount );
static void         account_set_rough_debit( ofoAccount *account, ofxAmount amount );
static void         account_set_rough_credit( ofoAccount *account, ofxAmount amount );
static void         account_set_futur_debit( ofoAccount *account, ofxAmount amount );
static void         account_set_futur_credit( ofoAccount *account, ofxAmount amount );
static void         account_set_open_debit( ofoAccount *account, ofxAmount amount );
static void         account_set_open_credit( ofoAccount *account, ofxAmount amount );
static gboolean     account_do_insert( ofoAccount *account, const ofaDbms *dbms, const gchar *user );
static gboolean     account_do_update( ofoAccount *account, const ofaDbms *dbms, const gchar *user, const gchar *prev_number );
static gboolean     account_update_amounts( ofoAccount *account, const ofaDbms *dbms );
static gboolean     account_do_delete( ofoAccount *account, const ofaDbms *dbms );
static gint         account_cmp_by_number( const ofoAccount *a, const gchar *number );
static gint         account_cmp_by_ptr( const ofoAccount *a, const ofoAccount *b );
static void         iexportable_iface_init( ofaIExportableInterface *iface );
static guint        iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean     iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier );
static void         iimportable_iface_init( ofaIImportableInterface *iface );
static guint        iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean     iimportable_import( ofaIImportable *exportable, GSList *lines, ofoDossier *dossier );
static gboolean     account_do_drop_content( const ofaDbms *dbms );

OFA_IDATASET_LOAD( ACCOUNT, account );

static void
account_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_account_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ACC_NUMBER ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, ACC_LABEL ));

	g_return_if_fail( instance && OFO_IS_ACCOUNT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_account_parent_class )->finalize( instance );
}

static void
account_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_ACCOUNT( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_account_parent_class )->dispose( instance );
}

static void
ofo_account_init( ofoAccount *self )
{
	static const gchar *thisfn = "ofo_account_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_ACCOUNT, ofoAccountPrivate );
}

static void
ofo_account_class_init( ofoAccountClass *klass )
{
	static const gchar *thisfn = "ofo_account_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_account_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = account_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_finalize;

	g_type_class_add_private( klass, sizeof( ofoAccountPrivate ));
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
		( GClassInitFunc ) ofo_account_class_init,
		NULL,
		NULL,
		sizeof( ofoAccount ),
		0,
		( GInstanceInitFunc ) ofo_account_init
	};

	static const GInterfaceInfo iexportable_iface_info = {
		( GInterfaceInitFunc ) iexportable_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoAccount", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	g_type_add_interface_static( type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );

	return( type );
}

GType
ofo_account_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

static void
free_account_balance( ofsAccountBalance *sbal )
{
	g_free( sbal->account );
	g_free( sbal->currency );
	g_free( sbal );
}

/**
 * ofo_account_free_balances:
 *
 * Free a list of dynamically allocated ofsAccountBalance structures.
 */
void
ofo_account_free_balances( GList *balances )
{
	g_list_free_full( balances, ( GDestroyNotify ) free_account_balance );
}

/**
 * ofo_account_connect_handlers:
 *
 * This function is called once, when opening the dossier.
 */
void
ofo_account_connect_handlers( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_connect_handlers";

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_NEW_OBJECT, G_CALLBACK( on_new_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, G_CALLBACK( on_updated_object ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_FUTURE_ROUGH_ENTRY, G_CALLBACK( on_future_to_rough_entry ), NULL );

	g_signal_connect( G_OBJECT( dossier ),
				SIGNAL_DOSSIER_VALIDATED_ENTRY, G_CALLBACK( on_validated_entry ), NULL );
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data )
{
	static const gchar *thisfn = "ofo_account_on_new_object";

	g_debug( "%s: dossier=%p, object=%p (%s), user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) user_data );

	if( OFO_IS_ENTRY( object )){
		on_new_object_entry( dossier, OFO_ENTRY( object ));
	}
}

/*
 * a new entry has been recorded, so update the daily balances
 */
static void
on_new_object_entry( ofoDossier *dossier, ofoEntry *entry )
{
	ofaEntryStatus status;
	ofoAccount *account;
	gdouble debit, credit, prev;

	/* the only case where an entry is created with a 'past' status
	 *  is an imported entry in the past (before the beginning of the
	 *  exercice) - in this case, the 'new_object' message should not be
	 *  sent
	 * if not in the past, only allowed status are 'rough' or 'future' */
	status = ofo_entry_get_status( entry );
	g_return_if_fail( status != ENT_STATUS_PAST );
	g_return_if_fail( status == ENT_STATUS_ROUGH || status == ENT_STATUS_FUTURE );

	account = ofo_account_get_by_number( dossier, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	credit = ofo_entry_get_credit( entry );

	/* impute the new entry either to the debit or the credit of daily
	 * or futur balances depending of the position of the effect date
	 * vs. ending date of the exercice
	 */
	switch( status ){
		case ENT_STATUS_ROUGH:
			if( debit ){
				prev = ofo_account_get_rough_debit( account );
				account_set_rough_debit( account, prev+debit );

			} else {
				prev = ofo_account_get_rough_credit( account );
				account_set_rough_credit( account, prev+credit );
			}
			break;

		case ENT_STATUS_FUTURE:
			if( debit ){
				prev = ofo_account_get_futur_debit( account );
				account_set_futur_debit( account, prev+debit );

			} else {
				prev = ofo_account_get_futur_credit( account );
				account_set_futur_credit( account, prev+credit );
			}
			break;

		default:
			g_return_if_reached();
			break;
	}

	if( account_update_amounts( account, ofo_dossier_get_dbms( dossier ))){
		g_signal_emit_by_name(
				G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( account ), NULL );
	}
}

static void
on_updated_object( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, gpointer user_data )
{
	static const gchar *thisfn = "ofo_account_on_updated_object";
	const gchar *code;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, user_data=%p",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) user_data );

	if( OFO_IS_CURRENCY( object )){
		if( prev_id && g_utf8_strlen( prev_id, -1 )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( dossier, prev_id, code );
			}
		}
	}
}

/*
 * the currency code iso has been modified: update the accounts which
 * use it
 */
static void
on_updated_object_currency_code( ofoDossier *dossier, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_ACCOUNTS SET ACC_CURRENCY='%s' WHERE ACC_CURRENCY='%s'",
						code, prev_id );

	ofa_dbms_query( ofo_dossier_get_dbms( dossier ), query, TRUE );

	g_free( query );

	if( ofa_idataset_get_dataset( dossier, OFO_TYPE_ACCOUNT )){
		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_ACCOUNT );
	}
}

/*
 * an entry is becomes rough from a future status (after closing exercice)
 */
static void
on_future_to_rough_entry( ofoDossier *dossier, ofoEntry *entry, void *empty )
{
	static const gchar *thisfn = "ofo_account_on_validated_entry";
	ofoAccount *account;
	ofxAmount debit, credit, amount;

	g_debug( "%s: dossier=%p, entry=%p, empty=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) empty );

	account = ofo_account_get_by_number( dossier, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	if( debit ){
		amount = ofo_account_get_rough_debit( account );
		account_set_rough_debit( account, amount+debit );
		amount = ofo_account_get_futur_debit( account );
		account_set_futur_debit( account, amount-debit );
	}

	credit = ofo_entry_get_credit( entry );
	if( credit ){
		amount = ofo_account_get_rough_credit( account );
		account_set_rough_credit( account, amount+credit );
		amount = ofo_account_get_futur_credit( account );
		account_set_futur_credit( account, amount-credit );
	}

	if( account_update_amounts( account, ofo_dossier_get_dbms( dossier ))){
		g_signal_emit_by_name(
				G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( account ), NULL );
	}
}

/*
 * an entry is validated, either individually or as the result of the
 * closing of a journal
 */
static void
on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data )
{
	static const gchar *thisfn = "ofo_account_on_validated_entry";
	ofoAccount *account;
	ofxAmount debit, credit, amount;

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	account = ofo_account_get_by_number( dossier, ofo_entry_get_account( entry ));
	g_return_if_fail( account && OFO_IS_ACCOUNT( account ));

	debit = ofo_entry_get_debit( entry );
	if( debit ){
		amount = ofo_account_get_rough_debit( account );
		account_set_rough_debit( account, amount-debit );
		amount = ofo_account_get_val_debit( account );
		account_set_val_debit( account, amount+debit );
	}

	credit = ofo_entry_get_credit( entry );
	if( credit ){
		amount = ofo_account_get_rough_credit( account );
		account_set_rough_credit( account, amount-credit );
		amount = ofo_account_get_val_credit( account );
		account_set_val_credit( account, amount+credit );
	}

	if( account_update_amounts( account, ofo_dossier_get_dbms( dossier ))){
		g_signal_emit_by_name(
				G_OBJECT( dossier ),
				SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref( account ), NULL );
	}
}

/*
 * Loads/reloads the ordered list of accounts
 */
static GList *
account_load_dataset( ofoDossier *dossier )
{
	return( ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					"OFA_T_ACCOUNTS ORDER BY ACC_NUMBER ASC",
					OFO_TYPE_ACCOUNT ));
}

/**
 * ofo_account_get_dataset_for_solde:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: the #ofoAccount dataset, without the solde accounts.
 *
 * The returned list should be #ofo_account_free_dataset() by the caller.
 */
GList *
ofo_account_get_dataset_for_solde( ofoDossier *dossier )
{
	GList *dataset;
	gchar *query;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	query = g_strdup_printf( "OFA_T_ACCOUNTS WHERE "
			"	ACC_TYPE!='R' AND "
			"	ACC_NUMBER NOT IN (SELECT DOS_SLD_ACCOUNT FROM OFA_T_DOSSIER_CUR)" );

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					query,
					OFO_TYPE_ACCOUNT );

	g_free( query );

	return( dataset );
}

/**
 * ofo_account_get_by_number:
 * @dossier: the currently opened #ofoDossier dossier.
 * @number: the required account number.
 *
 * Returns: the searched #ofoAccount account, or %NULL.
 *
 * The returned object is owned by the #ofoAccount class, and should
 * not be unreffed by the caller.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
ofoAccount *
ofo_account_get_by_number( ofoDossier *dossier, const gchar *number )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	if( !number || !g_utf8_strlen( number, -1 )){
		return( NULL );
	}

	OFA_IDATASET_GET( dossier, ACCOUNT, account );

	return( account_find_by_number( account_dataset, number ));
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
 * ofo_account_use_class:
 * @dossier: the currently opened #ofoDossier dossier.
 * @number: the searched class number
 *
 * Returns: %TRUE if at least one recorded account makes use of the
 * specified class number.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
gboolean
ofo_account_use_class( ofoDossier *dossier, gint number )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	OFA_IDATASET_GET( dossier, ACCOUNT, account );

	return( account_count_for_like( ofo_dossier_get_dbms( dossier ), "ACC_NUMBER", number ) > 0 );
}

/**
 * ofo_account_use_currency:
 * @dossier: the currently opened #ofoDossier dossier.
 * @currency: the currency ISO 3A code
 *
 * Returns: %TRUE if at least one recorded account makes use of the
 * specified currency.
 *
 * The whole account dataset is loaded from DBMS if not already done.
 */
gboolean
ofo_account_use_currency( ofoDossier *dossier, const gchar *currency )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( currency && g_utf8_strlen( currency, -1 ), FALSE );

	OFA_IDATASET_GET( dossier, ACCOUNT, account );

	return( account_count_for_currency( ofo_dossier_get_dbms( dossier ), currency ) > 0 );
}

static gint
account_count_for_currency( const ofaDbms *dbms, const gchar *currency )
{
	return( account_count_for( dbms, "ACC_CURRENCY", currency ));
}

static gint
account_count_for( const ofaDbms *dbms, const gchar *field, const gchar *mnemo )
{
	gint count;
	gchar *query;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE %s='%s'", field, mnemo );

	ofa_dbms_query_int( dbms, query, &count, TRUE );

	g_free( query );

	return( count );
}

static gint
account_count_for_like( const ofaDbms *dbms, const gchar *field, gint number )
{
	gint count;
	gchar *query;

	count = 0;
	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_ACCOUNTS WHERE %s LIKE '%d%%'", field, number );

	ofa_dbms_query_int( dbms, query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_account_new:
 *
 * Returns: a new #ofoAccount object.
 */
ofoAccount *
ofo_account_new( void )
{
	ofoAccount *account;

	account = g_object_new( OFO_TYPE_ACCOUNT, NULL );
	OFO_BASE( account )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( account );
}

/**
 * ofo_account_get_class:
 * @account: the #ofoAccount account
 *
 * Returns: the class number of the @account.
 */
gint
ofo_account_get_class( const ofoAccount *account )
{
	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), 0 );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		return( ofo_account_get_class_from_number(
						ofo_account_get_number( account )));
	}

	return( 0 );
}

/**
 * ofo_account_get_class_from_number:
 * @account_number: an account number.
 *
 * Returns: the class number of this @account_number.
 *
 * TODO: make this UTF8-compliant as first character (a digit) may not
 * always be one byte wide (or is it ?)
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
 * ofo_account_get_level_from_number:
 * @account_number: an account number.
 *
 * Returns: the level number of this @account_number.
 *
 * The level is defined as the count of digits.
 * A class is defined as of level 1.
 * Any actual account is at least of level 2.
 *
 * Note: this is UTF8-compliant as g_utf8_strlen() rightly returns
 *  the count of characters.
 */
gint
ofo_account_get_level_from_number( const gchar *account_number )
{
	if( !account_number ){
		return( 0 );
	}
	return( g_utf8_strlen( account_number, -1 ));
}

/**
 * ofo_account_get_number:
 * @account: the #ofoAccount account
 *
 * Returns: the number of the @account.
 */
const gchar *
ofo_account_get_number( const ofoAccount *account )
{
	account_get_string( ACC_NUMBER );
}

/**
 * ofo_account_get_label:
 * @account: the #ofoAccount account
 *
 * Returns: the label of the @account.
 */
const gchar *
ofo_account_get_label( const ofoAccount *account )
{
	account_get_string( ACC_LABEL );
}

/**
 * ofo_account_get_currency:
 * @account: the #ofoAccount account
 *
 * Returns: the currency ISO 3A code of the @account.
 */
const gchar *
ofo_account_get_currency( const ofoAccount *account )
{
	account_get_string( ACC_CURRENCY );
}

/**
 * ofo_account_get_notes:
 * @account: the #ofoAccount account
 *
 * Returns: the notes attached to the @account.
 */
const gchar *
ofo_account_get_notes( const ofoAccount *account )
{
	account_get_string( ACC_NOTES );
}

/**
 * ofo_account_get_type_account:
 * @account: the #ofoAccount account
 *
 * Returns: the type of the @account
 *
 * The type is identified by a letter which may be:
 * 'R': this is a root account, with or without children
 * 'D' (default): this is a detail account, on which entries may be
 *                written.
 */
const gchar *
ofo_account_get_type_account( const ofoAccount *account )
{
	static const gchar *thisfn = "ofo_account_get_type_account";
	const gchar *str;

	str = account_get_string_ex( account, ACC_TYPE );

	if(  str && g_utf8_strlen( str, -1 )){

		if( !g_utf8_collate( str, ACCOUNT_TYPE_ROOT ) || !g_utf8_collate( str, ACCOUNT_TYPE_DETAIL )){
			return( str );
		}

		g_warning( "%s: invalid type account: %s", thisfn, str );
		return( ACCOUNT_TYPE_DETAIL );
	}

	/* default is detail account */
	return( ACCOUNT_TYPE_DETAIL );
}

/**
 * ofo_account_get_upd_user:
 * @account: the #ofoAccount account
 *
 * Returns: the user name responsible of the last properties update.
 */
const gchar *
ofo_account_get_upd_user( const ofoAccount *account )
{
	account_get_string( ACC_UPD_USER );
}

/**
 * ofo_account_get_upd_stamp:
 * @account: the #ofoAccount account
 *
 * Returns: the timestamp of the last properties update.
 */
const GTimeVal *
ofo_account_get_upd_stamp( const ofoAccount *account )
{
	account_get_timestamp( ACC_UPD_STAMP );
}

/**
 * ofo_account_get_val_debit:
 * @account: the #ofoAccount account
 *
 * Returns: the validated debit balance of the @account.
 */
ofxAmount
ofo_account_get_val_debit( const ofoAccount *account )
{
	account_get_amount( ACC_VAL_DEBIT );
}

/**
 * ofo_account_get_val_credit:
 * @account: the #ofoAccount account
 *
 * Returns: the validated credit balance of the @account.
 */
ofxAmount
ofo_account_get_val_credit( const ofoAccount *account )
{
	account_get_amount( ACC_VAL_CREDIT );
}

/**
 * ofo_account_get_rough_debit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_rough_debit( const ofoAccount *account )
{
	account_get_amount( ACC_ROUGH_DEBIT );
}

/**
 * ofo_account_get_rough_credit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_rough_credit( const ofoAccount *account )
{
	account_get_amount( ACC_ROUGH_CREDIT );
}

/**
 * ofo_account_get_open_debit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_open_debit( const ofoAccount *account )
{
	account_get_amount( ACC_OPEN_DEBIT );
}

/**
 * ofo_account_get_open_credit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_open_credit( const ofoAccount *account )
{
	account_get_amount( ACC_OPEN_CREDIT );
}

/**
 * ofo_account_get_futur_debit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_futur_debit( const ofoAccount *account )
{
	account_get_amount( ACC_FUT_DEBIT );
}

/**
 * ofo_account_get_futur_credit:
 * @account: the #ofoAccount account
 */
ofxAmount
ofo_account_get_futur_credit( const ofoAccount *account )
{
	account_get_amount( ACC_FUT_CREDIT );
}

/**
 * ofo_account_is_deletable:
 * @account: the #ofoAccount account
 *
 * A account is considered to be deletable if no entry is referencing it.
 *
 * Whether a root account with children is deletable is a user preference.
 * To be deletable, all children must also be deletable.
 */
gboolean
ofo_account_is_deletable( const ofoAccount *account, ofoDossier *dossier )
{
	gboolean deletable;
	GList *children, *it;
	const gchar *number;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		number = ofo_account_get_number( account );
		deletable = !ofo_entry_use_account( dossier, number ) &&
					!ofo_dossier_use_account( dossier, number );

		if( ofo_account_is_root( account ) && ofa_prefs_account_delete_root_with_children()){

			children = ofo_account_get_children( account, dossier );
			for( it=children ; it ; it=it->next ){
				deletable &= ofo_account_is_deletable( OFO_ACCOUNT( it->data ), dossier );
			}
			g_list_free( children );
		}

		return( deletable );
	}

	return( FALSE );
}

/**
 * ofo_account_is_root:
 * @account: the #ofoAccount account
 *
 * Default is 'detail' type.
 */
gboolean
ofo_account_is_root( const ofoAccount *account )
{
	gboolean is_root;
	const gchar *account_type;

	g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );

	is_root = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_type = ofo_account_get_type_account( account );
		if( account_type && !g_utf8_collate( account_type, "R" )){
			is_root = TRUE;
		}
	}

	return( is_root );
}

/**
 * ofo_account_is_settleable:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account is settleable
 *
 * All accounts are actually settleable, i.e. all entries may be
 * settled. But only unsettled entries written on settleable accounts
 * will be reported on next exercice at closing time.
 */
gboolean
ofo_account_is_settleable( const ofoAccount *account )
{
	gboolean is_settleable;
	const gchar *str;

	str = account_get_string_ex( account, ACC_SETTLEABLE );
	is_settleable = str && g_utf8_strlen( str, -1 ) && !g_utf8_collate( str, ACCOUNT_SETTLEABLE );

	return( is_settleable );
}

/**
 * ofo_account_is_reconciliable:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account is reconciliable
 *
 * All accounts are actually reconciliable, i.e. all entries may be
 * reconciliated. But only unreconciliated entries written on
 * reconciliable accounts will be reported on next exercice at closing
 * time.
 */
gboolean
ofo_account_is_reconciliable( const ofoAccount *account )
{
	gboolean is_reconciliable;
	const gchar *str;

	str = account_get_string_ex( account, ACC_RECONCILIABLE );
	is_reconciliable = str && g_utf8_strlen( str, -1 ) && !g_utf8_collate( str, ACCOUNT_RECONCILIABLE );

	return( is_reconciliable );
}

/**
 * ofo_account_is_forward:
 * @account: the #ofoAccount account
 *
 * Returns: %TRUE if the account supports carried forward entries.
 */
gboolean
ofo_account_is_forward( const ofoAccount *account )
{
	gboolean is_forward;
	const gchar *str;

	str = account_get_string_ex( account, ACC_FORWARD );
	is_forward = str && g_utf8_strlen( str, -1 ) && !g_utf8_collate( str, ACCOUNT_FORWARDABLE );

	return( is_forward );
}

static const gchar *
account_get_string_ex( const ofoAccount *account, gint data_id )
{
	account_get_string( data_id );
}

/**
 * ofo_account_is_valid_data:
 */
gboolean
ofo_account_is_valid_data( const gchar *number, const gchar *label, const gchar *currency, const gchar *type )
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

	/* currency must be set for detail account */
	if( !is_root ){
		if( !currency || !g_utf8_strlen( currency, -1 )){
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofo_account_get_global_deffect:
 * @account: the #ofoAccount account
 * @dossier: the current #ofoDossier
 * @date: [out]: where to store the returned date.
 *
 * Returns the most recent effect date of the account, keeping into
 * account both validated, rough and future entries.
 *
 * This is used when printing reconciliation summary to qualify the
 * starting date of the print.
 *
 * The returned value may be %NULL or not valid if no entry has ever
 * been recorded on this @account.
 */
GDate *
ofo_account_get_global_deffect( const ofoAccount *account, ofoDossier *dossier, GDate *date )
{
	GDate max_val, max_rough, max_futur;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );

	ofo_entry_get_max_val_deffect( dossier, ofo_account_get_number( account ), &max_val );
	ofo_entry_get_max_rough_deffect( dossier, ofo_account_get_number( account ), &max_rough );
	ofo_entry_get_max_futur_deffect( dossier, ofo_account_get_number( account ), &max_futur );

	my_date_clear( date );

	if( my_date_is_valid( &max_val )){
		my_date_set_from_date( date, &max_val );
	}

	if( my_date_is_valid( &max_rough ) &&
			( !my_date_is_valid( date ) || my_date_compare( date, &max_rough ) < 0 )){
		my_date_set_from_date( date, &max_rough );
	}

	if( my_date_is_valid( &max_futur ) &&
			( !my_date_is_valid( date ) || my_date_compare( date, &max_futur ) < 0 )){
		my_date_set_from_date( date, &max_futur );
	}

	return( date );
}

/**
 * ofo_account_get_global_solde:
 * @account: the #ofoAccount account
 *
 * Returns: the current global balance of the @account, taking into
 * account both validated and rough balances.
 *
 * This is used when printing reconciliation summary to qualify the
 * starting solde of the print.
 *
 * The returned value is lesser than zero for a debit, or greater than
 * zero for a credit.
 */
ofxAmount
ofo_account_get_global_solde( const ofoAccount *account )
{
	ofxAmount amount;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), 0 );

	amount = 0;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		amount -= ofo_account_get_val_debit( account );
		amount += ofo_account_get_val_credit( account );
		amount -= ofo_account_get_rough_debit( account );
		amount += ofo_account_get_rough_credit( account );
	}

	return( amount );
}

/**
 * ofo_account_has_children:
 * @account: the #ofoAccount account
 *
 * Whether an account has children is only relevant for a root account
 * (but this is not checked here).
 */
gboolean
ofo_account_has_children( const ofoAccount *account, ofoDossier *dossier )
{
	sChildren child_str;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_get_children( account, &child_str, dossier );
		g_list_free( child_str.children_list );

		return( child_str.children_count > 0 );
	}

	return( FALSE );
}

/**
 * ofo_account_get_children:
 * @account: the #ofoAccount account
 *
 * Returns: the list of children accounts.
 *
 * The list may be freed with g_list_free().
 */
GList *
ofo_account_get_children( const ofoAccount *account, ofoDossier *dossier )
{
	sChildren child_str;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_get_children( account, &child_str, dossier );

		return( child_str.children_list );
	}

	return( NULL );
}

static void
account_get_children( const ofoAccount *account, sChildren *child_str, ofoDossier *dossier )
{
	memset( child_str, '\0' ,sizeof( sChildren ));
	child_str->number = ofo_account_get_number( account );
	child_str->children_count = 0;
	child_str->children_list = NULL;

	OFA_IDATASET_GET( dossier, ACCOUNT, account );

	g_list_foreach( account_dataset, ( GFunc ) account_iter_children, child_str );
}

static void
account_iter_children( const ofoAccount *account, sChildren *child_str )
{
	const gchar *number;

	number = ofo_account_get_number( account );
	if( g_str_has_prefix( number, child_str->number ) &&
			g_utf8_collate( number, child_str->number ) > 0 ){

		child_str->children_count += 1;
		child_str->children_list = g_list_append( child_str->children_list, ( gpointer ) account );
	}
}

/**
 * ofo_account_is_child_of:
 * @account: the #ofoAccount account
 * @candidate: another account to be compared relatively to @account
 *
 * Returns: %TRUE if the @number should logically be a child of @æccount.
 */
gboolean
ofo_account_is_child_of( const ofoAccount *account, const ofoAccount *candidate )
{
	const gchar *account_number;
	const gchar *candidate_number;
	gboolean is_child;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( candidate && OFO_IS_ACCOUNT( candidate ), FALSE );

	is_child = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		account_number = ofo_account_get_number( account );
		candidate_number = ofo_account_get_number( candidate );
		is_child = g_str_has_prefix( candidate_number, account_number );
	}

	return( is_child );
}

/**
 * ofo_account_archive_open_balances:
 * @dossier: the currently opened dossier.
 *
 * As part of the closing of the exercice N, we archive the balances
 * of the accounts at the beginning of the exercice N+1. These balances
 * take into account the carried forward entries (fr: reports à
 * nouveau).
 *
 * At this time, all ledgers on exercice N should have been closed, and
 * no entries should have been recorded in ledgers for exercice N+1.
 */
gboolean
ofo_account_archive_open_balance( ofoAccount *account, ofoDossier *dossier )
{
	gboolean ok;

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	ok = FALSE;

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		if( ofo_account_is_root( account )){
			ok = TRUE;

		} else {
			ok = do_archive_open_balance( account, ofo_dossier_get_dbms( dossier ));
		}
	}

	return( ok );
}

static gboolean
do_archive_open_balance( ofoAccount *account, const ofaDbms *dbms )
{
	GString *query;
	gchar *samount;
	ofxAmount amount;
	gboolean ok;

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	amount = ofo_account_get_rough_debit( account )+ofo_account_get_val_debit( account );
	account_set_open_debit( account, amount );
	samount = my_double_to_sql( amount );
	g_string_append_printf( query, "ACC_OPEN_DEBIT=%s,", samount );
	g_free( samount );

	amount = ofo_account_get_rough_credit( account )+ofo_account_get_val_credit( account );
	account_set_open_credit( account, amount );
	samount = my_double_to_sql( amount );
	g_string_append_printf( query, "ACC_OPEN_CREDIT=%s ", samount );
	g_free( samount );

	g_string_append_printf( query,
			"WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_account_set_number:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_number( ofoAccount *account, const gchar *number )
{
	account_set_string( ACC_NUMBER, number );
}

/**
 * ofo_account_set_label:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_label( ofoAccount *account, const gchar *label )
{
	account_set_string( ACC_LABEL, label );
}

/**
 * ofo_account_set_currency:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_currency( ofoAccount *account, const gchar *currency )
{
	account_set_string( ACC_CURRENCY, currency );
}

/**
 * ofo_account_set_notes:
 * @account: the #ofoAccount account
 */
void
ofo_account_set_notes( ofoAccount *account, const gchar *notes )
{
	account_set_string( ACC_NOTES, notes );
}

/**
 * ofo_account_set_type_account:
 * @account: the #ofoAccount account
 * @type: either "R" for a root account or "D" for a detail account.
 */
void
ofo_account_set_type_account( ofoAccount *account, const gchar *type )
{
	const gchar *validated;

	validated = ACCOUNT_TYPE_DETAIL;
	if( type && g_utf8_strlen( type, -1 ) &&
			( !g_utf8_collate( type, ACCOUNT_TYPE_DETAIL ) || !g_utf8_collate( type, ACCOUNT_TYPE_ROOT ))){

			validated = type;
	}

	account_set_string( ACC_TYPE, validated );
}

/**
 * ofo_account_set_settleable:
 * @account: the #ofoAccount account
 * @settleable: %TRUE if the account is to be set settleable
 */
void
ofo_account_set_settleable( ofoAccount *account, gboolean settleable )
{
	account_set_string( ACC_SETTLEABLE, settleable ? ACCOUNT_SETTLEABLE : NULL );
}

/**
 * ofo_account_set_reconciliable:
 * @account: the #ofoAccount account
 * @reconciliable: %TRUE if the account is to be set reconciliable
 */
void
ofo_account_set_reconciliable( ofoAccount *account, gboolean reconciliable )
{
	account_set_string( ACC_RECONCILIABLE, reconciliable ? ACCOUNT_RECONCILIABLE : NULL );
}

/**
 * ofo_account_set_forward:
 * @account: the #ofoAccount account
 * @forward: %TRUE if the account supports carried forward entries
 */
void
ofo_account_set_forward( ofoAccount *account, gboolean forward )
{
	account_set_string( ACC_FORWARD, forward ? ACCOUNT_FORWARDABLE : NULL );
}

/*
 * ofo_account_set_upd_user:
 * @account: the #ofoAccount account
 */
static void
account_set_upd_user( ofoAccount *account, const gchar *user )
{
	account_set_string( ACC_UPD_USER, user );
}

/*
 * ofo_account_set_upd_stamp:
 * @account: the #ofoAccount account
 */
static void
account_set_upd_stamp( ofoAccount *account, const GTimeVal *stamp )
{
	account_set_timestamp( ACC_UPD_STAMP, stamp );
}

/*
 * ofo_account_set_val_debit:
 * @account: the #ofoAccount account
 */
static void
account_set_val_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_VAL_DEBIT, amount );
}

/*
 * ofo_account_set_val_credit:
 * @account: the #ofoAccount account
 */
static void
account_set_val_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_VAL_CREDIT, amount );
}

/*
 * ofo_account_set_rough_debit:
 * @account: the #ofoAccount account
 */
static void
account_set_rough_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_ROUGH_DEBIT, amount );
}

/*
 * ofo_account_set_rough_credit:
 * @account: the #ofoAccount account
 */
static void
account_set_rough_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_ROUGH_CREDIT, amount );
}

/*
 * ofo_account_set_open_debit:
 * @account: the #ofoAccount account
 */
static void
account_set_open_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_OPEN_DEBIT, amount );
}

/*
 * ofo_account_set_open_credit:
 * @account: the #ofoAccount account
 */
static void
account_set_open_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_OPEN_CREDIT, amount );
}

/*
 * ofo_account_set_futur_debit:
 * @account: the #ofoAccount account
 */
static void
account_set_futur_debit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FUT_DEBIT, amount );
}

/*
 * ofo_account_set_futur_credit:
 * @account: the #ofoAccount account
 */
static void
account_set_futur_credit( ofoAccount *account, ofxAmount amount )
{
	account_set_amount( ACC_FUT_CREDIT, amount );
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

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, dossier=%p",
				thisfn, ( void * ) account, ( void * ) dossier );

		if( account_do_insert(
					account,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, ACCOUNT, account );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_insert( ofoAccount *account, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;

	ok = FALSE;

	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_ACCOUNTS" );

	g_string_append_printf( query,
			"	(ACC_NUMBER,ACC_LABEL,ACC_CURRENCY,ACC_NOTES,"
			"	ACC_TYPE,ACC_SETTLEABLE,ACC_RECONCILIABLE,ACC_FORWARD,"
			"	ACC_UPD_USER, ACC_UPD_STAMP)"
			"	VALUES ('%s','%s',",
					ofo_account_get_number( account ),
					label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "NULL," );
	} else {
		g_string_append_printf( query, "'%s',", ofo_account_get_currency( account ));
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s',", ofo_account_get_type_account( account ));

	if( ofo_account_is_settleable( account )){
		g_string_append_printf( query, "'%s',", ACCOUNT_SETTLEABLE );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_account_is_reconciliable( account )){
		g_string_append_printf( query, "'%s',", ACCOUNT_RECONCILIABLE );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_account_is_forward( account )){
		g_string_append_printf( query, "'%s',", ACCOUNT_FORWARDABLE );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", user, stamp_str );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		account_set_upd_user( account, user );
		account_set_upd_stamp( account, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

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

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( prev_number && g_utf8_strlen( prev_number, -1 ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, dossier=%p, prev_number=%s",
					thisfn, ( void * ) account, ( void * ) dossier, prev_number );

		if( account_do_update(
					account,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ),
					prev_number )){

			OFA_IDATASET_UPDATE( dossier, ACCOUNT, account, prev_number );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_update( ofoAccount *account, const ofaDbms *dbms, const gchar *user, const gchar *prev_number )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	const gchar *new_number;
	gchar *stamp_str;
	GTimeVal stamp;

	ok = FALSE;

	label = my_utils_quote( ofo_account_get_label( account ));
	notes = my_utils_quote( ofo_account_get_notes( account ));
	new_number = ofo_account_get_number( account );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	if( g_utf8_collate( new_number, prev_number )){
		g_string_append_printf( query, "ACC_NUMBER='%s',", new_number );
	}

	g_string_append_printf( query, "ACC_LABEL='%s',", label );

	if( ofo_account_is_root( account )){
		query = g_string_append( query, "ACC_CURRENCY=NULL," );
	} else {
		g_string_append_printf( query, "ACC_CURRENCY='%s',", ofo_account_get_currency( account ));
	}

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "ACC_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "ACC_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	ACC_TYPE='%s',",
					ofo_account_get_type_account( account ));

	if( ofo_account_is_settleable( account )){
		g_string_append_printf( query, "ACC_SETTLEABLE='%s',", ACCOUNT_SETTLEABLE );
	} else {
		query = g_string_append( query, "ACC_SETTLEABLE=NULL," );
	}

	if( ofo_account_is_reconciliable( account )){
		g_string_append_printf( query, "ACC_RECONCILIABLE='%s',", ACCOUNT_RECONCILIABLE );
	} else {
		query = g_string_append( query, "ACC_RECONCILIABLE=NULL," );
	}

	if( ofo_account_is_forward( account )){
		g_string_append_printf( query, "ACC_FORWARD='%s',", ACCOUNT_FORWARDABLE );
	} else {
		query = g_string_append( query, "ACC_FORWARD=NULL," );
	}

	g_string_append_printf( query,
			"	ACC_UPD_USER='%s',ACC_UPD_STAMP='%s'"
			"	WHERE ACC_NUMBER='%s'",
					user,
					stamp_str,
					prev_number );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		account_set_upd_user( account, user );
		account_set_upd_stamp( account, &stamp );

		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( stamp_str );
	g_free( notes );
	g_free( label );

	return( ok );
}

static gboolean
account_update_amounts( ofoAccount *account, const ofaDbms *dbms )
{
	GString *query;
	gboolean ok;
	gchar *samount;
	ofxAmount amount;

	query = g_string_new( "UPDATE OFA_T_ACCOUNTS SET " );

	/* validated debit */
	amount = ofo_account_get_val_debit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_VAL_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_VAL_DEBIT=NULL," );
	}

	/* validated credit */
	amount = ofo_account_get_val_credit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_VAL_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_VAL_CREDIT=NULL," );
	}

	/* rough debit */
	amount = ofo_account_get_rough_debit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_ROUGH_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_ROUGH_DEBIT=NULL," );
	}

	/* rough credit */
	amount = ofo_account_get_rough_credit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_ROUGH_CREDIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_ROUGH_CREDIT=NULL," );
	}

	/* future debit */
	amount = ofo_account_get_futur_debit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_FUT_DEBIT=%s,", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FUT_DEBIT=NULL," );
	}

	/* future credit */
	amount = ofo_account_get_futur_credit( account );
	if( amount ){
		samount = my_double_to_sql( amount );
		g_string_append_printf( query, "ACC_FUT_CREDIT=%s ", samount );
		g_free( samount );
	} else {
		query = g_string_append( query, "ACC_FUT_CREDIT=NULL " );
	}

	g_string_append_printf( query,
				"WHERE ACC_NUMBER='%s'", ofo_account_get_number( account ));

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}

/**
 * ofo_account_delete:
 */
gboolean
ofo_account_delete( ofoAccount *account, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_account_delete";

	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_account_is_deletable( account, dossier ), FALSE );

	if( !OFO_BASE( account )->prot->dispose_has_run ){

		g_debug( "%s: account=%p, dossier=%p",
				thisfn, ( void * ) account, ( void * ) dossier );

		if( account_do_delete(
					account,
					ofo_dossier_get_dbms( dossier ))){

			OFA_IDATASET_REMOVE( dossier, ACCOUNT, account );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
account_do_delete( ofoAccount *account, const ofaDbms *dbms )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_ACCOUNTS"
			"	WHERE ACC_NUMBER='%s'",
					ofo_account_get_number( account ));

	ok = ofa_dbms_query( dbms, query, TRUE );

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
	return( account_cmp_by_number( a, ofo_account_get_number( b )));
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/*
 * iexportable_export:
 *
 * Exports the accounts line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GSList *lines;
	gchar *str;
	GList *it;
	gchar field_sep, decimal_sep;
	gboolean with_headers, ok;
	gulong count;

	OFA_IDATASET_GET( dossier, ACCOUNT, account );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );
	decimal_sep = ofa_file_format_get_decimal_sep( settings );

	count = ( gulong ) g_list_length( account_dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=account_dataset ; it ; it=it->next ){
		str = ofa_box_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, decimal_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	return( TRUE );
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

/*
 * ofo_account_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - account number
 * - label
 * - currency iso 3a code (mandatory for detail accounts, default to
 *   dossier currency)
 * - type = {D|R} (default to Detail)
 * - settleable = {S| } (defaults to no)
 * - reconciliable = {R| } (defaults to no)
 * - carried forwardable on new exercice = {F| } (defaults to no)
 * - notes (opt)
 *
 * Replace the whole table with the provided datas.
 * All the balances are set to NULL.
 */
static gint
iimportable_import( ofaIImportable *importable, GSList *lines, ofoDossier *dossier )
{
	GSList *itl, *fields, *itf;
	const gchar *def_dev_code;
	ofoAccount *account;
	GList *dataset, *it;
	guint errors, class_num, line;
	gchar *msg, *splitted, *dev_code;
	ofoCurrency *currency;
	ofoClass *class_obj;
	gchar *str;

	line = 0;
	errors = 0;
	dataset = NULL;
	def_dev_code = ofo_dossier_get_default_currency( dossier );

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		account = ofo_account_new();
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		/* account number */
		itf = fields;
		str = ofa_iimportable_get_string( &itf );
		if( !str ){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty account number" ));
			errors += 1;
			continue;
		}
		class_num = ofo_account_get_class_from_number( str );
		class_obj = ofo_class_get_by_number( dossier, class_num );
		if( !class_obj && !OFO_IS_CLASS( class_obj )){
			msg = g_strdup_printf( _( "invalid account class number: %s" ), str );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			g_free( str );
			errors += 1;
			continue;
		}
		ofo_account_set_number( account, str );
		g_free( str );

		/* account label */
		str = ofa_iimportable_get_string( &itf );
		if( !str ){
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, _( "empty account label" ));
			errors += 1;
			continue;
		}
		ofo_account_set_label( account, str );

		/* currency code */
		str = ofa_iimportable_get_string( &itf );
		if( str ){
			dev_code = g_strdup( str );
		} else {
			dev_code = g_strdup( def_dev_code );
		}
		g_free( str );
		currency = ofo_currency_get_by_code( dossier, dev_code );
		if( !currency ){
			msg = g_strdup_printf( _( "invalid account currency: %s" ), dev_code );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			g_free( dev_code );
			errors += 1;
			continue;
		}
		ofo_account_set_currency( account, dev_code );
		g_free( dev_code );

		/* account type */
		str = ofa_iimportable_get_string( &itf );
		if( !str ){
			str = g_strdup( ACCOUNT_TYPE_DETAIL );
		} else if( g_utf8_collate( str, ACCOUNT_TYPE_DETAIL ) && g_utf8_collate( str, ACCOUNT_TYPE_ROOT )){
			msg = g_strdup_printf( _( "invalid account type: %s" ), str );
			ofa_iimportable_set_message(
					importable, line, IMPORTABLE_MSG_ERROR, msg );
			g_free( msg );
			g_free( str );
			errors += 1;
			continue;
		}
		ofo_account_set_type_account( account, str );
		g_free( str );

		/* settleable ? */
		str = ofa_iimportable_get_string( &itf );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_SETTLEABLE )){
				msg = g_strdup_printf( _( "invalid account settleable indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_settleable( account, TRUE );
			}
		}
		g_free( str );

		/* reconciliable ? */
		str = ofa_iimportable_get_string( &itf );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_RECONCILIABLE )){
				msg = g_strdup_printf( _( "invalid account reconciliable indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_reconciliable( account, TRUE );
			}
		}
		g_free( str );

		/* carried forwardable ? */
		str = ofa_iimportable_get_string( &itf );
		if( str ){
			if( g_utf8_collate( str, ACCOUNT_FORWARDABLE )){
				msg = g_strdup_printf( _( "invalid account forwardable indicator: %s" ), str );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				g_free( str );
				errors += 1;
				continue;
			} else {
				ofo_account_set_forward( account, TRUE );
			}
		}
		g_free( str );

		/* notes
		 * we are tolerant on the last field... */
		str = ofa_iimportable_get_string( &itf );
		splitted = my_utils_import_multi_lines( str );
		ofo_account_set_notes( account, splitted );
		g_free( splitted );
		g_free( str );

		dataset = g_list_prepend( dataset, account );
	}

	if( !errors ){
		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_ACCOUNT, FALSE );

		account_do_drop_content( ofo_dossier_get_dbms( dossier ));

		for( it=dataset ; it ; it=it->next ){
			account_do_insert(
					OFO_ACCOUNT( it->data ),
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ));

			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
		}

		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
		ofa_idataset_free_dataset( dossier, OFO_TYPE_ACCOUNT );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_ACCOUNT );

		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_ACCOUNT, TRUE );
	}

	return( errors );
}

static gboolean
account_do_drop_content( const ofaDbms *dbms )
{
	return( ofa_dbms_query( dbms, "DELETE FROM OFA_T_ACCOUNTS", TRUE ));
}
