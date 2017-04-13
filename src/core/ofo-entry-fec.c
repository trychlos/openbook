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

#include "my/my-char.h"
#include "my/my-date.h"
#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-stream-format.h"
#include "api/ofo-account.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-iconcil.h"
#include "core/ofo-entry-fec.h"

/* a data structure attached to the instance
 * so that we are able to release the resources at the end
 *
 * We provide here a null-terminated array with one 'FEC' format.
 */
typedef struct {
	ofsIExporterFormat *format_array;
}
	sFecData;

#define FEC_DATA                        "ofo-entry-fec-data"

static ofsIExporterFormat *get_fec_format( ofaIExporter *self, ofaIGetter *getter );
static GList              *iexportable_export_fec_get_entries( ofaIGetter *getter );
static gint                iexportable_export_fec_cmp_entries( ofoEntry *a, ofoEntry *b );
static sFecData           *get_instance_data( const ofaIExporter *self );
static void                on_instance_finalized( sFecData *sdata, GObject * finalized_instance );

/**
 * ofo_entry_fec_get_exporter_formats:
 * @exporter: this #ofaIExporter instance.
 * @exportable_type: the currently exported #GType.
 * @getter: the #ofaIGetter of the application.
 *
 * Returns: the export formats we are able to manage for @exportable_type.
 */
ofsIExporterFormat *
ofo_entry_fec_get_exporter_formats( ofaIExporter *exporter, GType exportable_type, ofaIGetter *getter )
{
	return( exportable_type == OFO_TYPE_ENTRY ? get_fec_format( exporter, getter ) : NULL );
}

/*
 * Allocating here a new dedicated #ofaStreamFormat object for FEC
 */
static ofsIExporterFormat *
get_fec_format( ofaIExporter *self, ofaIGetter *getter )
{
	ofsIExporterFormat *fec_format;
	ofaStreamFormat *st_format;
	sFecData *sdata;

	sdata = get_instance_data( self );

	if( !sdata->format_array ){
		sdata->format_array = g_new0( ofsIExporterFormat, 2 );

		fec_format = &sdata->format_array[0];
		fec_format->format_id = g_strdup( ENTRY_FEC_EXPORT_FORMAT );
		fec_format->format_label = g_strdup( _( "Fichier des Ecritures Comptables (FEC - Art. A47 A-1)" ));

		st_format = ofa_stream_format_new( getter, _( "FEC" ), OFA_SFMODE_EXPORT );

		ofa_stream_format_set( st_format,
				TRUE,  "ISO-8859-15",			/* this one (EBCDIC would be allowed too) */
				TRUE,  MY_DATE_YYMD,			/* date format yyyymmdd */
				FALSE, MY_CHAR_ZERO,			/* no thousand sep */
				TRUE,  MY_CHAR_COMMA,			/* comma decimal sep */
				TRUE,  MY_CHAR_PIPE,			/* '|' field sep */
				FALSE, MY_CHAR_ZERO,			/* no string delim */
				1 );							/* with headers */

		ofa_stream_format_set_field_updatable( st_format, OFA_SFHAS_ALL, FALSE );

		fec_format->stream_format = st_format;
	}

	return( sdata->format_array );
}

/**
 * ofo_entry_fec_export:
 * @exportable: this #ofaIExportable instance.
 *
 * Creates and exports the 'Fichier des Ecritures Comptables' (FEC)
 * cf. Article A47 A-1 du Livre des Procédures Fiscales de la DGI
 *
 * charmap: ASCII, norme ISO 8859-15 ou EBCDIC
 * date format: AAAAMMJJ (obligatoire, champ correspondant ignoré)
 * thousand sep: none (obligatoire, champ correspondant ignoré)
 * decimal sep: comma (obligatoire, champ correspondant ignoré)
 * field separator: tabulation ou le caractère " | "
 * string delim: not specified
 * with headers: yes
 *
 * Entries must be ordered by chronological order of validation; here,
 * this means by effect_date+upd_timestamp+entry_number
 *
 * Filenaming: <siren>FEC<AAAAMMJJ>, where 'AAAAMMJJ' is the end of
 * the exercice.
 *
 * Ce fichier est constitué des écritures après opérations d'inventaire,
 * hors écritures de centralisation et hors écritures de solde des comptes
 * de charges et de produits. Il comprend les écritures de reprise des
 * soldes de l'exercice antérieur.
 *
 * NOTE TO THE MAINTAINER: every update here should be described in the
 * 'docs/DGI/FEC_Description.ods' sheet.
 */
gboolean
ofo_entry_fec_export( ofaIExportable *exportable )
{
	static const gchar *thisfn = "ofo_entry_fec_export";
	ofaIGetter *getter;
	ofaStreamFormat *stformat;
	GList *sorted, *it;
	gboolean ok, with_headers;
	gchar field_sep;
	gulong count;
	ofoEntry *entry;
	GString *str;
	gchar *sdope, *sdeffect, *sdebit, *scredit, *sletid, *sletdate, *sref, *stiers;
	gchar *sopemne, *sopelib, *sopenum, *sdregl, *smodregl;
	ofoConcil *concil;
	const gchar *led_id, *acc_id, *cur_code, *cref, *cope;
	ofoAccount *account;
	ofoLedger *ledger;
	ofoCurrency *currency;
	guint date_fmt;
	ofxCounter counter, tiers;
	ofoOpeTemplate *template;
	ofeEntryStatus status;
	ofeEntryRule rule;
	ofeEntryPeriod period;

	g_debug( "%s: exportable=%p", thisfn, ( void * ) exportable );

	g_return_val_if_fail( exportable && OFA_IS_IEXPORTABLE( exportable ), FALSE );

	getter = ofa_iexportable_get_getter( exportable );
	sorted = iexportable_export_fec_get_entries( getter );

	stformat = ofa_iexportable_get_stream_format( exportable );
	with_headers = TRUE;
	date_fmt = MY_DATE_YYMD;
	field_sep = ofa_stream_format_get_field_sep( stformat );

	count = ( gulong ) g_list_length( sorted );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		/* 18 mandatory columns */
		str = g_string_new( "JournalCode" );
		g_string_append_printf( str, "%cJournalLib", field_sep );
		g_string_append_printf( str, "%cEcritureNum", field_sep );
		g_string_append_printf( str, "%cEcritureDate", field_sep );
		g_string_append_printf( str, "%cCompteNum", field_sep );
		g_string_append_printf( str, "%cCompteLib", field_sep );
		g_string_append_printf( str, "%cCompAuxNum", field_sep );
		g_string_append_printf( str, "%cCompAuxLib", field_sep );
		g_string_append_printf( str, "%cPieceRef", field_sep );
		g_string_append_printf( str, "%cPieceDate", field_sep );
		g_string_append_printf( str, "%cEcritureLib", field_sep );
		g_string_append_printf( str, "%cDebit", field_sep );
		g_string_append_printf( str, "%cCredit", field_sep );
		g_string_append_printf( str, "%cEcritureLet", field_sep );
		g_string_append_printf( str, "%cDateLet", field_sep );
		g_string_append_printf( str, "%cValidDate", field_sep );
		g_string_append_printf( str, "%cMontantDevise", field_sep );
		g_string_append_printf( str, "%cIDevise", field_sep );
		/* 4 columns for BNC recettes/dépenses */
		g_string_append_printf( str, "%cDateRglt", field_sep );
		g_string_append_printf( str, "%cModeRglt", field_sep );
		g_string_append_printf( str, "%cNatOp", field_sep );
		g_string_append_printf( str, "%cIdClient", field_sep );
		/* other columns from the application */
		g_string_append_printf( str, "%cOpeTemplateLib", field_sep );
		g_string_append_printf( str, "%cStatus", field_sep );
		g_string_append_printf( str, "%cOpeNum", field_sep );
		g_string_append_printf( str, "%cRule", field_sep );
		g_string_append_printf( str, "%cPeriod", field_sep );

		ok = ofa_iexportable_append_line( exportable, str->str );

		g_string_free( str, TRUE );

		if( !ok ){
			return( FALSE );
		}
	}

	for( it=sorted ; it ; it=it->next ){
		entry = ( ofoEntry * ) it->data;
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

		led_id = ofo_entry_get_ledger( entry );
		g_return_val_if_fail( led_id && my_strlen( led_id ), FALSE );
		ledger = ofo_ledger_get_by_mnemo( getter, led_id );
		g_return_val_if_fail( ledger && OFO_IS_LEDGER( ledger ), FALSE );

		acc_id = ofo_entry_get_account( entry );
		g_return_val_if_fail( acc_id && my_strlen( acc_id ), FALSE );
		account = ofo_account_get_by_number( getter, acc_id );
		g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

		cur_code = ofo_entry_get_currency( entry );
		g_return_val_if_fail( cur_code && my_strlen( cur_code ), FALSE );
		currency = ofo_currency_get_by_code( getter, cur_code );
		g_return_val_if_fail( currency && OFO_IS_CURRENCY( currency ), FALSE );

		cope = ofo_entry_get_ope_template( entry );
		template = my_strlen( cope ) ? ofo_ope_template_get_by_mnemo( getter, cope ) : NULL;
		g_return_val_if_fail( !template || OFO_IS_OPE_TEMPLATE( template ), FALSE );

		sdope = my_date_to_str( ofo_entry_get_dope( entry ), date_fmt );
		sdeffect = my_date_to_str( ofo_entry_get_deffect( entry ), date_fmt );
		sdebit = ofa_amount_to_csv( ofo_entry_get_debit( entry ), currency, stformat );
		scredit = ofa_amount_to_csv( ofo_entry_get_credit( entry ), currency, stformat );

		cref = ofo_entry_get_ref( entry );
		/* piece ref is mandatory */
		sref = g_strdup( cref ? cref : sdope );

		sopemne = g_strdup( cope ? cope : "" );
		sopelib = g_strdup( template ? ofo_ope_template_get_label( template ) : "" );

		counter = ofo_entry_get_ope_number( entry );
		sopenum = counter ? g_strdup_printf( "%lu", counter ) : g_strdup( "" );

		/* we put in 'lettrage' columns both conciliation and settlement infos
		 * with an indicator of the origin */
		sdregl = NULL;
		concil = ofa_iconcil_get_concil( OFA_ICONCIL( entry ));
		if( concil ){
			sletid = g_strdup_printf( "R%lu", ofo_concil_get_id( concil ));
			sletdate = my_date_to_str( ofo_concil_get_dval( concil ), date_fmt );
			sdregl = g_strdup( sletdate );
		} else {
			counter = ofo_entry_get_settlement_number( entry );
			if( counter ){
				sletid = g_strdup_printf( "S%lu", counter );
				sletdate = my_stamp_to_str( ofo_entry_get_settlement_stamp( entry ), MY_STAMP_YYMD );
			} else {
				sletid = g_strdup( "" );
				sletdate = g_strdup( "" );
			}
		}
		/* reglement date is conciliation value date if exists
		 * reglement mode is piece ref if exists */
		if( !sdregl ){
			sdregl = g_strdup( "" );
		}
		smodregl = g_strdup( cref ? cref : "" );

		tiers = ofo_entry_get_tiers( entry );
		stiers = tiers ? g_strdup_printf( "%lu", tiers ) : g_strdup( "" );

		status = ofo_entry_get_status( entry );
		rule = ofo_entry_get_rule( entry );
		period = ofo_entry_get_period( entry );

		/* 18 mandatory columns */
		str = g_string_new( led_id );
		g_string_append_printf( str, "%c%s", field_sep, ofo_ledger_get_label( ledger ));
		g_string_append_printf( str, "%c%lu", field_sep, ofo_entry_get_number( entry ));
		g_string_append_printf( str, "%c%s", field_sep, sdope );
		g_string_append_printf( str, "%c%s", field_sep, acc_id );
		g_string_append_printf( str, "%c%s", field_sep, ofo_account_get_label( account ));
		g_string_append_printf( str, "%c", field_sep );
		g_string_append_printf( str, "%c", field_sep );
		g_string_append_printf( str, "%c%s", field_sep, sref );
		g_string_append_printf( str, "%c%s", field_sep, sdope );
		g_string_append_printf( str, "%c%s", field_sep, ofo_entry_get_label( entry ));
		g_string_append_printf( str, "%c%s", field_sep, sdebit );
		g_string_append_printf( str, "%c%s", field_sep, scredit );
		g_string_append_printf( str, "%c%s", field_sep, sletid );
		g_string_append_printf( str, "%c%s", field_sep, sletdate );
		g_string_append_printf( str, "%c%s", field_sep, sdeffect );
		g_string_append_printf( str, "%c", field_sep );
		g_string_append_printf( str, "%c%s", field_sep, cur_code );
		/* 4 columns for BNC recettes/dépenses */
		g_string_append_printf( str, "%c%s", field_sep, sdregl );
		g_string_append_printf( str, "%c%s", field_sep, smodregl );
		g_string_append_printf( str, "%c%s", field_sep, sopemne );
		g_string_append_printf( str, "%c%s", field_sep, stiers );
		/* other columns from the system */
		g_string_append_printf( str, "%c%s", field_sep, sopelib );
		g_string_append_printf( str, "%c%s", field_sep, ofo_entry_status_get_dbms( status ));
		g_string_append_printf( str, "%c%s", field_sep, sopenum );
		g_string_append_printf( str, "%c%s", field_sep, ofo_entry_rule_get_dbms( rule ));
		g_string_append_printf( str, "%c%s", field_sep, ofo_entry_period_get_dbms( period ));

		ok = ofa_iexportable_append_line( exportable, str->str );

		g_string_free( str, TRUE );
		g_free( sdope );
		g_free( sdeffect );
		g_free( sdebit );
		g_free( scredit );
		g_free( sletid );
		g_free( sletdate );
		g_free( sref );
		g_free( sopemne );
		g_free( sopelib );
		g_free( sopenum );
		g_free( stiers );

		if( !ok ){
			return( FALSE );
		}
	}

	g_list_free( sorted );

	return( TRUE );
}

/*
 * The returned list should be g_list_free() by the caller.
 */
static GList *
iexportable_export_fec_get_entries( ofaIGetter *getter )
{
	GList *dataset, *sorted, *it;
	ofaHub *hub;
	ofoDossier *dossier;
	ofoEntry *entry;
	const GDate *dbegin, *dend, *deffect;

	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	dbegin = ofo_dossier_get_exe_begin( dossier );
	dend = ofo_dossier_get_exe_end( dossier );

	sorted = NULL;
	dataset = ofo_entry_get_dataset( getter );

	for( it=dataset ; it ; it=it->next ){
		entry = ( ofoEntry * ) it->data;
		g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), NULL );
		deffect = ofo_entry_get_deffect( entry );

		if( my_date_compare_ex( dbegin, deffect, TRUE ) <= 0 &&
				my_date_compare_ex( deffect, dend, FALSE ) <= 0 &&
				ofo_entry_get_rule( entry ) != ENT_RULE_CLOSE ){

			sorted = g_list_insert_sorted( sorted, entry, ( GCompareFunc ) iexportable_export_fec_cmp_entries );
		}
	}

	return( sorted );
}

static gint
iexportable_export_fec_cmp_entries( ofoEntry *a, ofoEntry *b )
{
	const GDate *deffecta, *deffectb;
	const GTimeVal *stampa, *stampb;
	ofxCounter numa, numb;
	gint cmp;

	deffecta = ofo_entry_get_deffect( a );
	deffectb = ofo_entry_get_deffect( b );
	cmp = my_date_compare( deffecta, deffectb );

	if( cmp == 0 ){
		stampa = ofo_entry_get_upd_stamp( a );
		stampb = ofo_entry_get_upd_stamp( b );
		cmp = my_stamp_compare( stampa, stampb );
	}

	if( cmp == 0 ){
		numa = ofo_entry_get_number( a );
		numb = ofo_entry_get_number( b );
		cmp = numa < numb ? -1 : ( numa > numb ? +1 : 0 );
	}

	return( cmp );
}

static sFecData *
get_instance_data( const ofaIExporter *self )
{
	sFecData *sdata;

	sdata = ( sFecData * ) g_object_get_data( G_OBJECT( self ), FEC_DATA );

	if( !sdata ){
		sdata = g_new0( sFecData, 1 );
		g_object_set_data( G_OBJECT( self ), FEC_DATA, sdata );
		g_object_weak_ref( G_OBJECT( self ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sFecData *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofo_entry_fec_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_free( sdata->format_array[0].format_id );
	g_free( sdata->format_array[0].format_label );
	g_object_unref( sdata->format_array[0].stream_format );
	g_free( sdata->format_array );
	g_free( sdata );
}
