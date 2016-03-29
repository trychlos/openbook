/*
 * A double-entry accounting application for freelances.
 *
 * Open Freelance Accounting
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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

/* priv instance data
 */
enum {
	DOS_DEF_CURRENCY = 1,
	DOS_EXE_BEGIN,
	DOS_EXE_END,
	DOS_EXE_LENGTH,
	DOS_EXE_NOTES,
	DOS_FORW_OPE,
	DOS_IMPORT_LEDGER,
	DOS_LABEL,
	DOS_NOTES,
	DOS_SIREN,
	DOS_SIRET,
	DOS_SLD_OPE,
	DOS_UPD_USER,
	DOS_UPD_STAMP,
	DOS_LAST_BAT,
	DOS_LAST_BATLINE,
	DOS_LAST_ENTRY,
	DOS_LAST_SETTLEMENT,
	DOS_LAST_CONCIL,
	DOS_CURRENT,
	DOS_LAST_CLOSING,
	DOS_PREVEXE_ENTRY,
	DOS_CURRENCY,
	DOS_SLD_ACCOUNT,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( DOS_DEF_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( DOS_EXE_BEGIN ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_EXE_END ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_EXE_LENGTH ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_EXE_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_FORW_OPE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_IMPORT_LEDGER ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_SIREN ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_SIRET ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_SLD_OPE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LAST_CLOSING ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_CURRENT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
										/* below data are not imported */
		{ OFA_BOX_CSV( DOS_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LAST_BAT ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LAST_BATLINE ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LAST_CONCIL ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LAST_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LAST_SETTLEMENT ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_PREVEXE_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_currency_defs[] = {
		{ OFA_BOX_CSV( DOS_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( DOS_SLD_ACCOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

typedef struct {
	GList         *cur_details;			/* a list of details per currency */
}
	ofoDossierPrivate;

static void        on_hub_updated_object( const ofaHub *hub, ofoBase *object, const gchar *prev_id, ofoDossier *dossier );
static void        on_updated_object_currency_code( const ofaHub *hub, const gchar *prev_id, const gchar *code );
static void        on_hub_exe_dates_changed( const ofaHub *hub, const GDate *prev_begin, const GDate *prev_end, ofoDossier *dossier );
static gint        dossier_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo );
static gint        dossier_cur_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo );
static void        dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number );
static GList      *dossier_find_currency_by_code( ofoDossier *dossier, const gchar *currency );
static GList      *dossier_new_currency_with_code( ofoDossier *dossier, const gchar *currency );
static void        dossier_set_upd_user( ofoDossier *dossier, const gchar *user );
static void        dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp );
static void        dossier_set_last_bat( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_batline( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_entry( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_settlement( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_concil( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_prev_exe_last_entry( ofoDossier *dossier, ofxCounter counter );
static ofoDossier *dossier_do_read( ofaHub *hub );
static gboolean    dossier_do_update( ofoDossier *dossier );
static gboolean    do_update_properties( ofoDossier *dossier );
static gboolean    dossier_do_update_currencies( ofoDossier *dossier );
static gboolean    do_update_currency_properties( ofoDossier *dossier );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static gchar      *iexportable_get_label( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static void        free_currency_details( ofoDossier *dossier );
static void        free_cur_detail( GList *fields );

G_DEFINE_TYPE_EXTENDED( ofoDossier, ofo_dossier, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoDossier )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init ))

static void
dossier_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_DOSSIER( instance ));

	/* free data members here */
	free_currency_details( OFO_DOSSIER( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->finalize( instance );
}

static void
dossier_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_dispose";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->dispose( instance );
}

static void
ofo_dossier_init( ofoDossier *self )
{
	static const gchar *thisfn = "ofo_dossier_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_finalize;
}

/**
 * ofo_dossier_new:
 * @hub: the #ofaHub object which will manage this dossier.
 *
 * Instanciates a new object, and initializes it with data read from
 * database.
 *
 * Returns: a newly allocated #ofoDossier object, or %NULL if an error
 * has occured.
 */
ofoDossier *
ofo_dossier_new( ofaHub *hub )
{
	ofoDossier *dossier;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	dossier = dossier_do_read( hub );
	if( dossier ){
		g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), dossier );
		g_signal_connect( hub, SIGNAL_HUB_EXE_DATES_CHANGED, G_CALLBACK( on_hub_exe_dates_changed ), dossier );
	}

	return( dossier );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( const ofaHub *hub, ofoBase *object, const gchar *prev_id, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_on_hub_updated_object";
	const gchar *code;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, dossier=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) dossier );

	if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( hub, prev_id, code );
			}
		}
	}
}

static void
on_updated_object_currency_code( const ofaHub *hub, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_DEF_CURRENCY='%s' WHERE DOS_DEF_CURRENCY='%s'", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}

/*
 * SIGNAL_HUB_EXE_DATES_CHANGED signal handler
 *
 * Changing beginning or ending exercice dates is only possible for
 * the current exercice.
 */
static void
on_hub_exe_dates_changed( const ofaHub *hub, const GDate *prev_begin, const GDate *prev_end, ofoDossier *dossier )
{
	const ofaIDBConnect *connect;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;

	connect = ofa_hub_get_connect( hub );
	meta = ofa_idbconnect_get_meta( connect );
	period = ofa_idbconnect_get_period( connect );

	ofa_idbmeta_update_period( meta, period,
			TRUE, ofo_dossier_get_exe_begin( dossier ), ofo_dossier_get_exe_end( dossier ));

	g_object_unref( period );
	g_object_unref( meta );
}

/**
 * ofo_dossier_use_account:
 *
 * Returns: %TRUE if the dossier makes use of this account, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_account( const ofoDossier *dossier, const gchar *account )
{
	gint count;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	count = dossier_cur_count_uses( dossier, "DOS_SLD_ACCOUNT", account );
	return( count > 0 );
}

/**
 * ofo_dossier_use_currency:
 *
 * Returns: %TRUE if the dossier makes use of this currency, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_currency( const ofoDossier *dossier, const gchar *currency )
{
	const gchar *default_dev;
	gint count;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	default_dev = ofo_dossier_get_default_currency( dossier );

	if( my_strlen( default_dev ) &&
			!g_utf8_collate( default_dev, currency )){
		return( TRUE );
	}

	count = dossier_cur_count_uses( dossier, "DOS_CURRENCY", currency );
	return( count > 0 );
}

/**
 * ofo_dossier_use_ledger:
 *
 * Returns: %TRUE if the dossier makes use of this ledger, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_ledger( const ofoDossier *dossier, const gchar *ledger )
{
	const gchar *import_ledger;
	gint cmp;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	import_ledger = ofo_dossier_get_import_ledger( dossier );
	if( my_strlen( import_ledger )){
		cmp = g_utf8_collate( ledger, import_ledger );
		return( cmp == 0 );
	}

	return( FALSE );
}

/**
 * ofo_dossier_use_ope_template:
 *
 * Returns: %TRUE if the dossier makes use of this operation template,
 * thus preventing its deletion.
 */
gboolean
ofo_dossier_use_ope_template( const ofoDossier *dossier, const gchar *ope_template )
{
	gint forward, solde;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	forward = dossier_count_uses( dossier, "DOS_FORW_OPE", ope_template );
	solde = dossier_count_uses( dossier, "DOS_SLD_OPE", ope_template );

	return( forward+solde > 0 );
}

static gint
dossier_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	hub = ofo_base_get_hub( OFO_BASE( dossier ));

	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DOSSIER WHERE %s='%s' AND DOS_ID=%d",
					field, mnemo, DOSSIER_ROW_ID );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count );
}

static gint
dossier_cur_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	hub = ofo_base_get_hub( OFO_BASE( dossier ));

	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DOSSIER_CUR WHERE %s='%s' AND DOS_ID=%d",
					field, mnemo, DOSSIER_ROW_ID );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_dossier_get_default_currency:
 *
 * Returns: the default currency of the dossier.
 */
const gchar *
ofo_dossier_get_default_currency( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_DEF_CURRENCY );
}

/**
 * ofo_dossier_get_exe_begin:
 *
 * Returns: the date of the beginning of the exercice.
 */
const GDate *
ofo_dossier_get_exe_begin( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, date, NULL, DOS_EXE_BEGIN );
}

/**
 * ofo_dossier_get_exe_end:
 *
 * Returns: the date of the end of the exercice.
 */
const GDate *
ofo_dossier_get_exe_end( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, date, NULL, DOS_EXE_END );
}

/**
 * ofo_dossier_get_exe_length:
 *
 * Returns: the length of the exercice, in months.
 */
gint
ofo_dossier_get_exe_length( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, int, 0, DOS_EXE_LENGTH );
}

/**
 * ofo_dossier_get_exe_notes:
 *
 * Returns: the notes associated to the exercice.
 */
const gchar *
ofo_dossier_get_exe_notes( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_EXE_NOTES );
}

/**
 * ofo_dossier_get_forward_ope:
 *
 * Returns: the forward ope of the dossier.
 */
const gchar *
ofo_dossier_get_forward_ope( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_FORW_OPE );
}

/**
 * ofo_dossier_get_import_ledger:
 *
 * Returns: the default import ledger of the dossier.
 */
const gchar *
ofo_dossier_get_import_ledger( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_IMPORT_LEDGER );
}

/**
 * ofo_dossier_get_label:
 *
 * Returns: the label of the dossier. This is the 'raison sociale' for
 * the dossier.
 */
const gchar *
ofo_dossier_get_label( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_LABEL );
}

/**
 * ofo_dossier_get_notes:
 *
 * Returns: the notes attached to the dossier.
 */
const gchar *
ofo_dossier_get_notes( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_NOTES );
}

/**
 * ofo_dossier_get_siren:
 *
 * Returns: the siren of the dossier.
 */
const gchar *
ofo_dossier_get_siren( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_SIREN );
}

/**
 * ofo_dossier_get_siret:
 *
 * Returns: the siret of the dossier.
 */
const gchar *
ofo_dossier_get_siret( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_SIRET );
}

/**
 * ofo_dossier_get_sld_ope:
 *
 * Returns: the sld ope of the dossier.
 */
const gchar *
ofo_dossier_get_sld_ope( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_SLD_OPE );
}

/**
 * ofo_dossier_get_upd_user:
 *
 * Returns: the identifier of the user who has last updated the
 * properties of the dossier.
 */
const gchar *
ofo_dossier_get_upd_user( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_UPD_USER );
}

/**
 * ofo_dossier_get_stamp:
 *
 * Returns: the timestamp when a user has last updated the properties
 * of the dossier.
 */
const GTimeVal *
ofo_dossier_get_upd_stamp( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, timestamp, NULL, DOS_UPD_STAMP );
}

/**
 * ofo_dossier_get_status:
 *
 * Returns: the status of the dossier as a const string suitable for
 * display.
 */
const gchar *
ofo_dossier_get_status( const ofoDossier *dossier )
{
	gboolean is_current;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, NULL );

	is_current = ofo_dossier_is_current( dossier );

	return( is_current ? _( "Opened" ) : _( "Archived" ));
}

/**
 * ofo_dossier_get_last_bat:
 *
 * Returns: the last bat number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_bat( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_LAST_BAT );
}

/**
 * ofo_dossier_get_last_batline:
 *
 * Returns: the last bat_line number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_batline( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_LAST_BATLINE );
}

/**
 * ofo_dossier_get_last_entry:
 *
 * Returns: the last entry number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_entry( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_LAST_ENTRY );
}

/**
 * ofo_dossier_get_last_settlement:
 *
 * Returns: the last settlement number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_settlement( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_LAST_SETTLEMENT );
}

/**
 * ofo_dossier_get_last_concil:
 *
 * Returns: the last reconciliation id. allocated.
 */
ofxCounter
ofo_dossier_get_last_concil( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_LAST_CONCIL );
}

/**
 * ofo_dossier_get_next_bat:
 */
ofxCounter
ofo_dossier_get_next_bat( ofoDossier *dossier )
{
	ofxCounter last, next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	last = ofo_dossier_get_last_bat( dossier );
	next = last+1;
	dossier_set_last_bat( dossier, next );
	dossier_update_next( dossier, "DOS_LAST_BAT", next );

	return( next );
}

/**
 * ofo_dossier_get_next_batline:
 */
ofxCounter
ofo_dossier_get_next_batline( ofoDossier *dossier )
{
	ofxCounter last, next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	last = ofo_dossier_get_last_batline( dossier );
	next = last+1;
	dossier_set_last_batline( dossier, next );
	dossier_update_next( dossier, "DOS_LAST_BATLINE", next );

	return( next );
}

/**
 * ofo_dossier_get_next_entry:
 *
 * Returns: the next entry number to be allocated in the dossier.
 */
ofxCounter
ofo_dossier_get_next_entry( ofoDossier *dossier )
{
	ofxCounter last, next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	last = ofo_dossier_get_last_entry( dossier );
	next = last+1;
	dossier_set_last_entry( dossier, next );
	dossier_update_next( dossier, "DOS_LAST_ENTRY", next );

	return( next );
}

/**
 * ofo_dossier_get_next_settlement:
 */
ofxCounter
ofo_dossier_get_next_settlement( ofoDossier *dossier )
{
	ofxCounter last, next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	last = ofo_dossier_get_last_settlement( dossier );
	next = last+1;
	dossier_set_last_settlement( dossier, next );
	dossier_update_next( dossier, "DOS_LAST_SETTLEMENT", next );

	return( next );
}

/**
 * ofo_dossier_get_next_concil:
 */
ofxCounter
ofo_dossier_get_next_concil( ofoDossier *dossier )
{
	ofxCounter last, next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	last = ofo_dossier_get_last_concil( dossier );
	next = last+1;
	dossier_set_last_concil( dossier, next );
	dossier_update_next( dossier, "DOS_LAST_CONCIL", next );

	return( next );
}

/*
 * ofo_dossier_update_next_number:
 */
static void
dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number )
{
	ofaHub *hub;
	gchar *query;

	hub = ofo_base_get_hub( OFO_BASE( dossier ));

	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER "
			"	SET %s=%ld "
			"	WHERE DOS_ID=%d",
					field, next_number, DOSSIER_ROW_ID );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}

/**
 * ofo_dossier_get_last_closing_date:
 * @dossier:
 *
 * Returns: the last period closing date.
 */
const GDate *
ofo_dossier_get_last_closing_date( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, date, NULL, DOS_LAST_CLOSING );
}

/**
 * ofo_dossier_get_prev_exe_last_entry:
 * @dossier:
 *
 * Returns: the last entry number of the previous exercice.
 */
ofxCounter
ofo_dossier_get_prev_exe_last_entry( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_PREVEXE_ENTRY );
}

/**
 * ofo_dossier_get_min_deffect:
 * @dossier: this dossier.
 * @ledger: [allow-none]: the imputed ledger.
 * @date: [out]: the #GDate date to be set.
 *
 * Computes the minimal effect date valid for the considered dossier and
 * ledger.
 *
 * This minimal effect date is:
 * - at least the begin of the exercice (if set)
 * - or the last ledger closing date (if set, and greater than the previous) + 1
 *
 * Returns: the @date #GDate pointer.
 */
GDate *
ofo_dossier_get_min_deffect( const ofoDossier *dossier, const ofoLedger *ledger, GDate *date )
{
	const GDate *last_clo;
	gint to_add;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, NULL );

	if( ledger ){
		g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );
	}

	last_clo = ledger ? ofo_ledger_get_last_close( ledger ) : NULL;
	my_date_set_from_date( date, ofo_dossier_get_exe_begin( dossier ));
	to_add = 0;

	if( my_date_is_valid( date )){
		if( my_date_is_valid( last_clo )){
			if( my_date_compare( date, last_clo ) < 0 ){
				my_date_set_from_date( date, last_clo );
				to_add = 1;
			}
		}
	} else if( my_date_is_valid( last_clo )){
		my_date_set_from_date( date, last_clo );
		to_add = 1;
	}

	if( my_date_is_valid( date )){
		g_date_add_days( date, to_add );
	}

	return( date );
}

/**
 * ofo_dossier_get_currencies:
 * @dossier: this dossier.
 *
 * Returns: an alphabetically sorted #GSList of the currencies defined
 * in the subtable.
 *
 * The returned list should be #ofo_dossier_free_currencies() by the
 * caller.
 */
GSList *
ofo_dossier_get_currencies( const ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	GSList *list;
	GList *it;
	GList *cur_detail;
	const gchar *currency;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, NULL );

	priv = ofo_dossier_get_instance_private( dossier );
	list = NULL;

	for( it=priv->cur_details ; it ; it=it->next ){
		cur_detail = ( GList * ) it->data;
		currency = ofa_box_get_string( cur_detail, DOS_CURRENCY );
		list = g_slist_insert_sorted( list, g_strdup( currency ), ( GCompareFunc ) g_utf8_collate );
	}

	return( list );
}

/**
 * ofo_dossier_get_sld_account:
 * @dossier: this dossier.
 * @currency: the serched currency.
 *
 * Returns: the account configured as balancing account for this
 * currency.
 *
 * The returned string is owned by the @dossier, and must not be freed
 * by the caller.
 */
const gchar *
ofo_dossier_get_sld_account( ofoDossier *dossier, const gchar *currency )
{
	GList *cur_detail;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, NULL );

	cur_detail = dossier_find_currency_by_code( dossier, currency );
	if( cur_detail ){
		return( ofa_box_get_string( cur_detail, DOS_SLD_ACCOUNT ));
	}

	return( NULL );
}

static GList *
dossier_find_currency_by_code( ofoDossier *dossier, const gchar *currency )
{
	static const gchar *thisfn = "ofo_dossier_find_currency_by_code";
	ofoDossierPrivate *priv;
	GList *it;
	GList *cur_detail;
	const gchar *cur_code;

	priv = ofo_dossier_get_instance_private( dossier );

	for( it=priv->cur_details ; it ; it=it->next ){
		cur_detail = ( GList * ) it->data;
		cur_code = ofa_box_get_string( cur_detail, DOS_CURRENCY );
		if( !g_utf8_collate( cur_code, currency )){
			return( cur_detail );
		}
	}

	g_debug( "%s: currency=%s not found", thisfn, currency );

	return( NULL );
}

static GList *
dossier_new_currency_with_code( ofoDossier *dossier, const gchar *currency )
{
	ofoDossierPrivate *priv;
	GList *cur_detail;

	cur_detail = dossier_find_currency_by_code( dossier, currency );

	if( !cur_detail ){
		cur_detail = ofa_box_init_fields_list( st_currency_defs );
		ofa_box_set_string( cur_detail, DOS_CURRENCY, currency );
		ofa_box_set_string( cur_detail, DOS_SLD_ACCOUNT, NULL );

		priv = ofo_dossier_get_instance_private( dossier );
		priv->cur_details = g_list_prepend( priv->cur_details, cur_detail );
	}

	return( cur_detail );
}

#if 0
/**
 * ofo_dossier_get_last_closed_exercice:
 *
 * Returns: the last exercice closing date, as a newly allocated
 * #GDate structure which should be g_free() by the caller,
 * or %NULL if the dossier has never been closed.
 */
GDate *
ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_last_closed_exercice";
	GList *exe;
	sDetailExe *sexe;
	GDate *dmax;
	gchar *str;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, NULL );

	dmax = NULL;

	for( exe=dossier->priv->exes ; exe ; exe=exe->next ){
		sexe = ( sDetailExe * ) exe->data;
		if( sexe->status == DOS_STATUS_CLOSED ){
			g_return_val_if_fail( my_date_is_valid( &sexe->exe_end ), NULL );
			if( dmax ){
				g_return_val_if_fail( my_date_is_valid( dmax ), NULL );
				if( my_date_compare( &sexe->exe_end, dmax ) > 0 ){
					my_date_set_from_date( dmax, &sexe->exe_end );
				}
			} else {
				dmax = g_new0( GDate, 1 );
				my_date_set_from_date( dmax, &sexe->exe_end );
			}
		}
	}

	str = my_date_to_str( dmax, ofa_prefs_date_display());
	g_debug( "%s: last_closed_exercice=%s", thisfn, str );
	g_free( str );

	return( dmax );
}
#endif

/**
 * ofo_dossier_is_current:
 *
 * Returns: %TRUE if the exercice if the current one.
 */
gboolean
ofo_dossier_is_current( const ofoDossier *dossier )
{
	const gchar *cstr;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	cstr = ofa_box_get_string( OFO_BASE( dossier )->prot->fields, DOS_CURRENT );

	return( my_utils_boolean_from_str( cstr ));
}

/**
 * ofo_dossier_is_valid_data:
 */
gboolean
ofo_dossier_is_valid_data( const gchar *label, gint nb_months, const gchar *currency,
								const GDate *begin, const GDate *end, gchar **msgerr )
{
	if( !my_strlen( label )){
		*msgerr = g_strdup( _( "Empty label" ));
		return( FALSE );
	}

	if( nb_months <= 0 ){
		*msgerr = g_strdup_printf( "Invalid length of exercice: %d", nb_months );
		return( FALSE );
	}

	if( !my_strlen( currency )){
		*msgerr = g_strdup( _( "Empty default currency"));
		return( FALSE );
	}

	if( my_date_is_valid( begin ) && my_date_is_valid( end )){
		if( my_date_compare( begin, end ) > 0 ){
			*msgerr = g_strdup( _( "Exercice is set to begin after it has ended" ));
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofo_dossier_set_default_currency:
 */
void
ofo_dossier_set_default_currency( ofoDossier *dossier, const gchar *currency )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_DEF_CURRENCY, currency );
}

/**
 * ofo_dossier_set_exe_begin:
 */
void
ofo_dossier_set_exe_begin( ofoDossier *dossier, const GDate *date )
{
	ofo_base_setter( DOSSIER, dossier, date, DOS_EXE_BEGIN, date );
}

/**
 * ofo_dossier_set_exe_end:
 */
void
ofo_dossier_set_exe_end( ofoDossier *dossier, const GDate *date )
{
	ofo_base_setter( DOSSIER, dossier, date, DOS_EXE_END, date );
}

/**
 * ofo_dossier_set_exe_length:
 */
void
ofo_dossier_set_exe_length( ofoDossier *dossier, gint nb_months )
{
	ofo_base_setter( DOSSIER, dossier, int, DOS_EXE_LENGTH, nb_months );
}

/**
 * ofo_dossier_set_exe_notes:
 * @dossier: this dossier
 * @notes : the notes to be set
 *
 * Attach the given @notes to the exercice.
 */
void
ofo_dossier_set_exe_notes( ofoDossier *dossier, const gchar *notes )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_EXE_NOTES, notes );
}

/**
 * ofo_dossier_set_forward_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_forward_ope( ofoDossier *dossier, const gchar *ope )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_FORW_OPE, ope );
}

/**
 * ofo_dossier_set_import_ledger:
 *
 * Not mandatory until importing entries.
 */
void
ofo_dossier_set_import_ledger( ofoDossier *dossier, const gchar *ledger )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_IMPORT_LEDGER, ledger );
}

/**
 * ofo_dossier_set_label:
 */
void
ofo_dossier_set_label( ofoDossier *dossier, const gchar *label )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_LABEL, label );
}

/**
 * ofo_dossier_set_notes:
 */
void
ofo_dossier_set_notes( ofoDossier *dossier, const gchar *notes )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_NOTES, notes );
}

/**
 * ofo_dossier_set_siren:
 */
void
ofo_dossier_set_siren( ofoDossier *dossier, const gchar *siren )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_SIREN, siren );
}

/**
 * ofo_dossier_set_siret:
 */
void
ofo_dossier_set_siret( ofoDossier *dossier, const gchar *siret )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_SIRET, siret );
}

/**
 * ofo_dossier_set_sld_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_sld_ope( ofoDossier *dossier, const gchar *ope )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_SLD_OPE, ope );
}

/*
 * ofo_dossier_set_upd_user:
 */
static void
dossier_set_upd_user( ofoDossier *dossier, const gchar *user )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_UPD_USER, user );
}

/*
 * ofo_dossier_set_upd_stamp:
 */
static void
dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp )
{
	ofo_base_setter( DOSSIER, dossier, timestamp, DOS_UPD_STAMP, stamp );
}

static void
dossier_set_last_bat( ofoDossier *dossier, ofxCounter counter )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_LAST_BAT, counter );
}

static void
dossier_set_last_batline( ofoDossier *dossier, ofxCounter counter )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_LAST_BATLINE, counter );
}

static void
dossier_set_last_entry( ofoDossier *dossier, ofxCounter counter )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_LAST_ENTRY, counter );
}

static void
dossier_set_last_settlement( ofoDossier *dossier, ofxCounter counter )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_LAST_SETTLEMENT, counter );
}

static void
dossier_set_last_concil( ofoDossier *dossier, ofxCounter counter )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_LAST_CONCIL, counter );
}

/**
 * ofo_dossier_set_current:
 * @dossier: this #ofoDossier instance.
 * @current: %TRUE if this @dossier period is opened, %FALSE if it is
 *  archived.
 *
 * Set the status of the financial period.
 */
void
ofo_dossier_set_current( ofoDossier *dossier, gboolean current )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_CURRENT, current ? "Y":"N" );
}

/**
 * ofo_dossier_set_last_closing_date:
 * @dossier:
 * @last_closing: the last period closing date.
 */
void
ofo_dossier_set_last_closing_date( ofoDossier *dossier, const GDate *last_closing )
{
	ofo_base_setter( DOSSIER, dossier, date, DOS_LAST_CLOSING, last_closing );
}

/**
 * ofo_dossier_set_prev_exe_last_entry:
 * @dossier:
 */
void
ofo_dossier_set_prev_exe_last_entry( ofoDossier *dossier )
{
	ofxCounter last;

	last = ofo_dossier_get_last_entry( dossier );
	dossier_set_prev_exe_last_entry( dossier, last );
}

/*
 * dossier_set_prev_exe_last_entry:
 * @dossier:
 * @last_entry:
 */
static void
dossier_set_prev_exe_last_entry( ofoDossier *dossier, ofxCounter last_entry )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_PREVEXE_ENTRY, last_entry );
}

/**
 * ofo_dossier_reset_currencies:
 * @dossier: this dossier.
 *
 * Reset the currencies (free all).
 *
 * This function should be called when updating the currencies
 * properties, as we are only able to set new elements in the list.
 */
void
ofo_dossier_reset_currencies( ofoDossier *dossier )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run );

	free_currency_details( dossier );
}

/**
 * ofo_dossier_set_sld_account:
 * @dossier: this dossier.
 * @currency: the currency.
 * @account: the account.
 *
 * Set the balancing account for the currency.
 */
void
ofo_dossier_set_sld_account( ofoDossier *dossier, const gchar *currency, const gchar *account )
{
	GList *cur_detail;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( my_strlen( currency ));
	g_return_if_fail( my_strlen( account ));
	g_return_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run );

	cur_detail = dossier_new_currency_with_code( dossier, currency );
	g_return_if_fail( cur_detail );

	ofa_box_set_string( cur_detail, DOS_SLD_ACCOUNT, account );
}

static ofoDossier *
dossier_do_read( ofaHub *hub )
{
	ofoDossier *dossier;
	ofoDossierPrivate *priv;
	gchar *where;
	GList *list;

	dossier = NULL;

	where = g_strdup_printf( "OFA_T_DOSSIER WHERE DOS_ID=%d", DOSSIER_ROW_ID );
	list = ofo_base_load_dataset(
					st_boxed_defs,
					where,
					OFO_TYPE_DOSSIER,
					hub );
	g_free( where );

	if( g_list_length( list ) > 0 ){
		dossier = OFO_DOSSIER( list->data );
	}
	g_list_free( list );

	priv = ofo_dossier_get_instance_private( dossier );
	where = g_strdup_printf(
				"OFA_T_DOSSIER_CUR WHERE DOS_ID=%d ORDER BY DOS_CURRENCY ASC", DOSSIER_ROW_ID );
	priv->cur_details =
				ofo_base_load_rows( st_currency_defs, ofa_hub_get_connect( hub ), where );
	g_free( where );

	return( dossier );
}

/**
 * ofo_dossier_update:
 *
 * Update the properties of the dossier.
 */
gboolean
ofo_dossier_update( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update";

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	return( dossier_do_update( dossier ));
}

static gboolean
dossier_do_update( ofoDossier *dossier )
{
	return( do_update_properties( dossier ));
}

static gboolean
do_update_properties( ofoDossier *dossier )
{
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *label, *notes, *stamp_str, *sdate, *userid;
	GTimeVal stamp;
	gboolean ok, current;
	const gchar *cstr;
	const GDate *date;
	ofxCounter number;

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( dossier ));
	connect = ofa_hub_get_connect( hub );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_DOSSIER SET " );

	cstr = ofo_dossier_get_default_currency( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_DEF_CURRENCY='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_DEF_CURRENCY=NULL," );
	}

	date = ofo_dossier_get_exe_begin( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_EXE_BEGIN='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_EXE_BEGIN=NULL," );
	}

	date = ofo_dossier_get_exe_end( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_EXE_END='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_EXE_END=NULL," );
	}

	g_string_append_printf( query,
			"DOS_EXE_LENGTH=%d,", ofo_dossier_get_exe_length( dossier ));

	notes = my_utils_quote_single( ofo_dossier_get_exe_notes( dossier ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "DOS_EXE_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_EXE_NOTES=NULL," );
	}
	g_free( notes );

	cstr = ofo_dossier_get_forward_ope( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_FORW_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_FORW_OPE=NULL," );
	}

	cstr = ofo_dossier_get_import_ledger( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_IMPORT_LEDGER='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_IMPORT_LEDGER=NULL," );
	}

	label = my_utils_quote_single( ofo_dossier_get_label( dossier ));
	if( my_strlen( label )){
		g_string_append_printf( query, "DOS_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "DOS_LABEL=NULL," );
	}
	g_free( label );

	notes = my_utils_quote_single( ofo_dossier_get_notes( dossier ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "DOS_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_NOTES=NULL," );
	}
	g_free( notes );

	cstr = ofo_dossier_get_siren( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SIREN='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SIREN=NULL," );
	}

	cstr = ofo_dossier_get_siret( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SIRET='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SIRET=NULL," );
	}

	cstr = ofo_dossier_get_sld_ope( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SLD_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SLD_OPE=NULL," );
	}

	date = ofo_dossier_get_last_closing_date( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_LAST_CLOSING='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_LAST_CLOSING=NULL," );
	}

	number = ofo_dossier_get_prev_exe_last_entry( dossier );
	if( number > 0 ){
		g_string_append_printf( query, "DOS_PREVEXE_ENTRY=%ld,", number );
	} else {
		query = g_string_append( query, "DOS_PREVEXE_ENTRY=NULL," );
	}

	current = ofo_dossier_is_current( dossier );
	g_string_append_printf( query, "DOS_CURRENT='%s',", current ? "Y":"N" );

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query,
			"DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' ", userid, stamp_str );
	g_free( stamp_str );

	g_string_append_printf( query, "WHERE DOS_ID=%d", DOSSIER_ROW_ID );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		dossier_set_upd_user( dossier, userid );
		dossier_set_upd_stamp( dossier, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( userid );

	return( ok );
}

/**
 * ofo_dossier_update_currencies:
 *
 * Update the currency properties of the dossier.
 */
gboolean
ofo_dossier_update_currencies( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update_currencies";

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, FALSE );

	return( dossier_do_update_currencies( dossier ));
}

static gboolean
dossier_do_update_currencies( ofoDossier *dossier )
{
	return( do_update_currency_properties( dossier ));
}

static gboolean
do_update_currency_properties( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query, *userid;
	GList *details;
	gboolean ok;
	GList *it;
	GTimeVal stamp;
	gchar *stamp_str;
	gint count;

	priv = ofo_dossier_get_instance_private( dossier );
	hub = ofo_base_get_hub( OFO_BASE( dossier ));
	connect = ofa_hub_get_connect( hub );
	userid = ofa_idbconnect_get_account( connect );

	ok = ofa_idbconnect_query( connect, "DELETE FROM OFA_T_DOSSIER_CUR", TRUE );
	count = 0;

	if( ok ){
		for( it=priv->cur_details ; it && ok ; it=it->next ){
			details = ( GList * ) it->data;
			query = g_strdup_printf(
					"INSERT INTO OFA_T_DOSSIER_CUR (DOS_ID,DOS_CURRENCY,DOS_SLD_ACCOUNT) VALUES "
					"	(%d,'%s','%s')",
					DOSSIER_ROW_ID,
					ofa_box_get_string( details, DOS_CURRENCY ),
					ofa_box_get_string( details, DOS_SLD_ACCOUNT ));
			ok &= ofa_idbconnect_query( connect, query, TRUE );
			g_free( query );
			count += 1;
		}

		if( ok && count ){
			my_utils_stamp_set_now( &stamp );
			stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
			query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER SET "
					"	DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' "
					"	WHERE DOS_ID=%d", userid, stamp_str, DOSSIER_ROW_ID );
			g_free( stamp_str );

			if( !ofa_idbconnect_query( connect, query, TRUE )){
				ok = FALSE;
			} else {
				dossier_set_upd_user( dossier, userid );
				dossier_set_upd_stamp( dossier, &stamp );
			}
		}
	}

	g_free( userid );

	return( ok );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_dossier_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_label = iexportable_get_label;
	iface->export = iexportable_export;
}

static gchar *
iexportable_get_label( const ofaIExportable *instance )
{
	return( g_strdup( _( "_Dossier" )));
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	ofoDossier *dossier;
	ofoDossierPrivate *priv;
	GList *cur_detail;
	GSList *lines;
	gchar *str;
	gboolean ok, with_headers;
	gulong count;
	gchar field_sep;

	dossier = ofa_hub_get_dossier( hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	priv = ofo_dossier_get_instance_private( dossier );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );

	count = 1;
	if( with_headers ){
		count += 2;
	}
	count += g_list_length( priv->cur_details );
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_csv_get_header( st_boxed_defs, settings );
		lines = g_slist_prepend( NULL, g_strdup_printf( "1%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_csv_get_header( st_currency_defs, settings );
		lines = g_slist_prepend( NULL, g_strdup_printf( "2%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	str = ofa_box_csv_get_line( OFO_BASE( dossier )->prot->fields, settings );
	lines = g_slist_prepend( NULL, g_strdup_printf( "1%c%s", field_sep, str ));
	g_free( str );
	ok = ofa_iexportable_export_lines( exportable, lines );
	g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	if( !ok ){
		return( FALSE );
	}

	for( cur_detail=priv->cur_details ; cur_detail ; cur_detail=cur_detail->next ){
		str = ofa_box_csv_get_line( cur_detail->data, settings );
		lines = g_slist_prepend( NULL, g_strdup_printf( "2%c%s", field_sep, str ));
		g_free( str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	return( TRUE );
}

static void
free_currency_details( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	priv = ofo_dossier_get_instance_private( dossier );

	g_list_free_full( priv->cur_details, ( GDestroyNotify ) free_cur_detail );
	priv->cur_details = NULL;
}

static void
free_cur_detail( GList *fields )
{
	ofa_box_free_fields_list( fields );
}
