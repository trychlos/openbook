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
#include "ui/ofo-dossier.h"
#include "ui/ofo-journal.h"

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
	GDate    cloture;
	GList   *amounts;					/* balances per currency */
};

typedef struct {
	gint     dev_id;
	gdouble  clo_deb;
	gdouble  clo_cre;
	gdouble  deb;
	gdouble  cre;
}
	sDetailBalance;

G_DEFINE_TYPE( ofoJournal, ofo_journal, OFO_TYPE_BASE )

#define OFO_JOURNAL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_JOURNAL, ofoJournalPrivate))

static ofoBaseStatic *st_static = NULL;

static ofoBaseStatic *get_static_data( ofoDossier *dossier );
static GList            *journal_load_dataset( void );
static ofoJournal       *journal_find_by_id( GList *set, gint id );
static gint              journal_cmp_by_id( const ofoJournal *a, gconstpointer b );
static ofoJournal       *journal_find_by_mnemo( GList *set, const gchar *mnemo );
static gint              journal_cmp_by_mnemo( const ofoJournal *a, const gchar *mnemo );
static gint              journal_cmp_by_ptr( const ofoJournal *a, const ofoJournal *b );
static gboolean          journal_do_insert( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user );
static gboolean          journal_insert_main( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user );
static gboolean          journal_reset_id( ofoJournal *journal, ofoSgbd *sgbd );
static gboolean          journal_insert_details( ofoJournal *journal, ofoSgbd *sgbd );
static gboolean          journal_do_update( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user );
static gboolean          journal_do_delete( ofoJournal *journal, ofoSgbd *sgbd );

static void
ofo_journal_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_journal_finalize";
	ofoJournal *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_JOURNAL( instance );

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
	static const gchar *thisfn = "ofo_journal_dispose";
	ofoJournal *self;

	self = OFO_JOURNAL( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				self->priv->mnemo, self->priv->label );

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

	self->priv->id = -1;
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

static ofoBaseStatic *
get_static_data( ofoDossier *dossier )
{
	if( !st_static ){
		st_static = g_new0( ofoBaseStatic, 1 );
		st_static->dossier = OFO_BASE( dossier );
	}
	return( st_static );
}

/**
 * ofo_journal_get_dataset:
 *
 * Loads the list of journals ordered by ascending mnemo.
 *
 * The list is kept by this class as a static data.
 */
GList *
ofo_journal_get_dataset( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_get_dataset";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	st = get_static_data( dossier );
	if( !st->dataset ){
		st->dataset = journal_load_dataset();
	}

	return( st->dataset );
}

static GList *
journal_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoJournal *journal;
	ofoSgbd *sgbd;
	GList *dataset;
	gint prev_jou, jou_id;
	sDetailBalance *balance;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_static->dossier ));

	/* Journals list is loaded 'jou_id' order so that we will be able
	 * to easily load the balance details.
	 * It will have to be reordered before returning as all the callers
	 * expect to have it in display order
	 */
	result = ofo_sgbd_query_ex( sgbd,
			"SELECT JOU_ID,JOU_MNEMO,JOU_LABEL,JOU_NOTES,"
			"	JOU_MAJ_USER,JOU_MAJ_STAMP,"
			"	JOU_CLO"
			"	FROM OFA_T_JOURNAUX "
			"	ORDER BY JOU_ID ASC" );

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
		icol = icol->next;
		ofo_journal_set_cloture( journal, my_utils_date_from_str(( gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, journal );
	}

	ofo_sgbd_free_result( result );

	/* Then load the details
	 */
	result = ofo_sgbd_query_ex( sgbd,
			"SELECT JOU_ID,JOU_DEV_ID,"
			"	JOU_CLO_DEB,JOU_CLO_CRE,JOU_DEB,JOU_CRE,"
			"	FROM OFA_T_JOURNAUX_DET "
			"	ORDER BY JOU_ID ASC,JOU_DEV_ID ASC" );

	prev_jou = -1;
	journal = NULL;						/* so that gcc -pedantic is happy */

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		balance = g_new0( sDetailBalance, 1 );
		jou_id =  atoi(( gchar * ) icol->data );
		if( jou_id != prev_jou ){
			prev_jou = jou_id;
			journal = journal_find_by_id( dataset, jou_id );
			journal->priv->amounts = NULL;
		}
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

	/* Last, sort the journal list in ascending mnemo order
	 */
	return( g_list_sort( dataset, ( GCompareFunc ) journal_cmp_by_ptr ));
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
ofo_journal_get_by_id( ofoDossier *dossier, gint id )
{
	static const gchar *thisfn = "ofo_journal_get_by_id";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( id > 0, NULL );

	g_debug( "%s: dossier=%p, id=%d", thisfn, ( void * ) dossier, id );

	st = get_static_data( dossier );
	if( !st->dataset ){
		st->dataset = journal_load_dataset();
	}

	return( journal_find_by_id( st->dataset, id ));
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

static gint
journal_cmp_by_id( const ofoJournal *a, gconstpointer b )
{
	return( ofo_journal_get_id( a ) - GPOINTER_TO_INT( b ));
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
ofo_journal_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo )
{
	static const gchar *thisfn = "ofo_journal_get_by_id";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	g_debug( "%s: dossier=%p, mnemo=%s", thisfn, ( void * ) dossier, mnemo );

	st = get_static_data( dossier );
	if( !st->dataset ){
		st->dataset = journal_load_dataset();
	}

	return( journal_find_by_mnemo( st->dataset, mnemo ));
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

/**
 * ofo_journal_clear_dataset:
 */
void
ofo_journal_clear_dataset( void )
{
	if( st_static ){
		g_list_foreach( st_static->dataset, ( GFunc ) g_object_unref, NULL );
		g_list_free( st_static->dataset );
		g_free( st_static );
		st_static = NULL;
	}
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
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), -1 );

	if( !journal->priv->dispose_has_run ){

		return( journal->priv->id );
	}

	return( -1 );
}

/**
 * ofo_journal_get_mnemo:
 */
const gchar *
ofo_journal_get_mnemo( const ofoJournal *journal )
{
	const gchar *mnemo = NULL;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !journal->priv->dispose_has_run ){

		mnemo = journal->priv->mnemo;
	}

	return( mnemo );
}

/**
 * ofo_journal_get_label:
 */
const gchar *
ofo_journal_get_label( const ofoJournal *journal )
{
	const gchar *label = NULL;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !journal->priv->dispose_has_run ){

		label = journal->priv->label;
	}

	return( label );
}

/**
 * ofo_journal_get_notes:
 */
const gchar *
ofo_journal_get_notes( const ofoJournal *journal )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	if( !journal->priv->dispose_has_run ){

		notes = journal->priv->notes;
	}

	return( notes );
}

/**
 * ofo_journal_get_cloture:
 */
const GDate *
ofo_journal_get_cloture( const ofoJournal *journal )
{
	const GDate *date;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	date = NULL;

	if( !journal->priv->dispose_has_run ){

		date = ( const GDate * ) &journal->priv->cloture;
	}

	return( date );
}

/**
 * ofo_journal_is_deletable:
 *
 * A journal is considered to be deletable if no entry has been recorded
 * during the current exercice - This means that all its amounts must be
 * nuls for all currencies
 *
 * There is no need to test for the last closing date as this is not
 * relevant here: if set, it may be from the previous exercice, and if
 * unset, we have to check for the details
 */
gboolean
ofo_journal_is_deletable( const ofoJournal *journal )
{
	gboolean ok;
	GList *ic;
	sDetailBalance *detail;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	if( !journal->priv->dispose_has_run ){

		ok = TRUE;

		for( ic=journal->priv->amounts ; ic && ok ; ic=ic->next ){
			detail = ( sDetailBalance * ) ic->data;
			ok &= detail->clo_deb == 0.0 && detail->clo_cre == 0.0 &&
					detail->deb == 0.0 && detail->cre == 0.0;
		}

		return( ok );
	}

	return( FALSE );
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
void
ofo_journal_set_cloture( ofoJournal *journal, const GDate *date )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->priv->dispose_has_run ){

		memcpy( &journal->priv->cloture, date, sizeof( GDate ));
	}
}

/**
 * ofo_journal_insert:
 */
gboolean
ofo_journal_insert( ofoJournal *journal, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_journal_insert";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		st = get_static_data( dossier );
		if( !st->dataset ){
			st->dataset = journal_load_dataset();
		}

		if( journal_do_insert(
					journal,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			st->dataset = g_list_insert_sorted(
					st->dataset, journal, ( GCompareFunc ) journal_cmp_by_ptr );
		}
	}

	return( FALSE );
}

static gboolean
journal_do_insert( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
{
	return( journal_insert_main( journal, sgbd, user ) &&
			journal_reset_id( journal, sgbd ) &&
			journal_insert_details( journal, sgbd ));
}

static gboolean
journal_insert_main( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

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

	ok = ofo_sgbd_query( sgbd, query->str );

	ofo_journal_set_maj_user( journal, user );
	ofo_journal_set_maj_stamp( journal, my_utils_stamp_from_str( stamp ));

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

static gboolean
journal_reset_id( ofoJournal *journal, ofoSgbd *sgbd )
{
	GString *query;
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE;

	query = g_string_new( "SELECT JOU_ID FROM OFA_T_JOURNAUX" );
	g_string_append_printf( query,
			"	WHERE JOU_MNEMO='%s'",
			ofo_journal_get_mnemo( journal ));

	result = ofo_sgbd_query_ex( sgbd, query->str );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_journal_set_id( journal, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	g_string_free( query, TRUE );

	return( ok );
}

static gboolean
journal_insert_details( ofoJournal *journal, ofoSgbd *sgbd )
{
	GString *query;
	GList *idet;
	sDetailBalance *detail;
	gboolean ok;
	gchar sclodeb[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar sclocre[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar sdeb[1+G_ASCII_DTOSTR_BUF_SIZE];
	gchar scre[1+G_ASCII_DTOSTR_BUF_SIZE];

	ok = TRUE;
	query = g_string_new( "" );

	for( idet=journal->priv->amounts ; idet ; idet=idet->next ){

		detail = ( sDetailBalance * ) idet->data;

		g_string_printf( query,
					"INSERT INTO OFA_T_JOURNAUX_DET "
					"	(JOU_ID,JOU_DEV_ID,JOU_CLO_DEB,JOU_CLO_CRE,JOU_DEB,JOU_CRE) "
					"	VALUES (%d,%d,%s,%s,%s,%s)",
					ofo_journal_get_id( journal ),
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
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		st = get_static_data( dossier );
		if( !st->dataset ){
			st->dataset = journal_load_dataset();
		}

		if( journal_do_update(
					journal,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			st->dataset = g_list_remove( st->dataset, journal );
			st->dataset = g_list_insert_sorted(
					st->dataset, journal, ( GCompareFunc ) journal_cmp_by_ptr );
		}
	}

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
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_debug( "%s: journal=%p, dossier=%p",
				thisfn, ( void * ) journal, ( void * ) dossier );

		st = get_static_data( dossier );
		if( !st->dataset ){
			st->dataset = journal_load_dataset();
		}

		if( journal_do_delete(
					journal,
					ofo_dossier_get_sgbd( dossier ))){

			st->dataset = g_list_remove( st->dataset, journal );
			g_object_unref( journal );
		}
	}

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
			"DELETE FROM OFA_T_JOURNAUX_DET WHERE JOU_ID=%d",
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
