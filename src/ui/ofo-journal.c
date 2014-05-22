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
};

G_DEFINE_TYPE( ofoJournal, ofo_journal, OFO_TYPE_BASE )

#define OFO_JOURNAL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_JOURNAL, ofoJournalPrivate))

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
 * ofo_journal_load_set:
 *
 * Loads/reloads the ordered list of journals
 */
GList *
ofo_journal_load_set( ofoSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_journal_load_set";
	GSList *result, *irow, *icol;
	ofoJournal *journal;
	GList *set;

	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT JOU_ID,JOU_MNEMO,JOU_LABEL,JOU_NOTES,"
			"	JOU_MAJ_USER,JOU_MAJ_STAMP,"
			"	JOU_CLO"
			"	FROM OFA_T_JOURNAUX "
			"	ORDER BY JOU_MNEMO ASC" );

	set = NULL;

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

		set = g_list_prepend( set, journal );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( set ));
}

/**
 * ofo_journal_dump_set:
 */
void
ofo_journal_dump_set( GList *set )
{
	static const gchar *thisfn = "ofo_journal_dump_set";
	ofoJournalPrivate *priv;
	GList *ic;

	for( ic=set ; ic ; ic=ic->next ){
		priv = OFO_JOURNAL( ic->data )->priv;
		g_debug( "%s: journal %s - %s", thisfn, priv->mnemo, priv->label );
	}
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
 * ofo_journal_is_empty:
 */
gboolean
ofo_journal_is_empty( const ofoJournal *journal )
{
	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );

	if( !journal->priv->dispose_has_run ){

		g_warning( "ofo_journal_is_empty: TO BE WRITTEN" );
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
ofo_journal_insert( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

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
			"'%s','%s')", user, stamp );

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_journal_set_maj_user( journal, user );
		ofo_journal_set_maj_stamp( journal, my_utils_stamp_from_str( stamp ));

		g_string_printf( query,
				"SELECT JOU_ID FROM OFA_T_JOURNAUX"
				"	WHERE JOU_MNEMO='%s'",
				ofo_journal_get_mnemo( journal ));

		result = ofo_sgbd_query_ex( sgbd, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_journal_set_id( journal, atoi(( gchar * ) icol->data ));

			ofo_sgbd_free_result( result );

			ok = TRUE;
		}
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_journal_update:
 */
gboolean
ofo_journal_update( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

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
 */
gboolean
ofo_journal_delete( ofoJournal *journal, ofoSgbd *sgbd, const gchar *user )
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
