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
	gchar   *mnemo;
	gchar   *label;
	gchar   *symbol;
};

G_DEFINE_TYPE( ofoDevise, ofo_devise, OFO_TYPE_BASE )

#define OFO_DEVISE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_DEVISE, ofoDevisePrivate))

static void
ofo_devise_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_devise_finalize";
	ofoDevise *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_DEVISE( instance );

	g_free( self->priv->mnemo );
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
				self->priv->mnemo, self->priv->label );

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
 * ofo_devise_load_set:
 *
 * Loads/reloads the ordered list of devises
 */
GList *
ofo_devise_load_set( ofaSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_devise_load_set";
	GSList *result, *irow, *icol;
	ofoDevise *devise;
	GList *set;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofa_sgbd_query_ex( sgbd, NULL,
			"SELECT DEV_ID,DEV_CODE,DEV_LABEL,DEV_SYMBOL "
			"	FROM OFA_T_DEVISES "
			"	ORDER BY DEV_CODE ASC" );

	set = NULL;

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

		set = g_list_prepend( set, devise );
	}

	ofa_sgbd_free_result( result );

	return( g_list_reverse( set ));
}

/**
 * ofo_devise_dump_set:
 */
void
ofo_devise_dump_set( GList *set )
{
	static const gchar *thisfn = "ofo_devise_dump_set";
	ofoDevisePrivate *priv;
	GList *ic;

	for( ic=set ; ic ; ic=ic->next ){
		priv = OFO_DEVISE( ic->data )->priv;
		g_debug( "%s: devise %s - %s", thisfn, priv->mnemo, priv->label );
	}
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

		return( devise->priv->mnemo );
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
ofo_devise_set_code( ofoDevise *devise, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->priv->dispose_has_run ){

		devise->priv->mnemo = g_strdup( mnemo );
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
ofo_devise_insert( ofoDevise *devise, ofaSgbd *sgbd )
{
	GString *query;
	gchar *label;
	const gchar *symbol;
	gboolean ok;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_devise_get_label( devise ));
	symbol = ofo_devise_get_symbol( devise );

	query = g_string_new( "INSERT INTO OFA_T_DEVISES" );

	g_string_append_printf( query,
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL)"
			"	VALUES ('%s','%s',",
			ofo_devise_get_code( devise ),
			label );

	if( symbol && g_utf8_strlen( symbol, -1 )){
		g_string_append_printf( query, "'%s'", symbol );
	} else {
		query = g_string_append( query, "NULL" );
	}

	query = g_string_append( query, ")" );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		g_string_printf( query,
				"SELECT DEV_ID FROM OFA_T_DEVISES"
				"	WHERE DEV_CODE='%s'",
				ofo_devise_get_code( devise ));

		result = ofa_sgbd_query_ex( sgbd, NULL, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_devise_set_id( devise, atoi(( gchar * ) icol->data ));

			ofa_sgbd_free_result( result );

			ok = TRUE;
		}
	}

	g_string_free( query, TRUE );
	g_free( label );

	return( ok );
}

/**
 * ofo_devise_update:
 *
 * we deal here with an update of publicly modifiable devise properties
 * mnemo is not modifiable
 */
gboolean
ofo_devise_update( ofoDevise *devise, ofaSgbd *sgbd )
{
	GString *query;
	gchar *label;
	const gchar *symbol;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_devise_get_label( devise ));
	symbol = ofo_devise_get_symbol( devise );

	query = g_string_new( "UPDATE OFA_T_DEVISES SET " );

	g_string_append_printf( query, "DEV_LABEL='%s',", label );

	if( symbol && g_utf8_strlen( symbol, -1 )){
		g_string_append_printf( query, "DEV_SYMBOL='%s'", symbol );
	} else {
		query = g_string_append( query, "DEV_SYMBOL=NULL" );
	}

	g_string_append_printf( query,
			"	WHERE DEV_CODE='%s'", ofo_devise_get_code( devise ));

	ok = ofa_sgbd_query( sgbd, NULL, query->str );

	g_string_free( query, TRUE );
	g_free( label );

	return( ok );
}

/**
 * ofo_devise_delete:
 */
gboolean
ofo_devise_delete( ofoDevise *devise, ofaSgbd *sgbd )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_DEVISE( devise ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_DEVISES"
			"	WHERE DEV_CODE='%s'",
					ofo_devise_get_code( devise ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
