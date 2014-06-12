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
#include "ui/ofo-account.h"
#include "ui/ofo-devise.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-model.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoJournalPrivate {

	/* sgbd data
	 */
	gchar     *mnemo;
	gchar     *label;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
	GList     *exes;					/* exercices */
	GList     *amounts;					/* balances per currency */
};

typedef struct {
	gint       exe_id;
	gchar     *devise;
	gdouble    clo_deb;
	gdouble    clo_cre;
	gdouble    deb;
	gdouble    cre;
}
	sDetailDev;

typedef struct {
	gint       exe_id;
	GDate      last_clo;
}
	sDetailExe;

G_DEFINE_TYPE( ofoJournal, ofo_journal, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, journal )

static gboolean st_connected = FALSE;

static void        init_global_handlers( const ofoDossier *dossier );
static void        on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data );
static void        on_new_journal_entry( ofoDossier *dossier, ofoEntry *entry );
static void        on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data );
static GList      *journal_load_dataset( void );
static ofoJournal *journal_find_by_mnemo( GList *set, const gchar *mnemo );
static gint        journal_count_for_devise( const ofoSgbd *sgbd, const gchar *devise );
static sDetailDev *journal_find_dev_by_code( const ofoJournal *journal, gint exe_id, const gchar *devise );
static sDetailExe *journal_find_exe_by_id( const ofoJournal *journal, gint exe_id );
static sDetailDev *journal_new_dev_with_code( ofoJournal *journal, gint exe_id, const gchar *devise );
static gboolean    journal_do_insert( ofoJournal *journal, const ofoSgbd *sgbd, const gchar *user );
static gboolean    journal_insert_main( ofoJournal *journal, const ofoSgbd *sgbd, const gchar *user );
static gboolean    journal_do_update( ofoJournal *journal, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user );
static gboolean    journal_do_update_detail_dev( const ofoJournal *journal, sDetailDev *detail, const ofoSgbd *sgbd );
static gboolean    journal_do_update_detail_exe( const ofoJournal *journal, sDetailExe *detail, const ofoSgbd *sgbd );
static gboolean    journal_do_delete( ofoJournal *journal, const ofoSgbd *sgbd );
static gint        journal_cmp_by_mnemo( const ofoJournal *a, const gchar *mnemo );
static gboolean    journal_do_drop_content( const ofoSgbd *sgbd );

static void
free_detail_dev( sDetailDev *detail )
{
	g_free( detail->devise );
	g_free( detail );
}

static void
ofo_journal_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_journal_finalize";
	ofoJournalPrivate *priv;

	priv = OFO_JOURNAL( instance )->private;

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->mnemo, priv->label );

	/* free data members here */
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv->maj_user );
	g_list_free_full( priv->amounts, ( GDestroyNotify ) free_detail_dev );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_journal_parent_class )->finalize( instance );
}

static void
ofo_journal_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_JOURNAL( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_journal_parent_class )->dispose( instance );
}

static void
ofo_journal_init( ofoJournal *self )
{
	static const gchar *thisfn = "ofo_journal_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoJournalPrivate, 1 );
}

static void
ofo_journal_class_init( ofoJournalClass *klass )
{
	static const gchar *thisfn = "ofo_journal_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoJournalPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_journal_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_journal_finalize;
}

static void
init_global_handlers( const ofoDossier *dossier )
{
	OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

	if( !st_connected ){
		g_signal_connect( G_OBJECT( dossier ),
					OFA_SIGNAL_NEW_OBJECT, G_CALLBACK( on_new_object ), NULL );
		g_signal_connect( G_OBJECT( dossier ),
					OFA_SIGNAL_VALIDATED_ENTRY, G_CALLBACK( on_validated_entry ), NULL );
		st_connected = TRUE;
	}
}

static void
on_new_object( ofoDossier *dossier, ofoBase *object, gpointer user_data )
{
	if( OFO_IS_ENTRY( object )){
		on_new_journal_entry( dossier, OFO_ENTRY( object ));
	}
}

/*
 * recording a new entry is necessarily on the current exercice
 */
static void
on_new_journal_entry( ofoDossier *dossier, ofoEntry *entry )
{
	static const gchar *thisfn = "ofo_journal_on_new_journal_entry";
	gint current;
	const gchar *mnemo, *currency;
	ofoJournal *journal;
	sDetailDev *detail;

	current = ofo_dossier_get_current_exe_id( dossier );
	mnemo = ofo_entry_get_journal( entry );
	journal = ofo_journal_get_by_mnemo( dossier, mnemo );

	if( journal ){
		currency = ofo_entry_get_devise( entry );
		detail = journal_find_dev_by_code( journal, current, currency );
		if( !detail ){
			detail = g_new0( sDetailDev, 1 );
			detail->exe_id = current;
			detail->devise = g_strdup( currency );
			detail->deb = 0.0;
			detail->cre = 0.0;
			detail->clo_deb = 0.0;
			detail->clo_cre = 0.0;
			journal->private->amounts = g_list_prepend( journal->private->amounts, detail );
		}
		detail->deb += ofo_entry_get_debit( entry );
		detail->cre += ofo_entry_get_credit( entry );

		if( journal_do_update_detail_dev( journal, detail, ofo_dossier_get_sgbd( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					OFA_SIGNAL_UPDATED_OBJECT, g_object_ref( journal ), NULL );
		}

	} else {
		g_warning( "%s: journal not found: %s", thisfn, mnemo );
	}

}

/*
 * an entry is validated, either individually or as the result of the
 * closing of a journal
 */
static void
on_validated_entry( ofoDossier *dossier, ofoEntry *entry, void *user_data )
{
	static const gchar *thisfn = "ofo_journal_on_validated_entry";
	gint exe_id;
	const gchar *currency, *mnemo;
	ofoJournal *journal;
	sDetailDev *detail;
	gdouble debit, credit;

	mnemo = ofo_entry_get_journal( entry );
	journal = ofo_journal_get_by_mnemo( dossier, mnemo );
	if( journal ){

		exe_id = ofo_dossier_get_exe_by_date( dossier, ofo_entry_get_deffect( entry ));
		currency = ofo_entry_get_devise( entry );
		detail = journal_find_dev_by_code( journal, exe_id, currency );
		/* the entry has necessarily be already recorded while in rough
		 * status */
		g_return_if_fail( detail );

		debit = ofo_entry_get_debit( entry );
		detail->clo_deb += debit;
		detail->deb -= debit;
		credit = ofo_entry_get_credit( entry );
		detail->clo_cre += credit;
		detail->cre -= credit;

		if( journal_do_update_detail_dev( journal, detail, ofo_dossier_get_sgbd( dossier ))){
			g_signal_emit_by_name(
					G_OBJECT( dossier ),
					OFA_SIGNAL_UPDATED_OBJECT, g_object_ref( journal ), NULL );
		}

	} else {
		g_warning( "%s: journal not found: %s", thisfn, mnemo );
	}
}

/**
 * ofo_journal_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoJournal journals, ordered by ascending
 * mnemonic. The returned list is owned by the #ofoJournal class, and
 * should not be freed by the caller.
 *
 * Notes:
 * - the list is not sorted
 * - we are loading all the content of the entity, i.e. the list of
 *   journals plus all the list of detail rows
 */
GList *
ofo_journal_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	init_global_handlers( dossier );

	return( st_global->dataset );
}

static GList *
journal_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoJournal *journal;
	const ofoSgbd *sgbd;
	GList *dataset, *iset;
	gchar *query;
	sDetailDev *balance;
	sDetailExe *exercice;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT JOU_MNEMO,JOU_LABEL,JOU_NOTES,"
			"	JOU_MAJ_USER,JOU_MAJ_STAMP "
			"	FROM OFA_T_JOURNAUX" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		journal = ofo_journal_new();
		ofo_journal_set_mnemo( journal, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_journal_set_label( journal, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_journal_set_notes( journal, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_journal_set_maj_user( journal, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_journal_set_maj_stamp( journal, my_utils_stamp_from_str(( gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, journal );
	}

	ofo_sgbd_free_result( result );

	/* then load the details
	 */
	for( iset=dataset ; iset ; iset=iset->next ){

		journal = OFO_JOURNAL( iset->data );

		query = g_strdup_printf(
				"SELECT JOU_EXE_ID,JOU_DEV_CODE,"
				"	JOU_DEV_CLO_DEB,JOU_DEV_CLO_CRE,JOU_DEV_DEB,JOU_DEV_CRE "
				"	FROM OFA_T_JOURNAUX_DEV "
				"	WHERE JOU_MNEMO='%s'",
						ofo_journal_get_mnemo( journal ));

		result = ofo_sgbd_query_ex( sgbd, query );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			balance = g_new0( sDetailDev, 1 );
			balance->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			balance->devise = g_strdup(( gchar * ) icol->data );
			icol = icol->next;
			balance->clo_deb = g_ascii_strtod(( gchar * ) icol->data, NULL );
			icol = icol->next;
			balance->clo_cre = g_ascii_strtod(( gchar * ) icol->data, NULL );
			icol = icol->next;
			balance->deb = g_ascii_strtod(( gchar * ) icol->data, NULL );
			icol = icol->next;
			balance->cre = g_ascii_strtod(( gchar * ) icol->data, NULL );

			journal->private->amounts = g_list_prepend( journal->private->amounts, balance );
		}

		ofo_sgbd_free_result( result );

		query = g_strdup_printf(
				"SELECT JOU_EXE_ID,JOU_EXE_LAST_CLO "
				"	FROM OFA_T_JOURNAUX_EXE "
				"	WHERE JOU_MNEMO='%s'",
						ofo_journal_get_mnemo( journal ));

		result = ofo_sgbd_query_ex( sgbd, query );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			exercice = g_new0( sDetailExe, 1 );
			exercice->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			memcpy( &exercice->last_clo, my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));

			journal->private->exes = g_list_prepend( journal->private->exes, exercice );
		}

		ofo_sgbd_free_result( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_journal_get_by_mnemo:
 *
 * Returns: the searched journal, or %NULL.
 *
 * The returned object is owned by the #ofoJournal class, and should
 * not be unreffed by the caller.
 */
ofoJournal *
ofo_journal_get_by_mnemo( const ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	init_global_handlers( dossier );

	return( journal_find_by_mnemo( st_global->dataset, mnemo ));
}

static ofoJournal *
journal_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) journal_cmp_by_mnemo );
	if( found ){
		return( OFO_JOURNAL( found->data ));
	}

	return( NULL );
}

/**
 * ofo_journal_use_devise:
 *
 * Returns: %TRUE if a recorded journal makes use of the specified
 * currency.
 */
gboolean
ofo_journal_use_devise( const ofoDossier *dossier, const gchar *devise )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( journal_count_for_devise( ofo_dossier_get_sgbd( dossier ), devise ) > 0 );
}

static gint
journal_count_for_devise( const ofoSgbd *sgbd, const gchar *devise )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_JOURNAUX_DEV "
				"	WHERE JOU_DEV_CODE='%s'",
					devise );

	count = 0;
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
 * ofo_journal_new:
 */
ofoJournal *
ofo_journal_new( void )
{
	ofoJournal *journal;

	journal = g_object_new( OFO_TYPE_JOURNAL, NULL );

	return( journal );
}

/**
 * ofo_journal_get_mnemo:
 */
const gchar *
ofo_journal_get_mnemo( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		return(( const gchar * ) journal->private->mnemo );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_journal_get_label:
 */
const gchar *
ofo_journal_get_label( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		return(( const gchar * ) journal->private->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_journal_get_notes:
 */
const gchar *
ofo_journal_get_notes( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		return(( const gchar * ) journal->private->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_journal_get_maj_user:
 */
const gchar *
ofo_journal_get_maj_user( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		return(( const gchar * ) journal->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_journal_get_maj_stamp:
 */
const GTimeVal *
ofo_journal_get_maj_stamp( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &journal->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_journal_get_last_entry:
 *
 * Returns the effect date of the most recent entry written in this
 * journal, or NULL.
 */
const GDate *
ofo_journal_get_last_entry( const ofoJournal *journal )
{
	static GDate deffet;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( journal && OFO_IS_JOURNAL( journal ), NULL );

	g_date_clear( &deffet, 1 );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		query = g_strdup_printf(
				"SELECT MAX(ECR_DEFFET) FROM OFA_T_ECRITURES "
				"	WHERE ECR_JOU_MNEMO='%s'", ofo_journal_get_mnemo( journal ));

		result = ofo_sgbd_query_ex(
						ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )), query );
		g_free( query );

		if( result ){
			icol = ( GSList * ) result->data;
			memcpy( &deffet, my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));
			ofo_sgbd_free_result( result );
		}
	}

	return( &deffet );
}

/**
 * ofo_journal_get_last_closing:
 *
 * Returns the last closing date, all exercices considered, for this
 * journal, or NULL
 */
const GDate *
ofo_journal_get_last_closing( const ofoJournal *journal )
{
	static GDate dclosing;
	gchar *query;
	GSList *result, *icol;

	g_return_val_if_fail( journal && OFO_IS_JOURNAL( journal ), NULL );

	g_date_clear( &dclosing, 1 );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		query = g_strdup_printf(
				"SELECT MAX(JOU_EXE_LAST_CLO) FROM OFA_T_JOURNAUX_EXE "
				"	WHERE JOU_MNEMO='%s'", ofo_journal_get_mnemo( journal ));

		result = ofo_sgbd_query_ex(
						ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )), query );
		g_free( query );

		if( result ){
			icol = ( GSList * ) result->data;
			memcpy( &dclosing, my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));
			ofo_sgbd_free_result( result );
		}
	}

	return( &dclosing );
}

/**
 * ofo_journal_get_clo_deb:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Returns the debit balance of this journal at the last closing for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_journal_get_clo_deb( const ofoJournal *journal, gint exe_id, const gchar *devise )
{
	sDetailDev *sdev;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), 0.0 );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_find_dev_by_code( journal, exe_id, devise );
		if( sdev ){
			return( sdev->clo_deb );
		}
	}

	return( 0.0 );
}

/**
 * ofo_journal_get_clo_cre:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Returns the credit balance of this journal at the last closing for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_journal_get_clo_cre( const ofoJournal *journal, gint exe_id, const gchar *devise )
{
	sDetailDev *sdev;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), 0.0 );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_find_dev_by_code( journal, exe_id, devise );
		if( sdev ){
			return( sdev->clo_cre );
		}
	}

	return( 0.0 );
}

/**
 * ofo_journal_get_deb:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Returns the current debit balance of this journal for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_journal_get_deb( const ofoJournal *journal, gint exe_id, const gchar *devise )
{
	sDetailDev *sdev;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), 0.0 );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_find_dev_by_code( journal, exe_id, devise );
		if( sdev ){
			return( sdev->deb );
		}
	}

	return( 0.0 );
}

/**
 * ofo_journal_get_cre:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Returns the current credit balance of this journal for
 * the currency specified, or zero if not found.
 */
gdouble
ofo_journal_get_cre( const ofoJournal *journal, gint exe_id, const gchar *devise )
{
	sDetailDev *sdev;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), 0.0 );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_find_dev_by_code( journal, exe_id, devise );
		if( sdev ){
			return( sdev->cre );
		}
	}

	return( 0.0 );
}

/**
 * ofo_journal_get_cloture:
 */
const GDate *
ofo_journal_get_cloture( const ofoJournal *journal, gint exe_id )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sexe = journal_find_exe_by_id( journal, exe_id );
		if( sexe ){
			return(( const GDate * ) &sexe->last_clo );
		}
	}

	return( NULL );
}

static sDetailDev *
journal_find_dev_by_code( const ofoJournal *journal, gint exe_id, const gchar *devise )
{
	GList *idet;
	sDetailDev *sdet;

	for( idet=journal->private->amounts ; idet ; idet=idet->next ){
		sdet = ( sDetailDev * ) idet->data;
		if( sdet->exe_id == exe_id && !g_utf8_collate( sdet->devise, devise )){
			return( sdet );
		}
	}

	return( NULL );
}

static sDetailExe *
journal_find_exe_by_id( const ofoJournal *journal, gint exe_id )
{
	GList *idet;
	sDetailExe *sdet;

	for( idet=journal->private->exes ; idet ; idet=idet->next ){
		sdet = ( sDetailExe * ) idet->data;
		if( sdet->exe_id == exe_id ){
			return( sdet );
		}
	}

	return( NULL );
}

/**
 * ofo_journal_has_entries:
 */
gboolean
ofo_journal_has_entries( const ofoJournal *journal )
{
	gboolean ok;
	const gchar *mnemo;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		mnemo = ofo_journal_get_mnemo( journal );

		ok = ofo_entry_use_journal( OFO_DOSSIER( st_global->dossier ), mnemo );

		return( ok );
	}

	return( FALSE );
}

/**
 * ofo_journal_is_deletable:
 *
 * A journal is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its amounts must be
 * nuls for all currencies.
 *
 * There is no need to test for the last closing date as this is not
 * relevant here: even if set, they does not mean that there has been
 * any entries recorded on the journal.
 *
 * More: a journal should not be deleted while it is referenced by a
 * model or an entry.
 */
gboolean
ofo_journal_is_deletable( const ofoJournal *journal, const ofoDossier *dossier )
{
	gboolean ok;
	GList *ic;
	gint exe_id;
	sDetailDev *detail;
	const gchar *mnemo;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		ok = TRUE;
		exe_id = ofo_dossier_get_current_exe_id( dossier );

		for( ic=journal->private->amounts ; ic && ok ; ic=ic->next ){
			detail = ( sDetailDev * ) ic->data;
			if( detail->exe_id == exe_id ){
				ok &= detail->clo_deb == 0.0 && detail->clo_cre == 0.0 &&
						detail->deb == 0.0 && detail->cre == 0.0;
			}
		}

		mnemo = ofo_journal_get_mnemo( journal );

		ok &= !ofo_entry_use_journal( dossier, mnemo ) &&
				!ofo_model_use_journal( dossier, mnemo );

		return( ok );
	}

	return( FALSE );
}

/**
 * ofo_journal_is_valid:
 *
 * Returns: %TRUE if the provided data makes the ofoJournal a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_journal_is_valid( const gchar *mnemo, const gchar *label )
{
	return( mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 ));
}

/**
 * ofo_journal_set_mnemo:
 */
void
ofo_journal_set_mnemo( ofoJournal *journal, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_free( journal->private->mnemo );
		journal->private->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_journal_set_label:
 */
void
ofo_journal_set_label( ofoJournal *journal, const gchar *label )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_free( journal->private->label );
		journal->private->label = g_strdup( label );
	}
}

/**
 * ofo_journal_set_notes:
 */
void
ofo_journal_set_notes( ofoJournal *journal, const gchar *notes )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_free( journal->private->notes );
		journal->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_journal_set_maj_user:
 */
void
ofo_journal_set_maj_user( ofoJournal *journal, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_free( journal->private->maj_user );
		journal->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_journal_set_maj_stamp:
 */
void
ofo_journal_set_maj_stamp( ofoJournal *journal, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		memcpy( &journal->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_journal_set_clo_deb:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Set the debit balance of this journal at the last closing for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_journal_set_clo_deb( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount )
{
	sDetailDev *sdev;

	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_new_dev_with_code( journal, exe_id, devise );
		g_return_if_fail( sdev );

		sdev->clo_deb += amount;
	}
}

/**
 * ofo_journal_set_clo_cre:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Set the credit balance of this journal at the last closing for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_journal_set_clo_cre( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount )
{
	sDetailDev *sdev;

	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_new_dev_with_code( journal, exe_id, devise );
		g_return_if_fail( sdev );

		sdev->clo_cre += amount;
	}
}

/**
 * ofo_journal_set_deb:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Set the current debit balance of this journal for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_journal_set_deb( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount )
{
	sDetailDev *sdev;

	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_new_dev_with_code( journal, exe_id, devise );
		g_return_if_fail( sdev );

		sdev->deb += amount;
	}
}

/**
 * ofo_journal_set_cre:
 * @journal:
 * @exe_id:
 * @dev_id:
 *
 * Set the current credit balance of this journal for
 * the currency specified.
 *
 * Creates an occurrence of the detail record if it didn't exist yet.
 */
void
ofo_journal_set_cre( ofoJournal *journal, gint exe_id, const gchar *devise, gdouble amount )
{
	sDetailDev *sdev;

	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		sdev = journal_new_dev_with_code( journal, exe_id, devise );
		g_return_if_fail( sdev );

		sdev->cre += amount;
	}
}

/**
 * ofo_journal_set_cloture:
 */
/*
void
ofo_journal_set_cloture( ofoJournal *journal, const GDate *date )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->private->dispose_has_run ){

		memcpy( &journal->private->cloture, date, sizeof( GDate ));
	}
}*/

static sDetailDev *
journal_new_dev_with_code( ofoJournal *journal, gint exe_id, const gchar *devise )
{
	sDetailDev *sdet;

	sdet = journal_find_dev_by_code( journal, exe_id, devise );

	if( !sdet ){
		sdet = g_new0( sDetailDev, 1 );

		sdet->clo_deb = 0.0;
		sdet->clo_cre = 0.0;
		sdet->deb = 0.0;
		sdet->cre = 0.0;
		sdet->exe_id = exe_id;
		sdet->devise = g_strdup( devise );

		journal->private->amounts = g_list_prepend( journal->private->amounts, sdet );
	}

	return( sdet );
}

/**
 * ofo_journal_close:
 *
 * - all entries in rough status and whose effect date in less or equal
 *   to the closing date and are written in this journal are validated
 */
gboolean
ofo_journal_close( ofoJournal *journal, const GDate *closing )
{
	static const gchar *thisfn = "ofo_journal_close";
	gboolean ok;
	gint exe_id;
	sDetailExe *sexe;

	g_return_val_if_fail( journal && OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( closing && g_date_valid( closing ), FALSE );

	g_debug( "%s: journal=%p, closing=%p", thisfn, ( void * ) journal, ( void * ) closing );

	ok = FALSE;

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		/* be sure account handlers are connected */
		ofo_account_connect_handlers( OFO_DOSSIER( st_global->dossier ));

		if( ofo_entry_validate_by_journal(
						OFO_DOSSIER( st_global->dossier ),
						ofo_journal_get_mnemo( journal ),
						closing )){

			exe_id = ofo_dossier_get_current_exe_id( OFO_DOSSIER( st_global->dossier ));
			sexe = journal_find_exe_by_id( journal, exe_id );

			if( !sexe ){
				sexe = g_new0( sDetailExe, 1 );
				sexe->exe_id = exe_id;
				journal->private->exes = g_list_prepend( journal->private->exes, sexe );
			}

			memcpy( &sexe->last_clo, closing, sizeof( GDate ));

			if( journal_do_update_detail_exe(
						journal,
						sexe,
						ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier )))){

				g_signal_emit_by_name(
						G_OBJECT( st_global->dossier ),
						OFA_SIGNAL_UPDATED_OBJECT,
						g_object_ref( journal ),
						NULL );

				ok = TRUE;
			}
		}
	}

	return( ok );
}

/**
 * ofo_journal_insert:
 *
 * Only insert here a new journal, so only the main properties
 */
gboolean
ofo_journal_insert( ofoJournal *journal, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_insert";

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		init_global_handlers( dossier );

		if( journal_do_insert(
					journal,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_ADD_TO_DATASET( st_global, journal );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
journal_do_insert( ofoJournal *journal, const ofoSgbd *sgbd, const gchar *user )
{
	return( journal_insert_main( journal, sgbd, user ));
}

static gboolean
journal_insert_main( ofoJournal *journal, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	ok = FALSE;
	label = my_utils_quote( ofo_journal_get_label( journal ));
	notes = my_utils_quote( ofo_journal_get_notes( journal ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_JOURNAUX" );

	g_string_append_printf( query,
			"	(JOU_MNEMO,JOU_LABEL,JOU_NOTES,"
			"	JOU_MAJ_USER, JOU_MAJ_STAMP) VALUES ('%s','%s',",
			ofo_journal_get_mnemo( journal ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')",
			user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_journal_set_maj_user( journal, user );
		ofo_journal_set_maj_stamp( journal, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_journal_update:
 *
 * We only update here the user properties, so do not care with the
 * details of balances per currency.
 */
gboolean
ofo_journal_update( ofoJournal *journal, const ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_journal_update";

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p, prev_mnemo=%s",
				thisfn, ( void * ) journal, ( void * ) dossier, prev_mnemo );

		init_global_handlers( dossier );

		if( journal_do_update(
					journal,
					prev_mnemo,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_UPDATE_DATASET( st_global, journal, prev_mnemo );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
journal_do_update( ofoJournal *journal, const gchar *prev_mnemo, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	ok = FALSE;
	label = my_utils_quote( ofo_journal_get_label( journal ));
	notes = my_utils_quote( ofo_journal_get_notes( journal ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_JOURNAUX SET " );

	g_string_append_printf( query, "JOU_MNEMO='%s',", ofo_journal_get_mnemo( journal ));
	g_string_append_printf( query, "JOU_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "JOU_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "JOU_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	JOU_MAJ_USER='%s',JOU_MAJ_STAMP='%s'"
			"	WHERE JOU_MNEMO='%s'", user, stamp, prev_mnemo );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_journal_set_maj_user( journal, user );
		ofo_journal_set_maj_stamp( journal, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

static gboolean
journal_do_update_detail_dev( const ofoJournal *journal, sDetailDev *detail, const ofoSgbd *sgbd )
{
	gchar *query;
	gchar *deb, *cre, *clo_deb, *clo_cre;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX_DEV "
			"	WHERE JOU_MNEMO='%s' AND JOU_EXE_ID=%d AND JOU_DEV_CODE='%s'",
					ofo_journal_get_mnemo( journal ),
					detail->exe_id,
					detail->devise );

	ofo_sgbd_query_ignore( sgbd, query );
	g_free( query );

	deb = my_utils_sql_from_double( ofo_journal_get_deb( journal, detail->exe_id, detail->devise ));
	cre = my_utils_sql_from_double( ofo_journal_get_cre( journal, detail->exe_id, detail->devise ));
	clo_deb = my_utils_sql_from_double( ofo_journal_get_clo_deb( journal, detail->exe_id, detail->devise ));
	clo_cre = my_utils_sql_from_double( ofo_journal_get_clo_cre( journal, detail->exe_id, detail->devise ));

	query = g_strdup_printf(
					"INSERT INTO OFA_T_JOURNAUX_DEV "
					"	(JOU_MNEMO,JOU_EXE_ID,JOU_DEV_CODE,"
					"	JOU_DEV_CLO_DEB,JOU_DEV_CLO_CRE,"
					"	JOU_DEV_DEB,JOU_DEV_CRE) VALUES "
					"	('%s',%d,'%s',%s,%s,%s,%s)",
							ofo_journal_get_mnemo( journal ),
							detail->exe_id,
							detail->devise,
							clo_deb,
							clo_cre,
							deb,
							cre );

	ok = ofo_sgbd_query( sgbd, query );

	g_free( deb );
	g_free( cre );
	g_free( clo_deb );
	g_free( clo_cre );
	g_free( query );

	return( ok );
}

static gboolean
journal_do_update_detail_exe( const ofoJournal *journal, sDetailExe *detail, const ofoSgbd *sgbd )
{
	gchar *query;
	gchar *sdate;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX_EXE "
			"	WHERE JOU_MNEMO='%s' AND JOU_EXE_ID=%d",
					ofo_journal_get_mnemo( journal ),
					detail->exe_id );

	ofo_sgbd_query_ignore( sgbd, query );
	g_free( query );

	sdate = my_utils_sql_from_date( &detail->last_clo );

	query = g_strdup_printf(
					"INSERT INTO OFA_T_JOURNAUX_EXE "
					"	(JOU_MNEMO,JOU_EXE_ID,JOU_EXE_LAST_CLO) "
					"	VALUES "
					"	('%s',%d,'%s')",
							ofo_journal_get_mnemo( journal ),
							detail->exe_id,
							sdate );

	ok = ofo_sgbd_query( sgbd, query );

	g_free( sdate );
	g_free( query );

	return( ok );
}

/**
 * ofo_journal_delete:
 *
 * Take care of deleting both main and detail records.
 */
gboolean
ofo_journal_delete( ofoJournal *journal, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_delete";

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_journal_is_deletable( journal, dossier ), FALSE );

	if( !OFO_BASE( journal )->prot->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		init_global_handlers( dossier );

		if( journal_do_delete(
					journal,
					ofo_dossier_get_sgbd( dossier ))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, journal );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
journal_do_delete( ofoJournal *journal, const ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX WHERE JOU_MNEMO='%s'",
					ofo_journal_get_mnemo( journal ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX_DEV WHERE JOU_MNEMO='%s'",
					ofo_journal_get_mnemo( journal ));

	ok &= ofo_sgbd_query( sgbd, query );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX_EXE WHERE JOU_MNEMO='%s'",
					ofo_journal_get_mnemo( journal ));

	ok &= ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
journal_cmp_by_mnemo( const ofoJournal *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_journal_get_mnemo( a ), mnemo ));
}

/**
 * ofo_journal_get_csv:
 */
GSList *
ofo_journal_get_csv( const ofoDossier *dossier )
{
	GList *set, *exe, *amount;
	GSList *lines;
	gchar *str, *stamp, *sdfin, *sdclo;
	ofoJournal *journal;
	sDetailExe *sexe;
	sDetailDev *sdev;
	const GDate *date;
	const gchar *notes, *muser;

	OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

	lines = NULL;

	str = g_strdup_printf( "1;Mnemo;Label;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "2;Mnemo;Exe;Closed" );
	lines = g_slist_prepend( lines, str );

	str = g_strdup_printf( "3;Mnemo;Exe;Currency;CloDeb;CloCre;Deb;Cre" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		journal = OFO_JOURNAL( set->data );

		notes = ofo_journal_get_notes( journal );
		muser = ofo_journal_get_maj_user( journal );
		stamp = my_utils_str_from_stamp( ofo_journal_get_maj_stamp( journal ));

		str = g_strdup_printf( "1;%s;%s;%s;%s;%s",
				ofo_journal_get_mnemo( journal ),
				ofo_journal_get_label( journal ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( stamp );

		lines = g_slist_prepend( lines, str );

		for( exe=journal->private->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;

			date = ofo_dossier_get_exe_fin( dossier, sexe->exe_id );
			if( date && g_date_valid( date )){
				sdfin = my_utils_sql_from_date( date );
			} else {
				sdfin = g_strdup( "" );
			}

			if( g_date_valid( &sexe->last_clo )){
				sdclo = my_utils_sql_from_date( &sexe->last_clo );
			} else {
				sdclo = g_strdup( "" );
			}

			str = g_strdup_printf( "2;%s;%s;%s",
					ofo_journal_get_mnemo( journal ),
					sdfin,
					sdclo );

			g_free( sdfin );
			g_free( sdclo );

			lines = g_slist_prepend( lines, str );
		}

		for( amount=journal->private->amounts ; amount ; amount=amount->next ){
			sdev = ( sDetailDev * ) amount->data;

			date = ofo_dossier_get_exe_fin( dossier, sdev->exe_id );
			if( date && g_date_valid( date )){
				sdfin = my_utils_sql_from_date( date );
			} else {
				sdfin = g_strdup( "" );
			}

			str = g_strdup_printf( "3;%s;%s;%s;%.2lf;%.2lf;%.2lf;%.2lf",
					ofo_journal_get_mnemo( journal ),
					sdfin,
					sdev->devise,
					ofo_journal_get_clo_deb( journal, sdev->exe_id, sdev->devise ),
					ofo_journal_get_clo_cre( journal, sdev->exe_id, sdev->devise ),
					ofo_journal_get_deb( journal, sdev->exe_id, sdev->devise ),
					ofo_journal_get_cre( journal, sdev->exe_id, sdev->devise ));

			g_free( sdfin );

			lines = g_slist_prepend( lines, str );
		}
	}

	return( g_slist_reverse( lines ));
}

/**
 * ofo_journal_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - journal mnemo
 * - label
 * - notes (opt)
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_journal_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_journal_import_csv";
	ofoJournal *journal;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	const gchar *str;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			journal = ofo_journal_new();
			ico=ili->data;

			/* journal mnemo */
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty mnemo", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_journal_set_mnemo( journal, str );

			/* journal label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_journal_set_label( journal, str );

			/* notes
			 * we are tolerant on the last field... */
			ico = ico->next;
			if( ico ){
				str = ( const gchar * ) ico->data;
				if( str && g_utf8_strlen( str, -1 )){
					ofo_journal_set_notes( journal, str );
				}
			} else {
				continue;
			}

			new_set = g_list_prepend( new_set, journal );
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
		st_global->dataset = NULL;

		journal_do_drop_content( ofo_dossier_get_sgbd( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			ofo_journal_insert( OFO_JOURNAL( ise->data ), dossier );
		}

		g_list_free( new_set );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), OFA_SIGNAL_RELOADED_DATASET, OFO_TYPE_JOURNAL );

		st_global->send_signal_new = TRUE;
	}
}

static gboolean
journal_do_drop_content( const ofoSgbd *sgbd )
{
	return( ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_JOURNAUX" ) &&
			ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_JOURNAUX_DEV" ) &&
			ofo_sgbd_query( sgbd, "DELETE FROM OFA_T_JOURNAUX_EXE" ));
}
