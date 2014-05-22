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
#include "ui/ofo-taux.h"

/* priv instance data
 */
struct _ofoTauxPrivate {
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
	GDate    val_begin;
	GDate    val_end;
	gdouble  taux;
	gchar   *maj_user;
	GTimeVal maj_stamp;
};

G_DEFINE_TYPE( ofoTaux, ofo_taux, OFO_TYPE_BASE )

#define OFO_TAUX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_TAUX, ofoTauxPrivate))

static void
ofo_taux_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_taux_finalize";
	ofoTaux *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_TAUX( instance );

	g_free( self->priv->mnemo );
	g_free( self->priv->label );
	g_free( self->priv->notes );
	g_free( self->priv->maj_user );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_taux_parent_class )->finalize( instance );
}

static void
ofo_taux_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_taux_dispose";
	ofoTaux *self;

	self = OFO_TAUX( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				self->priv->mnemo, self->priv->label );

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_taux_parent_class )->dispose( instance );
}

static void
ofo_taux_init( ofoTaux *self )
{
	static const gchar *thisfn = "ofo_taux_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_TAUX_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	self->priv->id = -1;
}

static void
ofo_taux_class_init( ofoTauxClass *klass )
{
	static const gchar *thisfn = "ofo_taux_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoTauxPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_taux_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_taux_finalize;
}

/**
 * ofo_taux_new:
 */
ofoTaux *
ofo_taux_new( void )
{
	ofoTaux *taux;

	taux = g_object_new( OFO_TYPE_TAUX, NULL );

	return( taux );
}

/**
 * ofo_taux_load_set:
 *
 * Loads/reloads the ordered list of tauxs
 */
GList *
ofo_taux_load_set( ofoSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_taux_load_set";
	GSList *result, *irow, *icol;
	ofoTaux *taux;
	GList *set;

	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofo_sgbd_query_ex( sgbd, NULL,
			"SELECT TAX_ID,TAX_MNEMO,TAX_LABEL,TAX_NOTES,"
			"	TAX_VAL_DEB,TAX_VAL_FIN,TAX_TAUX,"
			"	TAX_MAJ_USER,TAX_MAJ_STAMP"
			"	FROM OFA_T_TAUX "
			"	ORDER BY TAX_MNEMO ASC, TAX_VAL_DEB ASC" );

	set = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		taux = ofo_taux_new();
		ofo_taux_set_id( taux, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_taux_set_mnemo( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_label( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_notes( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_val_begin( taux, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_taux_set_val_end( taux, my_utils_date_from_str(( gchar * ) icol->data ));
		icol = icol->next;
		if( icol->data ){
			ofo_taux_set_taux( taux, g_ascii_strtod(( gchar * ) icol->data, NULL ));
		}
		icol = icol->next;
		ofo_taux_set_maj_user( taux, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str(( gchar * ) icol->data ));

		set = g_list_prepend( set, taux );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( set ));
}

/**
 * ofo_taux_dump_set:
 */
void
ofo_taux_dump_set( GList *set )
{
	static const gchar *thisfn = "ofo_taux_dump_set";
	ofoTauxPrivate *priv;
	GList *ic;

	for( ic=set ; ic ; ic=ic->next ){
		priv = OFO_TAUX( ic->data )->priv;
		g_debug( "%s: taux %s - %s", thisfn, priv->mnemo, priv->label );
	}
}

/**
 * ofo_taux_get_id:
 */
gint
ofo_taux_get_id( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), -1 );

	if( !taux->priv->dispose_has_run ){

		return( taux->priv->id );
	}

	return( -1 );
}

/**
 * ofo_taux_get_mnemo:
 */
const gchar *
ofo_taux_get_mnemo( const ofoTaux *taux )
{
	const gchar *mnemo = NULL;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !taux->priv->dispose_has_run ){

		mnemo = taux->priv->mnemo;
	}

	return( mnemo );
}

/**
 * ofo_taux_get_label:
 */
const gchar *
ofo_taux_get_label( const ofoTaux *taux )
{
	const gchar *label = NULL;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !taux->priv->dispose_has_run ){

		label = taux->priv->label;
	}

	return( label );
}

/**
 * ofo_taux_get_notes:
 */
const gchar *
ofo_taux_get_notes( const ofoTaux *taux )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !taux->priv->dispose_has_run ){

		notes = taux->priv->notes;
	}

	return( notes );
}

/**
 * ofo_taux_get_val_begin:
 */
const GDate *
ofo_taux_get_val_begin( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !taux->priv->dispose_has_run ){

		return(( const GDate * ) &taux->priv->val_begin );
	}

	return( NULL );
}

/**
 * ofo_taux_get_val_end:
 */
const GDate *
ofo_taux_get_val_end( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), NULL );

	if( !taux->priv->dispose_has_run ){

		return(( const GDate * ) &taux->priv->val_end );
	}

	return( NULL );
}

/**
 * ofo_taux_get_taux:
 */
gdouble
ofo_taux_get_taux( const ofoTaux *taux )
{
	g_return_val_if_fail( OFO_IS_TAUX( taux ), 0 );

	if( !taux->priv->dispose_has_run ){

		return( taux->priv->taux );
	}

	return( 0 );
}

/**
 * ofo_taux_set_id:
 */
void
ofo_taux_set_id( ofoTaux *taux, gint id )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->id = id;
	}
}

/**
 * ofo_taux_set_mnemo:
 */
void
ofo_taux_set_mnemo( ofoTaux *taux, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_taux_set_label:
 */
void
ofo_taux_set_label( ofoTaux *taux, const gchar *label )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->label = g_strdup( label );
	}
}

/**
 * ofo_taux_set_notes:
 */
void
ofo_taux_set_notes( ofoTaux *taux, const gchar *notes )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_taux_set_val_begin:
 */
void
ofo_taux_set_val_begin( ofoTaux *taux, const GDate *date )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		memcpy( &taux->priv->val_begin, date, sizeof( GDate ));
	}
}

/**
 * ofo_taux_set_val_end:
 */
void
ofo_taux_set_val_end( ofoTaux *taux, const GDate *date )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		memcpy( &taux->priv->val_end, date, sizeof( GDate ));
	}
}

/**
 * ofo_taux_set_taux:
 */
void
ofo_taux_set_taux( ofoTaux *taux, gdouble value )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->taux = value;
	}
}

/**
 * ofo_taux_set_maj_user:
 */
void
ofo_taux_set_maj_user( ofoTaux *taux, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		taux->priv->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_taux_set_maj_stamp:
 */
void
ofo_taux_set_maj_stamp( ofoTaux *taux, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_TAUX( taux ));

	if( !taux->priv->dispose_has_run ){

		memcpy( &taux->priv->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_taux_insert:
 */
gboolean
ofo_taux_insert( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gchar *dbegin, *dend;
	gchar rate[1+G_ASCII_DTOSTR_BUF_SIZE];
	gboolean ok;
	gchar *stamp;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_taux_get_label( taux ));
	notes = my_utils_quote( ofo_taux_get_notes( taux ));
	stamp = my_utils_timestamp();
	dbegin = my_utils_sql_from_date( ofo_taux_get_val_begin( taux ));
	dend = my_utils_sql_from_date( ofo_taux_get_val_end( taux ));

	query = g_string_new( "INSERT INTO OFA_T_TAUX" );

	g_string_append_printf( query,
			"	(TAX_MNEMO,TAX_LABEL,TAX_NOTES,"
			"	TAX_VAL_DEB, TAX_VAL_FIN,TAX_TAUX,"
			"	TAX_MAJ_USER, TAX_MAJ_STAMP) VALUES ('%s','%s',",
			ofo_taux_get_mnemo( taux ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( dbegin && g_utf8_strlen( dbegin, -1 )){
		g_string_append_printf( query, "'%s',", dbegin );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( dend && g_utf8_strlen( dend, -1 )){
		g_string_append_printf( query, "'%s',", dend );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
				"%s,'%s','%s')",
				g_ascii_dtostr( rate, G_ASCII_DTOSTR_BUF_SIZE, ofo_taux_get_taux( taux )),
				user, stamp );

	if( ofo_sgbd_query( sgbd, NULL, query->str )){

		ofo_taux_set_maj_user( taux, user );
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str( stamp ));

		g_string_printf( query,
				"SELECT TAX_ID FROM OFA_T_TAUX"
				"	WHERE TAX_MNEMO='%s'",
				ofo_taux_get_mnemo( taux ));

		result = ofo_sgbd_query_ex( sgbd, NULL, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_taux_set_id( taux, atoi(( gchar * ) icol->data ));
			ofo_sgbd_free_result( result );
			ok = TRUE;
		}
	}

	g_string_free( query, TRUE );
	g_free( dbegin );
	g_free( dend );
	g_free( notes );
	g_free( label );
	g_free( stamp );

	return( ok );
}

/**
 * ofo_taux_update:
 */
gboolean
ofo_taux_update( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;
	gchar *dbegin, *dend;
	gchar rate[1+G_ASCII_DTOSTR_BUF_SIZE];

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_taux_get_label( taux ));
	notes = my_utils_quote( ofo_taux_get_notes( taux ));
	stamp = my_utils_timestamp();
	dbegin = my_utils_sql_from_date( ofo_taux_get_val_begin( taux ));
	dend = my_utils_sql_from_date( ofo_taux_get_val_end( taux ));

	query = g_string_new( "UPDATE OFA_T_TAUX SET " );

	g_string_append_printf( query, "TAX_MNEMO='%s',", ofo_taux_get_mnemo( taux ));
	g_string_append_printf( query, "TAX_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "TAX_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "TAX_NOTES=NULL," );
	}

	if( dbegin && g_utf8_strlen( dbegin, -1 )){
		g_string_append_printf( query, "TAX_VAL_DEB='%s',", dbegin );
	} else {
		query = g_string_append( query, "TAX_VAL_DEB=NULL," );
	}

	if( dend && g_utf8_strlen( dend, -1 )){
		g_string_append_printf( query, "TAX_VAL_FIN='%s',", dend );
	} else {
		query = g_string_append( query, "TAX_VAL_FIN=NULL," );
	}

	g_string_append_printf( query,
			"	TAX_TAUX=%s,TAX_MAJ_USER='%s',TAX_MAJ_STAMP='%s'"
			"	WHERE TAX_ID=%d",
			g_ascii_dtostr( rate, G_ASCII_DTOSTR_BUF_SIZE, ofo_taux_get_taux( taux )),
			user, stamp, ofo_taux_get_id( taux ));

	if( ofo_sgbd_query( sgbd, NULL, query->str )){

		ofo_taux_set_maj_user( taux, user );
		ofo_taux_set_maj_stamp( taux, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

/**
 * ofo_taux_delete:
 */
gboolean
ofo_taux_delete( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_TAUX( taux ), FALSE );
	g_return_val_if_fail( OFO_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_TAUX WHERE TAX_ID=%d",
					ofo_taux_get_id( taux ));

	ok = ofo_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
