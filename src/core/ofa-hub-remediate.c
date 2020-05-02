/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "my/my-date.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-igetter.h"
#include "api/ofo-account.h"
#include "api/ofo-data.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"

#include "core/ofa-hub-remediate.h"

/*
 * ofa_hub_remediate_recompute_balances:
 * @hub: this #ofaHub instance.
 * @pfn: a callback function, called on each entry.
 * @data: data attached to the callback.
 *
 * Reset and recompute accounts and ledgers balances.
 *
 * Return: %TRUE if OK.
 */
gboolean
ofa_hub_remediate_recompute_balances( ofaHub *hub, fnRemediateRecompute pfn, void *data )
{
	static const gchar *thisfn = "ofa_hub_remediate_recompute_balances";
	gboolean ok;
	GList *entries, *it;
	//ofoEntry *entry;
	//const GDate *ent_deffect;

	g_debug( "%s: hub=%p, pfn=%p, data=%p", thisfn, ( void * ) hub, ( void * ) pfn, data );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	ok = TRUE;
	entries = ofo_entry_get_dataset_for_exercice_by_status( OFA_IGETTER( hub ), -1 );

	for( it=entries ; it ; it=it->next ){
		//entry = OFO_ENTRY( it->data );
		//ent_deffect = ofo_entry_get_deffect( entry );
		//g_return_val_if_fail( my_date_compare( ent_deffect, dos_dend ) <= 0, G_SOURCE_REMOVE );
		//g_signal_emit_by_name( signaler, SIGNALER_EXERCICE_RECOMPUTE, entry );
		//update_bar( bar, &i, count, thisfn );
	}

	/*
	signaler = ofa_igetter_get_signaler( priv->getter );
	dos_dend = ofo_dossier_get_exe_end( priv->dossier );
	count = g_list_length( entries );
	i = 0;
	bar = get_new_bar( self, "p6-future" );
	gtk_widget_show_all( priv->p6_page );
	if( count == 0 ){
		g_signal_emit_by_name( bar, "my-text", "0/0" );
	}

	gtk_widget_show_all( GTK_WIDGET( bar ));
	*/

	ofo_entry_free_dataset( entries );

	return( ok );
}

/*
 * #1542: the closing of the 2017 exercice has been wrong, and the accounts
 * and ledgers balances have been wrongly initialized in 2018. These balances
 * have to be recomputed here.
 *
 * This bug has been detected, and fixed, before the closing of the first quarter.
 *
 * So the dossier is to be remediated if :
 * - the exercice is 2018 at least
 * - the remediation has not already been done.
 */
static gboolean
remediate_1542( ofaHub *hub )
{
	static const gchar *keyed_data = "todo_1542";
	gboolean tobe_remediated, remediated;
	ofoDossier *dossier;
	const GDate *begin;
	GDate *begin_2018, *end_2018;
	GDate today;
	gchar *stoday;
	ofoData *keyed;

	tobe_remediated = TRUE;
	remediated = TRUE;
	dossier = ofa_hub_get_dossier( hub );
	begin = ofo_dossier_get_exe_begin( dossier );

	begin_2018 = g_date_new_dmy( 1, G_DATE_JANUARY, 2018 );
	end_2018 = g_date_new_dmy( 31, G_DATE_DECEMBER, 2018 );

	if( tobe_remediated && my_date_compare( begin, begin_2018 ) < 1 ){
		tobe_remediated = FALSE;
	}
	if( tobe_remediated && my_date_compare( end_2018, begin ) > 1 ){
		tobe_remediated = FALSE;
	}
	if( tobe_remediated ){
		keyed = ofo_data_get_by_key( OFA_IGETTER( hub ), keyed_data );
		if( keyed ){
			tobe_remediated = FALSE;
		}
	}
	/* remediation:
	 * - reset accounts and ledgers soldes
	 * - get all entries after the beginning of the exercice
	 * - recompute
	 * directly using database update to prevent all signaling stuff
	 */
	tobe_remediated = FALSE;
	if( tobe_remediated ){
		/* reset accounts soldes */
		if( remediated ){
			/*
			remediated = exec_query( "UPDATE OFA_T_ACCOUNTS SET "
					"	ACC_CR_DEBIT=0,ACC_CR_CREDIT=0,ACC_CV_DEBIT=0,ACC_CV_CREDIT=0,"
					"	ACC_FR_DEBIT=0,ACC_FR_CREDIT=0,ACC_FV_DEBIT=0,ACC_FV_CREDIT=0" );
					*/
		}
		/* reset accounts ledgers */
		if( remediated ){
			/*
			remediated = exec_query( "UPDATE OFA_T_LEDGERS_CUR SET "
				"	LED_CR_DEBIT=0,LED_CR_CREDIT=0,LED_CV_DEBIT=0,LED_CV_CREDIT=0,"
				"	LED_FR_DEBIT=0,LED_FR_CREDIT=0,LED_FV_DEBIT=0,LED_FV_CREDIT=0" );
				*/
		}
		remediated = ofa_hub_remediate_recompute_balances( hub, NULL, NULL );

		/* write un flag record with the current sys date in DATA table */
		if( remediated ){
			keyed = ofo_data_new( OFA_IGETTER( hub ));
			ofo_data_set_key( keyed, keyed_data );
			my_date_set_now( &today );
			stoday = my_date_to_str( &today, MY_DATE_SQL );
			ofo_data_set_content( keyed, stoday );
			g_free( stoday );
			remediated = ofo_data_insert( keyed );
		}
	}

	g_free( begin_2018 );
	g_free( end_2018 );

	return( remediated );
}

/**
 * ofa_hub_remediate_logicals:
 * @hub: this #ofaHub instance.
 *
 * Deals here with remediations which cannot (or shouldn't) be handled
 * by the DB Model.
 *
 * Returns: %TRUE if the dossier has been successully remediated, or
 * didn't need to, %FALSE if an error occured.
 */
gboolean
ofa_hub_remediate_logicals( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_hub_remediate_logicals";
	gboolean remediated;

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	remediated = remediate_1542( hub );

	return( remediated );
}

/*
 * ofa_hub_remediate_settings:
 * @hub: this #ofaHub instance.
 *
 * When opening the dossier, make sure the settings are up to date
 * (this may not be the case when the dossier has just been restored
 *  or created)
 *
 * The datas found in the dossier database take precedence over those
 * read from dossier settings. This is because dossier database is
 * (expected to be) updated via the Openbook software suite and so be
 * controlled, while the dossier settings may easily be tweaked by the
 * user.
 *
 * Returns: %TRUE if the dossier has been successully remediated.
 */
gboolean
ofa_hub_remediate_settings( ofaHub *hub )
{
	static const gchar *thisfn = "ofa_hub_remediate_settings";
	ofaIDBConnect *cnx;
	ofoDossier *dossier;
	gboolean db_current, settings_current, remediated;
	const GDate *db_begin, *db_end, *settings_begin, *settings_end;
	ofaIDBExerciceMeta *period;
	gchar *sdbbegin, *sdbend, *ssetbegin, *ssetend;

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	remediated = FALSE;
	dossier = ofa_hub_get_dossier( hub );
	cnx = ofa_hub_get_connect( hub );

	/* data from db */
	db_current = ofo_dossier_is_current( dossier );
	db_begin = ofo_dossier_get_exe_begin( dossier );
	db_end = ofo_dossier_get_exe_end( dossier );

	/* data from settings */
	period = ofa_idbconnect_get_exercice_meta( cnx );
	settings_current = ofa_idbexercice_meta_get_current( period );
	settings_begin = ofa_idbexercice_meta_get_begin_date( period );
	settings_end = ofa_idbexercice_meta_get_end_date( period );

	sdbbegin = my_date_to_str( db_begin, MY_DATE_SQL );
	sdbend = my_date_to_str( db_end, MY_DATE_SQL );
	ssetbegin = my_date_to_str( settings_begin, MY_DATE_SQL );
	ssetend = my_date_to_str( settings_end, MY_DATE_SQL );

	g_debug( "%s: db_current=%s, db_begin=%s, db_end=%s, settings_current=%s, settings_begin=%s, settings_end=%s",
			thisfn,
			db_current ? "True":"False", sdbbegin, sdbend,
			settings_current ? "True":"False", ssetbegin, ssetend );

	g_free( sdbbegin );
	g_free( sdbend );
	g_free( ssetbegin );
	g_free( ssetend );

	/* update settings if not equal */
	if( db_current != settings_current ||
			my_date_compare_ex( db_begin, settings_begin, TRUE ) != 0 ||
			my_date_compare_ex( db_end, settings_end, FALSE ) != 0 ){

		g_debug( "%s: remediating settings", thisfn );

		ofa_idbexercice_meta_set_current( period, db_current );
		ofa_idbexercice_meta_set_begin_date( period, db_begin );
		ofa_idbexercice_meta_set_end_date( period, db_end );
		ofa_idbexercice_meta_update_settings( period );

		remediated = TRUE;

	} else {
		g_debug( "%s: nothing to do", thisfn );
	}

	return( remediated );
}
