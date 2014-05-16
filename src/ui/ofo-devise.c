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

/* private class data
 */
struct _ofoDeviseClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofoDevisePrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	gchar   *mnemo;
	gchar   *label;
	gchar   *symbol;
};

static GObjectClass *st_parent_class  = NULL;

static GType  register_type( void );
static void   class_init( ofoDeviseClass *klass );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   instance_dispose( GObject *instance );
static void   instance_finalize( GObject *instance );

GType
ofo_devise_get_type( void )
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
	static const gchar *thisfn = "ofo_devise_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoDeviseClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoDevise ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofoDevise", &info, 0 );

	return( type );
}

static void
class_init( ofoDeviseClass *klass )
{
	static const gchar *thisfn = "ofo_devise_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoDeviseClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofo_devise_instance_init";
	ofoDevise *self;

	g_return_if_fail( OFO_IS_DEVISE( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFO_DEVISE( instance );

	self->private = g_new0( ofoDevisePrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_devise_instance_dispose";
	ofoDevisePrivate *priv;

	g_return_if_fail( OFO_IS_DEVISE( instance ));

	priv = ( OFO_DEVISE( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s - %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				priv->mnemo, priv->label );

		priv->dispose_has_run = TRUE;

		g_free( priv->mnemo );
		g_free( priv->label );
		g_free( priv->symbol );

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_devise_instance_finalize";
	ofoDevisePrivate *priv;

	g_return_if_fail( OFO_IS_DEVISE( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_DEVISE( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
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
			"SELECT DEV_CODE,DEV_LABEL,DEV_SYMBOL "
			"	FROM OFA_T_DEVISES "
			"	ORDER BY DEV_CODE ASC" );

	set = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		devise = ofo_devise_new();
		ofo_devise_set_mnemo( devise, ( gchar * ) icol->data );
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
		priv = OFO_DEVISE( ic->data )->private;
		g_debug( "%s: devise %s - %s", thisfn, priv->mnemo, priv->label );
	}
}

/**
 * ofo_devise_get_mnemo:
 */
const gchar *
ofo_devise_get_mnemo( const ofoDevise *devise )
{
	g_return_val_if_fail( OFO_IS_DEVISE( devise ), NULL );

	if( !devise->private->dispose_has_run ){

		return( devise->private->mnemo );
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

	if( !devise->private->dispose_has_run ){

		return( devise->private->label );
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

	if( !devise->private->dispose_has_run ){

		return( devise->private->symbol );
	}

	return( NULL );
}

/**
 * ofo_devise_set_mnemo:
 */
void
ofo_devise_set_mnemo( ofoDevise *devise, const gchar *mnemo )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->private->dispose_has_run ){

		devise->private->mnemo = g_strdup( mnemo );
	}
}

/**
 * ofo_devise_set_label:
 */
void
ofo_devise_set_label( ofoDevise *devise, const gchar *label )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->private->dispose_has_run ){

		devise->private->label = g_strdup( label );
	}
}

/**
 * ofo_devise_set_symbol:
 */
void
ofo_devise_set_symbol( ofoDevise *devise, const gchar *symbol )
{
	g_return_if_fail( OFO_IS_DEVISE( devise ));

	if( !devise->private->dispose_has_run ){

		devise->private->symbol = g_strdup( symbol );
	}
}

/**
 * ofo_devise_insert:
 *
 * we deal here with an update of publicly modifiable devise properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_devise_insert( ofoDevise *devise, ofaSgbd *sgbd )
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

	query = g_string_new( "INSERT INTO OFA_T_DEVISES" );

	g_string_append_printf( query,
			"	(DEV_CODE,DEV_LABEL,DEV_SYMBOL)"
			"	VALUES ('%s','%s',",
			ofo_devise_get_mnemo( devise ),
			label );

	if( symbol && g_utf8_strlen( symbol, -1 )){
		g_string_append_printf( query, "'%s'", symbol );
	} else {
		query = g_string_append( query, "NULL" );
	}

	query = g_string_append( query, ")" );

	ok = ofa_sgbd_query( sgbd, NULL, query->str );

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
			"	WHERE DEV_CODE='%s'", ofo_devise_get_mnemo( devise ));

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
					ofo_devise_get_mnemo( devise ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
