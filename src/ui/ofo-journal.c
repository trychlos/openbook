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
#include "ui/ofo-dossier.h"
#include "ui/ofo-entry.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-model.h"

/* priv instance data
 */
struct _ofoJournalPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	gint     id;
	gchar   *mnemo;
	gchar   *label;
	gchar   *notes;
	gchar   *maj_user;
	GTimeVal maj_stamp;
	GList   *exes;						/* exercices */
	GList   *amounts;					/* balances per currency */
};

typedef struct {
	gint     exe_id;
	gint     dev_id;
	gdouble  clo_deb;
	gdouble  clo_cre;
	gdouble  deb;
	gdouble  cre;
}
	sDetailDev;

typedef struct {
	gint     exe_id;
	GDate    last_clo;
}
	sDetailExe;

G_DEFINE_TYPE( ofoJournal, ofo_journal, OFO_TYPE_BASE )

#define OFO_JOURNAL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_JOURNAL, ofoJournalPrivate))

OFO_BASE_DEFINE_GLOBAL( st_global, journal )

static GList         *journal_load_dataset( void );
static ofoJournal    *journal_find_by_id( GList *set, gint id );
static ofoJournal    *journal_find_by_mnemo( GList *set, const gchar *mnemo );
static gint           journal_count_for_devise( ofoSgbd *sgbd, gint dev_id );
static sDetailExe    *journal_find_exe_by_id( const ofoJournal *journal, gint exe_id );
static gboolean       journal_do_insert( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user );
static gboolean       journal_insert_main( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user );
static gboolean       journal_get_back_id( ofoJournal *journal, ofoSgbd *sgbd );
/*static gboolean       journal_insert_details( ofoJournal *journal, ofoSgbd *sgbd );*/
static gboolean       journal_do_update( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user );
static gboolean       journal_do_delete( ofoJournal *journal, ofoSgbd *sgbd );
static gint           journal_cmp_by_id( const ofoJournal *a, gconstpointer b );
static gint           journal_cmp_by_mnemo( const ofoJournal *a, const gchar *mnemo );
static gint           journal_cmp_by_ptr( const ofoJournal *a, const ofoJournal *b );

static void
ofo_journal_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_journal_finalize";
	ofoJournal *self;

	self = OFO_JOURNAL( instance );

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			self->priv->mnemo, self->priv->label );

	g_free( self->priv->mnemo );
	g_free( self->priv->label );
	g_free( self->priv->notes );
	g_free( self->priv->maj_user );

	g_list_free_full( self->priv->amounts, ( GDestroyNotify ) g_free );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_journal_parent_class )->finalize( instance );
}

static void
ofo_journal_dispose( GObject *instance )
{
	ofoJournal *self;

	self = OFO_JOURNAL( instance );

	if( !self->priv->dispose_has_run ){

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_journal_parent_class )->dispose( instance );
}

static void
ofo_journal_init( ofoJournal *self )
{
	static const gchar *thisfn = "ofo_journal_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_JOURNAL_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	self->priv->id = OFO_BASE_UNSET_ID;
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

/**
 * ofo_journal_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoJournal journals, ordered by ascending
 * mnemonic. The returned list is owned by the #ofoJournal class, and
 * should not be freed by the caller.
 *
 * Note: The list is returned (and maintained) sorted for debug
 * facility only. Any way, the display treeview (#ofoJournalsSet class)
 * makes use of a sortable model which doesn't care of the order of the
 * provided dataset.
 */
GList *
ofo_journal_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

	return( st_global->dataset );
}

static GList *
journal_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoJournal *journal;
	ofoSgbd *sgbd;
	GList *dataset, *iset;
	gchar *query;
	sDetailDev *balance;
	sDetailExe *exercice;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT JOU_ID,JOU_MNEMO,JOU_LABEL,JOU_NOTES,"
			"	JOU_MAJ_USER,JOU_MAJ_STAMP "
			"	FROM OFA_T_JOURNAUX "
			"	ORDER BY JOU_MNEMO ASC" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		journal = ofo_journal_new();
		ofo_journal_set_id( journal, atoi(( gchar * ) icol->data ));
		icol = icol->next;
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
				"SELECT JOU_EXE_ID,JOU_DEV_ID,"
				"	JOU_DEV_CLO_DEB,JOU_DEV_CLO_CRE,JOU_DEV_DEB,JOU_DEV_CRE "
				"	FROM OFA_T_JOURNAUX_DEV "
				"	WHERE JOU_ID=%d "
				"	ORDER BY JOU_EXE_ID ASC,JOU_DEV_ID ASC",
						ofo_journal_get_id( journal ));

		result = ofo_sgbd_query_ex( sgbd, query );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			balance = g_new0( sDetailDev, 1 );
			balance->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			balance->dev_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			balance->clo_deb = g_ascii_strtod(( gchar * ) icol->data, NULL );
			icol = icol->next;
			balance->clo_cre = g_ascii_strtod(( gchar * ) icol->data, NULL );
			icol = icol->next;
			balance->deb = g_ascii_strtod(( gchar * ) icol->data, NULL );
			icol = icol->next;
			balance->cre = g_ascii_strtod(( gchar * ) icol->data, NULL );

			journal->priv->amounts = g_list_prepend( journal->priv->amounts, balance );
		}

		ofo_sgbd_free_result( result );

		query = g_strdup_printf(
				"SELECT JOU_EXE_ID,JOU_EXE_LAST_CLO "
				"	FROM OFA_T_JOURNAUX_EXE "
				"	WHERE JOU_ID=%d "
				"	ORDER BY JOU_EXE_ID ASC",
						ofo_journal_get_id( journal ));

		result = ofo_sgbd_query_ex( sgbd, query );
		g_free( query );

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			exercice = g_new0( sDetailExe, 1 );
			exercice->exe_id = atoi(( gchar * ) icol->data );
			icol = icol->next;
			memcpy( &exercice->last_clo, my_utils_date_from_str(( gchar * ) icol->data ), sizeof( GDate ));

			journal->priv->exes = g_list_prepend( journal->priv->exes, exercice );
		}

		ofo_sgbd_free_result( result );
	}

	return( g_list_reverse( dataset ));
}

/**
 * ofo_journal_get_by_id:
 *
 * Returns: the searched journal, or %NULL.
 *
 * The returned object is owned by the #ofoJournal class, and should
 * not be unreffed by the caller.
 */
ofoJournal *
ofo_journal_get_by_id( const ofoDossier *dossier, gint id )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( id > 0, NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

	return( journal_find_by_id( st_global->dataset, id ));
}

static ofoJournal *
journal_find_by_id( GList *set, gint id )
{
	GList *found;

	found = g_list_find_custom(
				set, GINT_TO_POINTER( id ), ( GCompareFunc ) journal_cmp_by_id );
	if( found ){
		return( OFO_JOURNAL( found->data ));
	}

	return( NULL );
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

	OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

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
ofo_journal_use_devise( const ofoDossier *dossier, gint dev_id )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	return( journal_count_for_devise( ofo_dossier_get_sgbd( dossier ), dev_id ) > 0 );
}

static gint
journal_count_for_devise( ofoSgbd *sgbd, gint dev_id )
{
	gint count;
	gchar *query;
	GSList *result, *icol;

	query = g_strdup_printf(
				"SELECT COUNT(*) FROM OFA_T_JOURNAUX_DEV "
				"	WHERE JOU_DEV_ID=%d",
					dev_id );

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
 * ofo_journal_get_id:
 */
gint
ofo_journal_get_id( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), OFO_BASE_UNSET_ID );

	if( !journal->priv->dispose_has_run ){

		return( journal->priv->id );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_journal_get_mnemo:
 */
const gchar *
ofo_journal_get_mnemo( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !journal->priv->dispose_has_run ){

		return(( const gchar * ) journal->priv->mnemo );
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

	if( !journal->priv->dispose_has_run ){

		return(( const gchar * ) journal->priv->label );
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

	if( !journal->priv->dispose_has_run ){

		return(( const gchar * ) journal->priv->notes );
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

	if( !journal->priv->dispose_has_run ){

		return(( const gchar * ) journal->priv->maj_user );
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

	if( !journal->priv->dispose_has_run ){

		return(( const GTimeVal * ) &journal->priv->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_journal_get_cloture:
 */
const GDate *
ofo_journal_get_cloture( const ofoJournal *journal, gint exe_id )
{
	sDetailExe *sexe;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !journal->priv->dispose_has_run ){

		sexe = journal_find_exe_by_id( journal, exe_id );
		if( sexe ){
			return(( const GDate * ) &sexe->last_clo );
		}
	}

	return( NULL );
}

static sDetailExe *
journal_find_exe_by_id( const ofoJournal *journal, gint exe_id )
{
	GList *idet;
	sDetailExe *sdet;

	for( idet=journal->priv->exes ; idet ; idet=idet->next ){
		sdet = ( sDetailExe * ) idet->data;
		if( sdet->exe_id == exe_id ){
			return( sdet );
		}
	}

	return( NULL );
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

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	/* a journal whose internal identifier is not set is deletable,
	 * but this should never appear */
	g_return_val_if_fail( ofo_journal_get_id( journal ) > 0, TRUE );

	if( !journal->priv->dispose_has_run ){

		ok = TRUE;
		exe_id = ofo_dossier_get_exercice_id( dossier );

		for( ic=journal->priv->amounts ; ic && ok ; ic=ic->next ){
			detail = ( sDetailDev * ) ic->data;
			if( detail->exe_id == exe_id ){
				ok &= detail->clo_deb == 0.0 && detail->clo_cre == 0.0 &&
						detail->deb == 0.0 && detail->cre == 0.0;
			}
		}

		ok &= !ofo_entry_use_journal( dossier, ofo_journal_get_id( journal )) &&
				!ofo_model_use_journal( dossier, ofo_journal_get_id( journal ));

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
 * ofo_journal_set_id:
 */
void
ofo_journal_set_id( ofoJournal *journal, gint id )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		journal->priv->id = id;
	}
}

/**
 * ofo_journal_set_mnemo:
 */
void
ofo_journal_set_mnemo( ofoJournal *journal, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		journal->priv->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_journal_set_label:
 */
void
ofo_journal_set_label( ofoJournal *journal, const gchar *label )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		journal->priv->label = g_strdup( label );
	}
}

/**
 * ofo_journal_set_notes:
 */
void
ofo_journal_set_notes( ofoJournal *journal, const gchar *notes )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		journal->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_journal_set_maj_user:
 */
void
ofo_journal_set_maj_user( ofoJournal *journal, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		journal->priv->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_journal_set_maj_stamp:
 */
void
ofo_journal_set_maj_stamp( ofoJournal *journal, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		memcpy( &journal->priv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
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

	if( !journal->priv->dispose_has_run ){

		memcpy( &journal->priv->cloture, date, sizeof( GDate ));
	}
}*/

/**
 * ofo_journal_insert:
 *
 * Only insert here a new journal, so only the main properties
 */
gboolean
ofo_journal_insert( ofoJournal *journal, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_insert";

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

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
journal_do_insert( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
{
	return( journal_insert_main( journal, sgbd, user ) &&
			journal_get_back_id( journal, sgbd ));
}

static gboolean
journal_insert_main( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
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

static gboolean
journal_get_back_id( ofoJournal *journal, ofoSgbd *sgbd )
{
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;

	result = ofo_sgbd_query_ex( sgbd, "SELECT LAST_INSERT_ID()" );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_journal_set_id( journal, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	return( ok );
}

#if 0
static gboolean
journal_insert_details( ofoJournal *journal, ofoSgbd *sgbd )
{
	GString *query;
	GList *idet;
	sDetailDev *detail;
	gboolean ok;
	gchar sclodeb[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar sclocre[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar sdeb[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar scre[1+G_ASCII_DTOSTR_BUF_SIZE];

	ok = TRUE;
	query = g_string_new( "" );

	for( idet=journal->priv->amounts ; idet ; idet=idet->next ){

		detail = ( sDetailDev * ) idet->data;

		g_string_printf( query,
					"INSERT INTO OFA_T_JOURNAUX_DEV "
					"	(JOU_ID,JOU_EXE_ID,JOU_DEV_ID,"
					"	JOU_DEV_CLO_DEB,JOU_DEV_CLO_CRE,JOU_DEV_DEB,JOU_DEV_CRE) "
					"	VALUES (%d,%d,%d,%s,%s,%s,%s)",
					ofo_journal_get_id( journal ),
					detail->exe_id,
					detail->dev_id,
					g_ascii_dtostr( sclodeb, G_ASCII_DTOSTR_BUF_SIZE, detail->clo_deb ),
					g_ascii_dtostr( sclocre, G_ASCII_DTOSTR_BUF_SIZE, detail->clo_cre ),
					g_ascii_dtostr( sdeb, G_ASCII_DTOSTR_BUF_SIZE, detail->deb ),
					g_ascii_dtostr( scre, G_ASCII_DTOSTR_BUF_SIZE, detail->cre ));

		ok &= ofo_sgbd_query( sgbd, query->str );
	}

	g_string_free( query, TRUE );

	return( ok );
}
#endif

/**
 * ofo_journal_update:
 *
 * We only update here the user properties, so do not care with the
 * details of balances per currency.
 */
gboolean
ofo_journal_update( ofoJournal *journal, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_update";

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

		if( journal_do_update(
					journal,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_UPDATE_DATASET( st_global, journal );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
journal_do_update( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
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
			"	WHERE JOU_ID=%d", user, stamp, ofo_journal_get_id( journal ));

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

/**
 * ofo_journal_delete:
 *
 * Take care of deleting both main and detail records.
 */
gboolean
ofo_journal_delete( ofoJournal *journal, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_delete";

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, journal );

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
journal_do_delete( ofoJournal *journal, ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX WHERE JOU_ID=%d",
					ofo_journal_get_id( journal ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX_DEV WHERE JOU_ID=%d",
					ofo_journal_get_id( journal ));

	ok &= ofo_sgbd_query( sgbd, query );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX_EXE WHERE JOU_ID=%d",
					ofo_journal_get_id( journal ));

	ok &= ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

/**
 * ofo_journal_record_entry:
 *
 * Record the last entry in the journal
 */
gboolean
ofo_journal_record_entry( ofoJournal *journal, ofoSgbd *sgbd, ofoEntry *entry )
{
	gboolean ok;

	g_return_val_if_fail( journal && OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( sgbd && OFO_IS_SGBD( sgbd ), FALSE );
	g_return_val_if_fail( entry && OFO_IS_ENTRY( entry ), FALSE );

	ok = TRUE;

	return( ok );
}

static gint
journal_cmp_by_id( const ofoJournal *a, gconstpointer b )
{
	return( ofo_journal_get_id( a ) - GPOINTER_TO_INT( b ));
}

static gint
journal_cmp_by_mnemo( const ofoJournal *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_journal_get_mnemo( a ), mnemo ));
}

static gint
journal_cmp_by_ptr( const ofoJournal *a, const ofoJournal *b )
{
	return( g_utf8_collate( ofo_journal_get_mnemo( a ), ofo_journal_get_mnemo( b )));
}
