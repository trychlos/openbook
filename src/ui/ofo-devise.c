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
#include "ui/ofo-devise.h"

/* priv instance data
 */
struct _ofoDevisePrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	gint     id;
	gchar   *code;
	gchar   *label;
	gchar   *symbol;
};

G_DEFINE_TYPE( ofoDevise, ofo_devise, OFO_TYPE_BASE )

#define OFO_DEVISE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_DEVISE, ofoDevisePrivate))

static ofoBaseStatic *st_static = NULL;

static ofoBaseStatic *get_static_data( ofoDossier *dossier );
static GList         *devise_load_dataset( void );
static ofoDevise     *devise_find_by_code( GList *set, const gchar *code );
static gint           devise_cmp_by_code( const ofoDevise *a, const gchar *code );
static gboolean       devise_do_insert( ofoDevise *devise, ofoSgbd *sgbd );
static gboolean       devise_insert_main( ofoDevise *devise, ofoSgbd *sgbd );
static gboolean       devise_reset_id( ofoDevise *devise, ofoSgbd *sgbd );
static gboolean       devise_do_update( ofoDevise *devise, ofoSgbd *sgbd );
static gboolean       devise_do_delete( ofoDevise *devise, ofoSgbd *sgbd );
static gint           devise_cmp_by_code( const ofoDevise *a, const gchar *code );
static gint           devise_cmp_by_ptr( const ofoDevise *a, const ofoDevise *b );

static void
ofo_devise_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_devise_finalize";
	ofoDevise *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_DEVISE( instance );

	g_free( self->priv->code );
	g_free( self->priv->label );
	g_free( self->priv->symbol );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_devise_parent_class )->finalize( instance );
}

static void
ofo_devise_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_devise_dispose";
	ofoDevise *self;

	self = OFO_DEVISE( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				self->priv->code, self->priv->label );

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_devise_parent_class )->dispose( instance );
}

static void
ofo_devise_init( ofoDevise *self )
{
	static const gchar *thisfn = "ofo_devise_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_DEVISE_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	self->priv->id = -1;
}

static void
ofo_devise_class_init( ofoDeviseClass *klass )
{
	static const gchar *thisfn = "ofo_devise_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoDevisePrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_devise_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_devise_finalize;
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
 * ofo_devise_get_dataset:
 *
 * Loads the list of devises ordered by ascending mnemo.
 *
 * The returned list is owned by the #ofoDevise class, and should
 * not be freed by the caller.
 */
GList *
ofo_devise_get_dataset( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_get_dataset";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	st = get_static_data( dossier );
	if( !st->dataset ){
		st->dataset = devise_load_dataset();
	}

	return( st->dataset );
}

static GList *
devise_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoDevise *devise;
	GList *dataset;
	ofoSgbd *sgbd;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_static->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT DEV_ID,DEV_CODE,DEV_LABEL,DEV_SYMBOL "
			"	FROM OFA_T_DEVISES "
			"	ORDER BY DEV_CODE ASC" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		devise = ofo_devise_new();
		ofo_devise_set_id( devise, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_devise_set_code( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_label( devise, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_devise_set_symbol( devise, ( gchar * ) icol->data );

		dataset = g_list_prepend( dataset, devise );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_devise_get_by_code:
 *
 * Returns: the searched currency, or %NULL.
 *
 * The returned object is owned by the #ofoDevise class, and should
 * not be unreffed by the caller.
 */
ofoDevise *
ofo_devise_get_by_code( ofoDossier *dossier, const gchar *code )
{
	static const gchar *thisfn = "ofo_devise_get_by_code";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( code && g_utf8_strlen( code, -1 ), NULL );

	g_debug( "%s: dossier=%p, code=%s", thisfn, ( void * ) dossier, code );

	st = get_static_data( dossier );
	if( !st->dataset ){
		st->dataset = devise_load_dataset();
	}

	return( devise_find_by_code( st->dataset, code ));
}

static ofoDevise *
devise_find_by_code( GList *set, const gchar *code )
{
	GList *found;

	found = g_list_find_custom(
				set, code, ( GCompareFunc ) devise_cmp_by_code );
	if( found ){
		return( OFO_DEVISE( found->data ));
	}

	return( NULL );
}

/**
 * ofo_devise_clear_static:
 */
void
ofo_devise_clear_static( void )
{
	if( st_static ){
		g_list_foreach( st_static->dataset, ( GFunc ) g_object_unref, NULL );
		g_list_free( st_static->dataset );
		g_free( st_static );
		st_static = NULL;
	}
}

/**
 * ofo_devise_new:
 */
ofoDevise *
ofo_devise_new( void )
{
	ofoDevise *devise;

	devise = g_object_new( OFO_TYPE_DEVISE, NULL );

	return( devise );
}

/**
 * ofo_devise_get_id:
 */
gint
ofo_devise_get_id( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), -1 );

	if( !devise->priv->dispose_has_run ){

		return( devise->priv->id );
	}

	return( -1 );
}

/**
 * ofo_devise_get_code:
 */
const gchar *
ofo_devise_get_code( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !devise->priv->dispose_has_run ){

		return( devise->priv->code );
	}

	return( NULL );
}

/**
 * ofo_devise_get_label:
 */
const gchar *
ofo_devise_get_label( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !devise->priv->dispose_has_run ){

		return( devise->priv->label );
	}

	return( NULL );
}

/**
 * ofo_devise_get_symbol:
 */
const gchar *
ofo_devise_get_symbol( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !devise->priv->dispose_has_run ){

		return( devise->priv->symbol );
	}

	return( NULL );
}

/**
 * ofo_devise_is_deletable:
 *
 * A currency should not be deleted while it is referenced by an
 * account.
 */
gboolean
ofo_devise_is_deletable( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !devise->priv->dispose_has_run ){

		g_warning( "ofo_devise_is_deletable: TO BE WRITTEN" );
	}

	return( FALSE );
}

/**
 * ofo_devise_set_id:
 */
void
ofo_devise_set_id( ofoDevise *devise, gint id )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->priv->dispose_has_run ){

		devise->priv->id = id;
	}
}

/**
 * ofo_devise_set_code:
 */
void
ofo_devise_set_code( ofoDevise *devise, const gchar *code )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->priv->dispose_has_run ){

		devise->priv->code = g_strdup( code );
	}
}

/**
 * ofo_devise_set_label:
 */
void
ofo_devise_set_label( ofoDevise *devise, const gchar *label )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->priv->dispose_has_run ){

		devise->priv->label = g_strdup( label );
	}
}

/**
 * ofo_devise_set_symbol:
 */
void
ofo_devise_set_symbol( ofoDevise *devise, const gchar *symbol )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->priv->dispose_has_run ){

		devise->priv->symbol = g_strdup( symbol );
	}
}

/**
 * ofo_devise_insert:
 */
gboolean
ofo_devise_insert( ofoDevise *devise, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_insert";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !devise->priv->dispose_has_run ){

		g_debug( "%s: devise=%p, dossier=%p",
				thisfn, ( void * ) devise, ( void * ) dossier );

		st = get_static_data( dossier );
		if( !st->dataset ){
			st->dataset = devise_load_dataset();
		}

		if( devise_do_insert(
					devise,
					ofo_dossier_get_sgbd( dossier ))){

			st->dataset = g_list_insert_sorted(
					st->dataset, devise, ( GCompareFunc ) devise_cmp_by_ptr );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
devise_do_insert( ofoDevise *devise, ofoSgbd *sgbd )
{
	return( devise_insert_main( devise, sgbd ) &&
			devise_reset_id( devise, sgbd ));
}

static gboolean
devise_insert_main( ofoDevise *devise, ofoSgbd *sgbd )
{
	gchar *query;
	gchar *label;
	const gchar *symbol;
	gboolean ok;

	label = my_utils_quote( ofo_devise_get_label( devise ));
	symbol = ofo_devise_get_symbol( devise );

	query = g_strdup_printf(
			"INSERT INTO OFA_T_DEVISES "
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL)"
			"	VALUES ('%s','%s','%s')",
			ofo_devise_get_code( devise ),
			label,
			symbol );

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );
	g_free( label );

	return( ok );
}

static gboolean
devise_reset_id( ofoDevise *devise, ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;
	GSList *result, *icol;

	ok = FALSE ;

	query = g_strdup_printf(
			"SELECT DEV_ID FROM OFA_T_DEVISES"
			"	WHERE DEV_CODE='%s'",
			ofo_devise_get_code( devise ));

	result = ofo_sgbd_query_ex( sgbd, query );

	if( result ){
		icol = ( GSList * ) result->data;
		ofo_devise_set_id( devise, atoi(( gchar * ) icol->data ));
		ofo_sgbd_free_result( result );
		ok = TRUE;
	}

	g_free( query );

	return( ok );
}

/**
 * ofo_devise_update:
 */
gboolean
ofo_devise_update( ofoDevise *devise, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_update";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !devise->priv->dispose_has_run ){

		g_debug( "%s: devise=%p, dossier=%p",
				thisfn, ( void * ) devise, ( void * ) dossier );

		st = get_static_data( dossier );
		if( !st->dataset ){
			st->dataset = devise_load_dataset();
		}

		if( devise_do_update(
					devise,
					ofo_dossier_get_sgbd( dossier ))){

			st->dataset = g_list_remove( st->dataset, devise );
			st->dataset = g_list_insert_sorted(
					st->dataset, devise, ( GCompareFunc ) devise_cmp_by_ptr );

			return( TRUE );
		}
	}

	return( FALSE );
}

/**
 * ofo_devise_update:
 *
 * we deal here with an update of publicly modifiable devise properties
 * mnemo is not modifiable
 */
static gboolean
devise_do_update( ofoDevise *devise, ofoSgbd *sgbd )
{
	gchar *query;
	gchar *label;
	gboolean ok;

	label = my_utils_quote( ofo_devise_get_label( devise ));

	query = g_strdup_printf(
			"UPDATE OFA_T_DEVISES SET "
			"	DEV_CODE='%s',DEV_LABEL='%s',DEV_SYMBOL='%s'"
			"	WHERE DEV_ID=%d",
			ofo_devise_get_code( devise ),
			label,
			ofo_devise_get_symbol( devise ),
			ofo_devise_get_id( devise ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );
	g_free( label );

	return( ok );
}

/**
 * ofo_devise_delete:
 */
gboolean
ofo_devise_delete( ofoDevise *devise, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_devise_delete";
	ofoBaseStatic *st;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !devise->priv->dispose_has_run ){

		g_debug( "%s: devise=%p, dossier=%p",
				thisfn, ( void * ) devise, ( void * ) dossier );

		st = get_static_data( dossier );
		if( !st->dataset ){
			st->dataset = devise_load_dataset();
		}

		if( devise_do_delete(
					devise,
					ofo_dossier_get_sgbd( dossier ))){

			st->dataset = g_list_remove( st->dataset, devise );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
devise_do_delete( ofoDevise *devise, ofoSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_DEVISES"
			"	WHERE DEV_CODE='%s'",
					ofo_devise_get_code( devise ));

	ok = ofo_sgbd_query( sgbd, query );

	g_free( query );

	return( ok );
}

static gint
devise_cmp_by_code( const ofoDevise *a, const gchar *code )
{
	return( g_utf8_collate( ofo_devise_get_code( a ), code ));
}

static gint
devise_cmp_by_ptr( const ofoDevise *a, const ofoDevise *b )
{
	return( g_utf8_collate( ofo_devise_get_code( a ), ofo_devise_get_code( b )));
}
