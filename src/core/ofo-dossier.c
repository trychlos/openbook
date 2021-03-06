/*
 * A double-entry accounting application for professional services.
 *
 * Open Firm Accounting
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-idoc.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iexporter.h"
#include "api/ofa-igetter.h"
#include "api/ofa-isignalable.h"
#include "api/ofa-isignaler.h"
#include "api/ofa-prefs.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-counters.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

/* priv instance data
 */
enum {
	DOS_ID = 1,
	DOS_CRE_USER,
	DOS_CRE_STAMP,
	DOS_DEF_CURRENCY,
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
	DOS_CURRENT,
	DOS_LAST_CLOSING,
	DOS_PREVEXE_ENTRY,
	DOS_PREVEXE_END,
	DOS_CURRENCY,
	DOS_SLD_ACCOUNT,
	DOS_RPID,
	DOS_VATIC,
	DOS_NAF,
	DOS_LABEL2,
	DOS_DOC_ID,
	DOS_PREF_KEY,
	DOS_PREF_VALUE,
	DOS_IDS_KEY,
	DOS_IDS_LAST,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order.
 * So:
 * 1/ the class default import should expect these fields in this same
 *    order.
 * 2/ new datas should be added to the end of the list.
 * 3/ a removed column should be replaced by an empty one to stay
 *    compatible with the class default import.
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( DOS_ID ),
				OFA_TYPE_INTEGER,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( DOS_CRE_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_CRE_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_DEF_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
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
		{ OFA_BOX_CSV( DOS_RPID ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_PREVEXE_ENTRY ),
				OFA_TYPE_COUNTER,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_PREVEXE_END ),
				OFA_TYPE_DATE,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( DOS_VATIC ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_NAF ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_LABEL2 ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_currency_defs[] = {
		{ OFA_BOX_CSV( DOS_ID ),
				OFA_TYPE_INTEGER,
				TRUE,					/* importable */
				FALSE },				/* amount, counter: export zero as empty */
		{ OFA_BOX_CSV( DOS_CURRENCY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_SLD_ACCOUNT ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_doc_defs[] = {
		{ OFA_BOX_CSV( DOS_ID ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_DOC_ID ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_pref_defs[] = {
		{ OFA_BOX_CSV( DOS_ID ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_PREF_KEY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_PREF_VALUE ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ 0 }
};

static const ofsBoxDef st_id_defs[] = {
		{ OFA_BOX_CSV( DOS_ID ),
				OFA_TYPE_INTEGER,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_IDS_KEY ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( DOS_IDS_LAST ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

#define DOSSIER_TABLES_COUNT            5
#define DOSSIER_EXPORT_VERSION          2

typedef struct {
	GList *cur_details;					/* a list of solde accounts per currency */
	GList *docs;
	GList *prefs;
}
	ofoDossierPrivate;

static void        dossier_set_upd_user( ofoDossier *dossier, const gchar *user );
static void        dossier_set_upd_stamp( ofoDossier *dossier, const myStampVal *stamp );
static void        dossier_setup_rpid( ofoDossier *self );
static GList      *dossier_find_currency_by_code( ofoDossier *dossier, const gchar *currency );
static GList      *dossier_find_currency_by_account( ofoDossier *dossier, const gchar *account );
static GList      *dossier_new_currency_with_code( ofoDossier *dossier, const gchar *currency );
static GList      *get_orphans( ofaIGetter *getter, const gchar *table );
static ofoDossier *dossier_do_read( ofaIGetter *getter );
static gboolean    dossier_do_update( ofoDossier *dossier );
static gboolean    do_update_properties( ofoDossier *dossier );
static gboolean    dossier_do_update_currencies( ofoDossier *dossier );
static gboolean    do_update_currency_properties( ofoDossier *dossier );
static void        idoc_iface_init( ofaIDocInterface *iface );
static guint       idoc_get_interface_version( void );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( void );
static gchar      *iexportable_get_label( const ofaIExportable *instance );
static gboolean    iexportable_get_published( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const gchar *format_id );
static gboolean    iexportable_export_default( ofaIExportable *exportable );
static void        free_currency_details( ofoDossier *dossier );
static void        free_cur_detail( GList *fields );
static void        isignalable_iface_init( ofaISignalableInterface *iface );
static void        isignalable_connect_to( ofaISignaler *signaler );
static gboolean    signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty );
static gboolean    signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account );
static gboolean    signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency );
static gboolean    signaler_is_deletable_ledger( ofaISignaler *signaler, ofoLedger *ledger );
static gboolean    signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template );
static void        signaler_on_exe_dates_changed( ofaISignaler *signaler, const GDate *prev_begin, const GDate *prev_end, void *empty );
static void        signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty );
static void        signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *prev_id, const gchar *new_id );
static void        signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code );
static void        signaler_on_updated_ledger_mnemo( ofaISignaler *signaler, const gchar *prev_mnemo, const gchar *new_mnemo );
static void        signaler_on_updated_ope_template_mnemo( ofaISignaler *signaler, const gchar *prev_mnemo, const gchar *new_mnemo );

G_DEFINE_TYPE_EXTENDED( ofoDossier, ofo_dossier, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoDossier )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IDOC, idoc_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IEXPORTABLE, iexportable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ISIGNALABLE, isignalable_iface_init ))

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
	ofoDossierPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_dossier_get_instance_private( self );

	priv->cur_details = NULL;
	priv->docs = NULL;
	priv->prefs = NULL;
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
 * @getter: a #ofaIGetter instance.
 *
 * Instanciates a new object, and initializes it with data read from
 * database.
 *
 * Returns: a newly allocated #ofoDossier object, or %NULL if an error
 * has occured.
 */
ofoDossier *
ofo_dossier_new( ofaIGetter *getter )
{
	ofoDossier *dossier;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	dossier = dossier_do_read( getter );

	return( dossier );
}

/**
 * ofo_dossier_get_cre_user:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the identifier of the user who has last updated the
 * properties of the dossier.
 */
const gchar *
ofo_dossier_get_cre_user( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_CRE_USER );
}

/**
 * ofo_dossier_get_cre_stamp:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the timestamp when a user has last updated the properties
 * of the dossier.
 */
const myStampVal *
ofo_dossier_get_cre_stamp( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, timestamp, NULL, DOS_CRE_STAMP );
}

/**
 * ofo_dossier_get_default_currency:
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the forward ope of the dossier.
 */
const gchar *
ofo_dossier_get_forward_ope( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_FORW_OPE );
}

/**
 * ofo_dossier_get_sld_ope:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the sld ope of the dossier.
 */
const gchar *
ofo_dossier_get_sld_ope( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_SLD_OPE );
}

/**
 * ofo_dossier_get_import_ledger:
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
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
 * ofo_dossier_get_label2:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the 2nd label of the dossier.
 * This is expected to be a sub-line of the 'raison sociale'.
 */
const gchar *
ofo_dossier_get_label2( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_LABEL2 );
}

/**
 * ofo_dossier_get_notes:
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
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
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the siret of the dossier.
 */
const gchar *
ofo_dossier_get_siret( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_SIRET );
}

/**
 * ofo_dossier_get_vatic:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the VAT identifier of the dossier.
 */
const gchar *
ofo_dossier_get_vatic( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_VATIC );
}

/**
 * ofo_dossier_get_naf:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the NAF identifier of the dossier.
 */
const gchar *
ofo_dossier_get_naf( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_NAF );
}

/**
 * ofo_dossier_get_upd_user:
 * @dossier: this #ofoDossier instance.
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
 * ofo_dossier_get_upd_stamp:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the timestamp when a user has last updated the properties
 * of the dossier.
 */
const myStampVal *
ofo_dossier_get_upd_stamp( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, timestamp, NULL, DOS_UPD_STAMP );
}

/**
 * ofo_dossier_get_status:
 * @dossier: this #ofoDossier instance.
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
 * ofo_dossier_get_last_closing_date:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the last period closing date.
 */
const GDate *
ofo_dossier_get_last_closing_date( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, date, NULL, DOS_LAST_CLOSING );
}

/**
 * ofo_dossier_get_prevexe_last_entry:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the last entry number of the previous exercice.
 */
ofxCounter
ofo_dossier_get_prevexe_last_entry( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, counter, 0, DOS_PREVEXE_ENTRY );
}

/**
 * ofo_dossier_get_prevexe_end:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the end date of the previous exercice, or %NULL.
 */
const GDate *
ofo_dossier_get_prevexe_end( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, date, NULL, DOS_PREVEXE_END );
}

/**
 * ofo_dossier_get_rpid:
 * @dossier: this #ofoDossier instance.
 *
 * Returns: the random pseudo identifier of the dossier.
 *
 * Use case: refuse to overwrite an archive with another which would
 * come from another dossier.
 */
const gchar *
ofo_dossier_get_rpid( const ofoDossier *dossier )
{
	ofo_base_getter( DOSSIER, dossier, string, NULL, DOS_RPID );
}

/**
 * ofo_dossier_get_min_deffect:
 * @dossier: this #ofoDossier instance.
 * @ledger: [allow-none]: the imputed ledger.
 * @date: [out]: the #GDate date to be set.
 *
 * Computes the minimal effect date valid for the considered dossier and
 * ledger.
 *
 * This minimal effect date is the greater of:
 * - the begin of the exercice (if set)
 * - last ledger closing date (if set) + 1
 * - the last period closing date + 1.
 *
 * Returns: the @date #GDate pointer. It should never be %NULL (unless the
 * provided @date be itself %NULL), but may be invalid.
 */
GDate *
ofo_dossier_get_min_deffect( const ofoDossier *dossier, const ofoLedger *ledger, GDate *date )
{
	const GDate *last_clo;
	gint to_add;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, NULL );
	g_return_val_if_fail( date, NULL );

	if( ledger ){
		g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );
	}

	my_date_set_from_date( date, ofo_dossier_get_exe_begin( dossier ));

	/* compare against the ledger closing */
	last_clo = ledger ? ofo_ledger_get_last_close( ledger ) : NULL;
	to_add = 0;

	if( my_date_is_valid( date )){
		if( my_date_is_valid( last_clo )){
			if( my_date_compare( date, last_clo ) <= 0 ){
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

	/* compare against the period closing */
	last_clo = ofo_dossier_get_last_closing_date( dossier );
	to_add = 0;

	if( my_date_is_valid( date )){
		if( my_date_is_valid( last_clo )){
			if( my_date_compare( date, last_clo ) <= 0 ){
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

	str = my_date_to_str( dmax, ofa_prefs_date_get_display_format( ofo_base_get_hub( OFO_BASE( dossier ))));
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
		*msgerr = g_strdup( _( "Label is empty" ));
		return( FALSE );
	}

	if( nb_months <= 0 ){
		*msgerr = g_strdup_printf( "Length of exercice = %d is invalid", nb_months );
		return( FALSE );
	}

	if( !my_strlen( currency )){
		*msgerr = g_strdup( _( "Default currency is empty"));
		return( FALSE );
	}

	if( my_date_is_valid( begin ) && my_date_is_valid( end )){
		if( my_date_compare( begin, end ) > 0 ){
			*msgerr = g_strdup( _( "Beginning date of the exercice is greater than the ending date" ));
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
 * ofo_dossier_set_sld_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_sld_ope( ofoDossier *dossier, const gchar *ope )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_SLD_OPE, ope );
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
 * ofo_dossier_set_label2:
 */
void
ofo_dossier_set_label2( ofoDossier *dossier, const gchar *label )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_LABEL2, label );
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
 * ofo_dossier_set_vatic:
 */
void
ofo_dossier_set_vatic( ofoDossier *dossier, const gchar *tvaic )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_VATIC, tvaic );
}

/**
 * ofo_dossier_set_naf:
 */
void
ofo_dossier_set_naf( ofoDossier *dossier, const gchar *naf )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_NAF, naf );
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
dossier_set_upd_stamp( ofoDossier *dossier, const myStampVal *stamp )
{
	ofo_base_setter( DOSSIER, dossier, timestamp, DOS_UPD_STAMP, stamp );
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
 * ofo_dossier_set_rpid:
 * @dossier: this #ofoDossier instance.
 * @rpid: the random pseudo identifier.
 *
 * Set the rpid of the dossier.
 */
void
ofo_dossier_set_rpid( ofoDossier *dossier, const gchar *rpid )
{
	ofo_base_setter( DOSSIER, dossier, string, DOS_RPID, rpid );
}

/*
 * our random pseudo identifier is :
 * - 32 lowest bits from timestamp
 * - 32 bits from random
 * - 32 lowest bits from MAC address
 *   -> replaced by 32 highest bits from timestamp
 */
static void
dossier_setup_rpid( ofoDossier *dossier )
{
	myStampVal *stamp;
	gchar *rpid;

	guint32 random32 = g_random_int();
	stamp = my_stamp_new_now();
	guint32 stamp_a = my_stamp_get_seconds( stamp ) & 0x00000000ffffffff;
	/* this is nul
	guint32 stamp_b = stamp.tv_usec & 0xffffffff00000000; */
	guint32 stamp_b = my_stamp_get_usecs( stamp ) & 0x00000000ffffffff;

	rpid = g_strdup_printf( "%8.8x-%8.8x-%8.8x", stamp_a, random32, stamp_b );
	ofo_dossier_set_rpid( dossier, rpid );
	g_free( rpid );

	my_stamp_free( stamp );
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
 * ofo_dossier_set_prevexe_last_entry:
 * @dossier: this #ofoDossier object.
 * @number: the last entry number allocated during the previous exercice.
 */
void
ofo_dossier_set_prevexe_last_entry( ofoDossier *dossier, ofxCounter number )
{
	ofo_base_setter( DOSSIER, dossier, counter, DOS_PREVEXE_ENTRY, number );
}

/**
 * dossier_set_prev_exe_end:
 * @dossier: this #ofoDossier instance.
 * @date: the end date of the previous exercice.
 */
void
ofo_dossier_set_prevexe_end( ofoDossier *dossier, const GDate *date )
{
	ofo_base_setter( DOSSIER, dossier, date, DOS_PREVEXE_END, date );
}

/**
 * ofo_dossier_currency_get_list:
 * @dossier: this dossier.
 *
 * Returns: an alphabetically sorted #GSList of the currencies defined
 * in the subtable.
 *
 * The returned list should be #ofo_dossier_currency_free_list() by the
 * caller.
 */
GSList *
ofo_dossier_currency_get_list( ofoDossier *dossier )
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
 * ofo_dossier_currency_get_sld_account:
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
ofo_dossier_currency_get_sld_account( ofoDossier *dossier, const gchar *currency )
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
	ofoDossierPrivate *priv;
	GList *it;
	GList *cur_detail;
	const gchar *cur_code;

	priv = ofo_dossier_get_instance_private( dossier );

	for( it=priv->cur_details ; it ; it=it->next ){
		cur_detail = ( GList * ) it->data;
		cur_code = ofa_box_get_string( cur_detail, DOS_CURRENCY );
		if( !my_collate( cur_code, currency )){
			return( cur_detail );
		}
	}

	return( NULL );
}

static GList *
dossier_find_currency_by_account( ofoDossier *dossier, const gchar *account )
{
	ofoDossierPrivate *priv;
	GList *it;
	GList *cur_detail;
	const gchar *cur_account;

	priv = ofo_dossier_get_instance_private( dossier );

	for( it=priv->cur_details ; it ; it=it->next ){
		cur_detail = ( GList * ) it->data;
		cur_account = ofa_box_get_string( cur_detail, DOS_SLD_ACCOUNT );
		if( !my_collate( cur_account, account )){
			return( cur_detail );
		}
	}

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

/**
 * ofo_dossier_currency_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown DOS_ID in OFA_T_DOSSIER_CUR child table.
 *
 * The returned list should be #ofo_dossier_cur_free_orphans() by the
 * caller.
 */
GList *
ofo_dossier_currency_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_DOSSIER_CUR" ));
}

static GList *
get_orphans( ofaIGetter *getter, const gchar *table )
{
	ofaHub *hub;
	ofaIDBConnect *connect;
	GList *orphans;
	GSList *result, *irow, *icol;
	gchar *query;
	guint dosid;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( my_strlen( table ), NULL );

	orphans = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_strdup_printf( "SELECT DOS_ID FROM %s WHERE DOS_ID!=%u", table, DOSSIER_ROW_ID );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = irow->data;
			dosid = atoi(( const gchar * ) icol->data );
			orphans = g_list_prepend( orphans, GUINT_TO_POINTER( dosid ));
		}
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( orphans );
}

/**
 * ofo_dossier_currency_reset:
 * @dossier: this dossier.
 *
 * Reset the currencies (free all).
 *
 * This function should be called when updating the currencies
 * properties, as we are only able to set new elements in the list.
 */
void
ofo_dossier_currency_reset( ofoDossier *dossier )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run );

	free_currency_details( dossier );
}

/**
 * ofo_dossier_currency_set_sld_account:
 * @dossier: this dossier.
 * @currency: the currency.
 * @account: the account.
 *
 * Set the balancing account for the currency.
 */
void
ofo_dossier_currency_set_sld_account( ofoDossier *dossier, const gchar *currency, const gchar *account )
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

/**
 * ofo_dossier_doc_get_count:
 * @dossier: this #ofoDossier object.
 *
 * Returns: the count of attached documents.
 */
guint
ofo_dossier_doc_get_count( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	priv = ofo_dossier_get_instance_private( dossier );

	return( g_list_length( priv->docs ));
}

/**
 * ofo_dossier_doc_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown DOS_ID in OFA_T_DOSSIER_DOC child table.
 *
 * The returned list should be #ofo_dossier_doc_free_orphans() by the
 * caller.
 */
GList *
ofo_dossier_doc_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_DOSSIER_DOC" ));
}

/**
 * ofo_dossier_prefs_get_count:
 * @dossier: this #ofoDossier object.
 *
 * Returns: the count of recorded @dossier preferences.
 */
guint
ofo_dossier_prefs_get_count( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( !OFO_BASE( dossier )->prot->dispose_has_run, 0 );

	priv = ofo_dossier_get_instance_private( dossier );

	return( g_list_length( priv->prefs ));
}

/**
 * ofo_dossier_prefs_get_orphans:
 * @getter: a #ofaIGetter instance.
 *
 * Returns: the list of unknown DOS_ID in OFA_T_DOSSIER_PREFS child table.
 *
 * The returned list should be #ofo_dossier_prefs_free_orphans() by the
 * caller.
 */
GList *
ofo_dossier_prefs_get_orphans( ofaIGetter *getter )
{
	return( get_orphans( getter, "OFA_T_DOSSIER_PREFS" ));
}

static ofoDossier *
dossier_do_read( ofaIGetter *getter )
{
	ofoDossier *dossier;
	ofoDossierPrivate *priv;
	gchar *where;
	GList *list;
	ofaHub *hub;

	dossier = NULL;
	hub = ofa_igetter_get_hub( getter );

	where = g_strdup_printf( "OFA_T_DOSSIER WHERE DOS_ID=%d", DOSSIER_ROW_ID );
	list = ofo_base_load_dataset(
					st_boxed_defs,
					where,
					OFO_TYPE_DOSSIER,
					getter );
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

	/* Starting with 0.65 where RPID is added, check that a RPID is defined
	 * for each dossier, and set it up if not already done */
	if( !my_strlen( ofo_dossier_get_rpid( dossier )) && ofo_dossier_is_current( dossier )){
		dossier_setup_rpid( dossier );
		ofo_dossier_update( dossier );
	}

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
	ofaIGetter *getter;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *label, *notes, *stamp_str, *sdate;
	myStampVal *stamp;
	gboolean ok, current;
	const gchar *cstr, *userid;
	const GDate *date;
	ofxCounter number;

	ok = FALSE;
	getter = ofo_base_get_getter( OFO_BASE( dossier ));
	hub = ofa_igetter_get_hub( getter );
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

	notes = my_utils_quote_sql( ofo_dossier_get_exe_notes( dossier ));
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

	cstr = ofo_dossier_get_sld_ope( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SLD_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SLD_OPE=NULL," );
	}

	cstr = ofo_dossier_get_import_ledger( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_IMPORT_LEDGER='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_IMPORT_LEDGER=NULL," );
	}

	label = my_utils_quote_sql( ofo_dossier_get_label( dossier ));
	if( my_strlen( label )){
		g_string_append_printf( query, "DOS_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "DOS_LABEL=NULL," );
	}
	g_free( label );

	label = my_utils_quote_sql( ofo_dossier_get_label2( dossier ));
	if( my_strlen( label )){
		g_string_append_printf( query, "DOS_LABEL2='%s',", label );
	} else {
		query = g_string_append( query, "DOS_LABEL2=NULL," );
	}
	g_free( label );

	notes = my_utils_quote_sql( ofo_dossier_get_notes( dossier ));
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

	cstr = ofo_dossier_get_vatic( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_VATIC='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_VATIC=NULL," );
	}

	cstr = ofo_dossier_get_naf( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_NAF='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_NAF=NULL," );
	}

	date = ofo_dossier_get_last_closing_date( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_LAST_CLOSING='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_LAST_CLOSING=NULL," );
	}

	number = ofo_dossier_get_prevexe_last_entry( dossier );
	if( number > 0 ){
		g_string_append_printf( query, "DOS_PREVEXE_ENTRY=%ld,", number );
	} else {
		query = g_string_append( query, "DOS_PREVEXE_ENTRY=NULL," );
	}

	date = ofo_dossier_get_prevexe_end( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_PREVEXE_END='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_PREVEXE_END=NULL," );
	}

	current = ofo_dossier_is_current( dossier );
	g_string_append_printf( query, "DOS_CURRENT='%s',", current ? "Y":"N" );

	cstr = ofo_dossier_get_rpid( dossier );
	g_string_append_printf( query, "DOS_RPID='%s',", cstr );

	stamp = my_stamp_new_now();
	stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query,
			"DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' ", userid, stamp_str );
	g_free( stamp_str );

	g_string_append_printf( query, "WHERE DOS_ID=%d", DOSSIER_ROW_ID );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		dossier_set_upd_user( dossier, userid );
		dossier_set_upd_stamp( dossier, stamp );
		ok = TRUE;
	}

	my_stamp_free( stamp );
	g_string_free( query, TRUE );

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
	ofaIGetter *getter;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query, *stamp_str;
	GList *details;
	gboolean ok;
	GList *it;
	myStampVal *stamp;
	gint count;
	const gchar *userid;

	priv = ofo_dossier_get_instance_private( dossier );

	getter = ofo_base_get_getter( OFO_BASE( dossier ));
	hub = ofa_igetter_get_hub( getter );
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
			stamp = my_stamp_new_now();
			stamp_str = my_stamp_to_str( stamp, MY_STAMP_YYMDHMS );
			query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER SET "
					"	DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' "
					"	WHERE DOS_ID=%d", userid, stamp_str, DOSSIER_ROW_ID );
			g_free( stamp_str );

			if( !ofa_idbconnect_query( connect, query, TRUE )){
				ok = FALSE;
			} else {
				dossier_set_upd_user( dossier, userid );
				dossier_set_upd_stamp( dossier, stamp );
			}
			my_stamp_free( stamp );
		}
	}

	return( ok );
}

/*
 * ofaIDoc interface management
 */
static void
idoc_iface_init( ofaIDocInterface *iface )
{
	static const gchar *thisfn = "ofo_dossier_idoc_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idoc_get_interface_version;
}

static guint
idoc_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_dossier_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->get_label = iexportable_get_label;
	iface->get_published = iexportable_get_published;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iexportable_get_label( const ofaIExportable *instance )
{
	return( g_strdup( _( "_Dossier" )));
}

static gboolean
iexportable_get_published( const ofaIExportable *instance )
{
	return( TRUE );
}

/*
 * iexportable_export:
 * @format_id: is 'DEFAULT' for the standard class export.
 *
 * Exports the dossier properties.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const gchar *format_id )
{
	static const gchar *thisfn = "ofo_dossier_iexportable_export";

	if( !my_collate( format_id, OFA_IEXPORTER_DEFAULT_FORMAT_ID )){
		return( iexportable_export_default( exportable ));
	}

	g_warning( "%s: format_id=%s unmanaged here", thisfn, format_id );

	return( FALSE );
}

static gboolean
iexportable_export_default( ofaIExportable *exportable )
{
	ofaIGetter *getter;
	ofaStreamFormat *stformat;
	ofoDossier *dossier;
	ofoDossierPrivate *priv;
	ofaHub *hub;
	GList *cur_detail, *itd, *itp;
	gchar *str1, *str2;
	gboolean ok;
	gulong count;
	gchar field_sep;
	guint counters_count, i;

	getter = ofa_iexportable_get_getter( exportable );
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	priv = ofo_dossier_get_instance_private( dossier );

	stformat = ofa_iexportable_get_stream_format( exportable );
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = 1;
	if( ofa_stream_format_get_with_headers( stformat )){
		count += DOSSIER_TABLES_COUNT;
	}
	count += g_list_length( priv->cur_details );
	count += ofo_dossier_doc_get_count( dossier );
	count += ofo_dossier_prefs_get_count( dossier );
	counters_count = ofo_counters_get_count();
	count += counters_count;
	ofa_iexportable_set_count( exportable, count+2 );

	/* add version lines at the very beginning of the file */
	str1 = g_strdup_printf( "0%c0%cVersion", field_sep, field_sep );
	ok = ofa_iexportable_append_line( exportable, str1 );
	g_free( str1 );
	if( ok ){
		str1 = g_strdup_printf( "1%c0%c%u", field_sep, field_sep, DOSSIER_EXPORT_VERSION );
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	/* export headers */
	if( ok ){
		/* add new ofsBoxDef array at the end of the list */
		ok = ofa_iexportable_append_headers( exportable,
					DOSSIER_TABLES_COUNT, st_boxed_defs, st_currency_defs, st_doc_defs, st_pref_defs, st_id_defs );
	}

	/* export the dataset */
	if( ok ){
		str1 = ofa_box_csv_get_line( OFO_BASE( dossier )->prot->fields, stformat, NULL );
		str2 = g_strdup_printf( "1%c1%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}

	for( cur_detail=priv->cur_details ; cur_detail && ok ; cur_detail=cur_detail->next ){
		str1 = ofa_box_csv_get_line( cur_detail->data, stformat, NULL );
		str2 = g_strdup_printf( "1%c2%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}

	for( itd=priv->docs ; itd && ok ; itd=itd->next ){
		str1 = ofa_box_csv_get_line( itd->data, stformat, NULL );
		str2 = g_strdup_printf( "1%c3%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}

	for( itp=priv->prefs ; itp && ok ; itp=itp->next ){
		str1 = ofa_box_csv_get_line( itp->data, stformat, NULL );
		str2 = g_strdup_printf( "1%c4%c%s", field_sep, field_sep, str1 );
		ok = ofa_iexportable_append_line( exportable, str2 );
		g_free( str2 );
		g_free( str1 );
	}

	for( i=0 ; i<counters_count ; ++i ){
		str1 = g_strdup_printf( "1%c5%c%u%c%s%c%lu",
					field_sep,
					field_sep, DOSSIER_ROW_ID,
					field_sep, ofo_counters_get_key( getter, i ),
					field_sep, ofo_counters_get_last_value( getter, i ));
		ok = ofa_iexportable_append_line( exportable, str1 );
		g_free( str1 );
	}

	return( ok );
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

/*
 * ofaISignalable interface management
 */
static void
isignalable_iface_init( ofaISignalableInterface *iface )
{
	static const gchar *thisfn = "ofo_entry_isignalable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->connect_to = isignalable_connect_to;
}

static void
isignalable_connect_to( ofaISignaler *signaler )
{
	static const gchar *thisfn = "ofo_dossier_isignalable_connect_to";

	g_debug( "%s: signaler=%p", thisfn, ( void * ) signaler );

	g_return_if_fail( signaler && OFA_IS_ISIGNALER( signaler ));

	g_signal_connect( signaler, SIGNALER_BASE_IS_DELETABLE, G_CALLBACK( signaler_on_deletable_object ), NULL );
	g_signal_connect( signaler, SIGNALER_EXERCICE_DATES_CHANGED, G_CALLBACK( signaler_on_exe_dates_changed ), NULL );
	g_signal_connect( signaler, SIGNALER_BASE_UPDATED, G_CALLBACK( signaler_on_updated_base ), NULL );
}

/*
 * SIGNALER_BASE_IS_DELETABLE signal handler
 */
static gboolean
signaler_on_deletable_object( ofaISignaler *signaler, ofoBase *object, void *empty )
{
	static const gchar *thisfn = "ofo_dossier_signaler_on_deletable_object";
	gboolean deletable;

	g_debug( "%s: signaler=%p, object=%p (%s), empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) empty );

	deletable = TRUE;

	if( OFO_IS_ACCOUNT( object )){
		deletable = signaler_is_deletable_account( signaler, OFO_ACCOUNT( object ));

	} else if( OFO_IS_CURRENCY( object )){
		deletable = signaler_is_deletable_currency( signaler, OFO_CURRENCY( object ));

	} else if( OFO_IS_LEDGER( object )){
		deletable = signaler_is_deletable_ledger( signaler, OFO_LEDGER( object ));

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		deletable = signaler_is_deletable_ope_template( signaler, OFO_OPE_TEMPLATE( object ));
	}

	return( deletable );
}

static gboolean
signaler_is_deletable_account( ofaISignaler *signaler, ofoAccount *account )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_DOSSIER_CUR WHERE DOS_SLD_ACCOUNT='%s'",
			ofo_account_get_number( account ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_currency( ofaISignaler *signaler, ofoCurrency *currency )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_DOSSIER WHERE DOS_DEF_CURRENCY='%s'",
			ofo_currency_get_code( currency ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	if( count == 0 ){
		query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_DOSSIER_CUR WHERE DOS_CURRENCY='%s'",
				ofo_currency_get_code( currency ));

		ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

		g_free( query );
	}

	return( count == 0 );
}

static gboolean
signaler_is_deletable_ledger( ofaISignaler *signaler, ofoLedger *ledger )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_DOSSIER WHERE DOS_IMPORT_LEDGER='%s'",
			ofo_ledger_get_mnemo( ledger ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

static gboolean
signaler_is_deletable_ope_template( ofaISignaler *signaler, ofoOpeTemplate *template )
{
	ofaIGetter *getter;
	ofaHub *hub;
	gchar *query;
	gint count;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_DOSSIER WHERE DOS_FORW_OPE='%s' OR DOS_SLD_OPE='%s'",
			ofo_ope_template_get_mnemo( template ),
			ofo_ope_template_get_mnemo( template ));

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count == 0 );
}

/*
 * SIGNALER_EXERCICE_DATES_CHANGED signal handler
 *
 * Changing beginning or ending exercice dates is only possible for the
 * current exercice.
 */
static void
signaler_on_exe_dates_changed( ofaISignaler *signaler, const GDate *prev_begin, const GDate *prev_end, void *empty )
{
	static const gchar *thisfn = "ofo_dossier_signaler_on_exe_dates_changed";
	ofaIGetter *getter;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	ofaIDBExerciceMeta *period;
	ofoDossier *dossier;
	gboolean current;

	g_debug( "%s: signaler=%p, prev_begin=%p, prev_end=%p, empty=%p",
			thisfn, ( void * ) signaler, ( void * ) prev_begin, ( void * ) prev_end, ( void * ) empty );

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );

	dossier = ofa_hub_get_dossier( hub );
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	current = ofo_dossier_is_current( dossier );
	g_return_if_fail( current );

	connect = ofa_hub_get_connect( hub );
	period = ofa_idbconnect_get_exercice_meta( connect );

	ofa_idbexercice_meta_set_current( period, current );
	ofa_idbexercice_meta_set_begin_date( period, ofo_dossier_get_exe_begin( dossier ));
	ofa_idbexercice_meta_set_end_date( period, ofo_dossier_get_exe_end( dossier ));
	ofa_idbexercice_meta_update_settings( period );
}

/*
 * SIGNALER_BASE_UPDATED signal handler
 */
static void
signaler_on_updated_base( ofaISignaler *signaler, ofoBase *object, const gchar *prev_id, void *empty )
{
	static const gchar *thisfn = "ofo_dossier_signaler_on_updated_base";
	const gchar *code, *new_id, *new_mnemo;

	g_debug( "%s: signaler=%p, object=%p (%s), prev_id=%s, empty=%p",
			thisfn,
			( void * ) signaler,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) empty );

	if( OFO_IS_ACCOUNT( object )){
		if( my_strlen( prev_id )){
			new_id = ofo_account_get_number( OFO_ACCOUNT( object ));
			if( my_collate( new_id, prev_id )){
				signaler_on_updated_account_id( signaler, prev_id, new_id );
			}
		}

	} else if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( my_collate( code, prev_id )){
				signaler_on_updated_currency_code( signaler, prev_id, code );
			}
		}

	} else if( OFO_IS_LEDGER( object )){
		if( my_strlen( prev_id )){
			new_mnemo = ofo_ledger_get_mnemo( OFO_LEDGER( object ));
			if( my_collate( new_mnemo, prev_id )){
				signaler_on_updated_ledger_mnemo( signaler, prev_id, new_mnemo );
			}
		}

	} else if( OFO_IS_OPE_TEMPLATE( object )){
		if( my_strlen( prev_id )){
			new_mnemo = ofo_ope_template_get_mnemo( OFO_OPE_TEMPLATE( object ));
			if( my_collate( new_mnemo, prev_id )){
				signaler_on_updated_ope_template_mnemo( signaler, prev_id, new_mnemo );
			}
		}
	}
}

static void
signaler_on_updated_account_id( ofaISignaler *signaler, const gchar *prev_id, const gchar *new_id )
{
	ofaIGetter *getter;
	ofaHub *hub;
	ofoDossier *dossier;
	gchar *query;
	GList *details;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER_CUR SET DOS_SLD_ACCOUNT='%s' WHERE DOS_SLD_ACCOUNT='%s'",
						new_id, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	details = dossier_find_currency_by_account( dossier, prev_id );
	if( details ){
		ofa_box_set_string( details, DOS_SLD_ACCOUNT, new_id );
	}
}

static void
signaler_on_updated_currency_code( ofaISignaler *signaler, const gchar *prev_id, const gchar *code )
{
	ofaIGetter *getter;
	ofaHub *hub;
	ofoDossier *dossier;
	gchar *query;
	const gchar *def_currency;
	GList *details;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_DEF_CURRENCY='%s' WHERE DOS_DEF_CURRENCY='%s'", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	def_currency = ofo_dossier_get_default_currency( dossier );
	if( !my_collate( def_currency, prev_id )){
		ofo_dossier_set_default_currency( dossier, code );
	}

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER_CUR "
					"	SET DOS_CURRENCY='%s' WHERE DOS_CURRENCY='%s'", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	details = dossier_find_currency_by_code( dossier, prev_id );
	if( details ){
		ofa_box_set_string( details, DOS_CURRENCY, code );
	}
}

static void
signaler_on_updated_ledger_mnemo( ofaISignaler *signaler, const gchar *prev_mnemo, const gchar *new_mnemo )
{
	ofaIGetter *getter;
	ofaHub *hub;
	ofoDossier *dossier;
	gchar *query;
	const gchar *mnemo;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_IMPORT_LEDGER='%s' WHERE DOS_IMPORT_LEDGER='%s'", new_mnemo, prev_mnemo );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	mnemo = ofo_dossier_get_import_ledger( dossier );
	if( !my_collate( mnemo, prev_mnemo )){
		ofo_dossier_set_import_ledger( dossier, new_mnemo );
	}
}

static void
signaler_on_updated_ope_template_mnemo( ofaISignaler *signaler, const gchar *prev_mnemo, const gchar *new_mnemo )
{
	ofaIGetter *getter;
	ofaHub *hub;
	ofoDossier *dossier;
	gchar *query;
	const gchar *mnemo;

	getter = ofa_isignaler_get_getter( signaler );
	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_FORW_OPE='%s' WHERE DOS_FORW_OPE='%s'", new_mnemo, prev_mnemo );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	mnemo = ofo_dossier_get_forward_ope( dossier );
	if( !my_collate( mnemo, prev_mnemo )){
		ofo_dossier_set_forward_ope( dossier, new_mnemo );
	}

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_SLD_OPE='%s' WHERE DOS_SLD_OPE='%s'", new_mnemo, prev_mnemo );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );

	mnemo = ofo_dossier_get_sld_ope( dossier );
	if( !my_collate( mnemo, prev_mnemo )){
		ofo_dossier_set_sld_ope( dossier, new_mnemo );
	}
}
