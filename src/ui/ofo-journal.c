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

/* private class data
 */
struct _ofoJournalClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
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
	GDate    maxdate;
	GDate    cloture;
};

static ofoBaseClass *st_parent_class  = NULL;

static GType  register_type( void );
static void   class_init( ofoJournalClass *klass );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   instance_dispose( GObject *instance );
static void   instance_finalize( GObject *instance );

GType
ofo_journal_get_type( void )
{
	static GType st_type = 0;

	if( !st_type ){
		st_type = register_type();
	}

	return( st_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_journal_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoJournalClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoJournal ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoJournal", &info, 0 );

	return( type );
}

static void
class_init( ofoJournalClass *klass )
{
	static const gchar *thisfn = "ofo_journal_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoJournalClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofo_journal_instance_init";
	ofoJournal *self;

	g_return_if_fail( OFO_IS_JOURNAL( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFO_JOURNAL( instance );

	self->private = g_new0( ofoJournalPrivate, 1 );

	self->private->dispose_has_run = FALSE;

	self->private->id = -1;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_journal_instance_dispose";
	ofoJournalPrivate *priv;

	g_return_if_fail( OFO_IS_JOURNAL( instance ));

	priv = ( OFO_JOURNAL( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				priv->mnemo, priv->label );

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
		g_free( priv->label );
		g_free( priv->notes );
		g_free( priv->maj_user );

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_journal_instance_finalize";
	ofoJournalPrivate *priv;

	g_return_if_fail( OFO_IS_JOURNAL( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_JOURNAL( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
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
 * ofo_journal_load_set:
 *
 * Loads/reloads the ordered list of journals
 */
GList *
ofo_journal_load_set( ofaSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_journal_load_set";
	GSList *result, *irow, *icol;
	ofoJournal *journal;
	GList *set;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofa_sgbd_query_ex( sgbd, NULL,
			"SELECT JOU_ID,JOU_MNEMO,JOU_LABEL,JOU_NOTES,"
			"	JOU_MAJ_USER,JOU_MAJ_STAMP,"
			"	JOU_MAXDATE,JOU_CLO"
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
		ofo_journal_set_maxdate( journal, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_journal_set_cloture( journal, my_utils_date_from_str(( gchar * ) icol->data ));

		set = g_list_prepend( set, journal );
	}

	ofa_sgbd_free_result( result );

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
		priv = OFO_JOURNAL( ic->data )->private;
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

	if( !journal->private->dispose_has_run ){

		return( journal->private->id );
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

	if( !journal->private->dispose_has_run ){

		mnemo = journal->private->mnemo;
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

	if( !journal->private->dispose_has_run ){

		label = journal->private->label;
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

	if( !journal->private->dispose_has_run ){

		notes = journal->private->notes;
	}

	return( notes );
}

/**
 * ofo_journal_get_maxdate:
 */
const GDate *
ofo_journal_get_maxdate( const ofoJournal *journal )
{
	const GDate *date;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), NULL );

	date = NULL;

	if( !journal->private->dispose_has_run ){

		date = ( const GDate * ) &journal->private->maxdate;
	}

	return( date );
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

	if( !journal->private->dispose_has_run ){

		date = ( const GDate * ) &journal->private->cloture;
	}

	return( date );
}

/**
 * ofo_journal_set_id:
 */
void
ofo_journal_set_id( ofoJournal *journal, gint id )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->private->dispose_has_run ){

		journal->private->id = id;
	}
}

/**
 * ofo_journal_set_mnemo:
 */
void
ofo_journal_set_mnemo( ofoJournal *journal, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->private->dispose_has_run ){

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

	if( !journal->private->dispose_has_run ){

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

	if( !journal->private->dispose_has_run ){

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

	if( !journal->private->dispose_has_run ){

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

	if( !journal->private->dispose_has_run ){

		memcpy( &journal->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_journal_set_maxdate:
 */
void
ofo_journal_set_maxdate( ofoJournal *journal, const GDate *date )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->private->dispose_has_run ){

		memcpy( &journal->private->maxdate, date, sizeof( GDate ));
	}
}

/**
 * ofo_journal_set_cloture:
 */
void
ofo_journal_set_cloture( ofoJournal *journal, const GDate *date )
{
	g_return_if_fail( OFO_IS_JOURNAL( journal ));

	if( !journal->private->dispose_has_run ){

		memcpy( &journal->private->cloture, date, sizeof( GDate ));
	}
}

/**
 * ofo_journal_insert:
 */
gboolean
ofo_journal_insert( ofoJournal *journal, ofaSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

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

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		ofo_journal_set_maj_user( journal, user );
		ofo_journal_set_maj_stamp( journal, my_utils_stamp_from_str( stamp ));

		g_string_printf( query,
				"SELECT JOU_ID FROM OFA_T_JOURNAUX"
				"	WHERE JOU_MNEMO='%s' AND JOU_MAJ_STAMP='%s'",
				ofo_journal_get_mnemo( journal ), stamp );

		result = ofa_sgbd_query_ex( sgbd, NULL, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_journal_set_id( journal, atoi(( gchar * ) icol->data ));

			ofa_sgbd_free_result( result );

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
ofo_journal_update( ofoJournal *journal, ofaSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

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

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

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
ofo_journal_delete( ofoJournal *journal, ofaSgbd *sgbd, const gchar *user )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_JOURNAL( journal ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_JOURNAUX WHERE JOU_ID=%d",
					ofo_journal_get_id( journal ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
