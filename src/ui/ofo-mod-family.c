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
#include "ui/ofo-mod-family.h"

/* private class data
 */
struct _ofoModFamilyClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofoModFamilyPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	gint     id;
	gchar   *label;
	gchar   *notes;
	gchar   *maj_user;
	GTimeVal maj_stamp;
};

static ofoBaseClass *st_parent_class  = NULL;

static GType  register_type( void );
static void   class_init( ofoModFamilyClass *klass );
static void   instance_init( GTypeInstance *instance, gpointer klass );
static void   instance_dispose( GObject *instance );
static void   instance_finalize( GObject *instance );

GType
ofo_mod_family_get_type( void )
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
	static const gchar *thisfn = "ofo_mod_family_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoModFamilyClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofoModFamily ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoModFamily", &info, 0 );

	return( type );
}

static void
class_init( ofoModFamilyClass *klass )
{
	static const gchar *thisfn = "ofo_mod_family_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofoModFamilyClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofo_mod_family_instance_init";
	ofoModFamily *self;

	g_return_if_fail( OFO_IS_MOD_FAMILY( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFO_MOD_FAMILY( instance );

	self->private = g_new0( ofoModFamilyPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_mod_family_instance_dispose";
	ofoModFamilyPrivate *priv;

	g_return_if_fail( OFO_IS_MOD_FAMILY( instance ));

	priv = ( OFO_MOD_FAMILY( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s): %s",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
				priv->label );

		priv->dispose_has_run = TRUE;

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
	static const gchar *thisfn = "ofo_mod_family_instance_finalize";
	ofoModFamilyPrivate *priv;

	g_return_if_fail( OFO_IS_MOD_FAMILY( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFO_MOD_FAMILY( instance )->private;

	g_free( priv );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofo_mod_family_new:
 */
ofoModFamily *
ofo_mod_family_new( void )
{
	ofoModFamily *mod_family;

	mod_family = g_object_new( OFO_TYPE_MOD_FAMILY, NULL );

	return( mod_family );
}

/**
 * ofo_mod_family_load_set:
 *
 * Loads/reloads the ordered list of mod_familys
 */
GList *
ofo_mod_family_load_set( ofaSgbd *sgbd )
{
	static const gchar *thisfn = "ofo_mod_family_load_set";
	GSList *result, *irow, *icol;
	ofoModFamily *family;
	GList *set;

	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), NULL );

	g_debug( "%s: sgbd=%p", thisfn, ( void * ) sgbd );

	result = ofa_sgbd_query_ex( sgbd, NULL,
			"SELECT FAM_ID,FAM_LABEL,FAM_NOTES,"
			"	FAM_MAJ_USER,FAM_MAJ_STAMP"
			"	FROM OFA_T_MOD_FAMILY "
			"	ORDER BY FAM_ID ASC" );

	set = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		family = ofo_mod_family_new();
		ofo_mod_family_set_id( family, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_mod_family_set_label( family, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_mod_family_set_notes( family, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_mod_family_set_maj_user( family, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_mod_family_set_maj_stamp( family, my_utils_stamp_from_str(( gchar * ) icol->data ));

		set = g_list_prepend( set, family );
	}

	ofa_sgbd_free_result( result );

	return( g_list_reverse( set ));
}

/**
 * ofo_mod_family_dump_set:
 */
void
ofo_mod_family_dump_set( GList *set )
{
	static const gchar *thisfn = "ofo_mod_family_dump_set";
	ofoModFamilyPrivate *priv;
	GList *ic;

	for( ic=set ; ic ; ic=ic->next ){
		priv = OFO_MOD_FAMILY( ic->data )->private;
		g_debug( "%s: mod_family %s", thisfn, priv->label );
	}
}

/**
 * ofo_mod_family_get_id:
 */
gint
ofo_mod_family_get_id( const ofoModFamily *family )
{
	gint id;

	g_return_val_if_fail( OFO_IS_MOD_FAMILY( family ), NULL );

	id = -1;

	if( !family->private->dispose_has_run ){

		id = family->private->id;
	}

	return( id );
}

/**
 * ofo_mod_family_get_label:
 */
const gchar *
ofo_mod_family_get_label( const ofoModFamily *family )
{
	const gchar *label = NULL;

	g_return_val_if_fail( OFO_IS_MOD_FAMILY( family ), NULL );

	if( !family->private->dispose_has_run ){

		label = family->private->label;
	}

	return( label );
}

/**
 * ofo_mod_family_get_notes:
 */
const gchar *
ofo_mod_family_get_notes( const ofoModFamily *family )
{
	const gchar *notes = NULL;

	g_return_val_if_fail( OFO_IS_MOD_FAMILY( family ), NULL );

	if( !family->private->dispose_has_run ){

		notes = family->private->notes;
	}

	return( notes );
}

/**
 * ofo_mod_family_set_id:
 */
void
ofo_mod_family_set_id( ofoModFamily *family, gint id )
{
	g_return_if_fail( OFO_IS_MOD_FAMILY( family ));

	if( !family->private->dispose_has_run ){

		family->private->id = id;
	}
}

/**
 * ofo_mod_family_set_label:
 */
void
ofo_mod_family_set_label( ofoModFamily *family, const gchar *label )
{
	g_return_if_fail( OFO_IS_MOD_FAMILY( family ));

	if( !family->private->dispose_has_run ){

		family->private->label = g_strdup( label );
	}
}

/**
 * ofo_mod_family_set_notes:
 */
void
ofo_mod_family_set_notes( ofoModFamily *family, const gchar *notes )
{
	g_return_if_fail( OFO_IS_MOD_FAMILY( family ));

	if( !family->private->dispose_has_run ){

		family->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_mod_family_set_maj_user:
 */
void
ofo_mod_family_set_maj_user( ofoModFamily *family, const gchar *maj_user )
{
	g_return_if_fail( OFO_IS_MOD_FAMILY( family ));

	if( !family->private->dispose_has_run ){

		family->private->maj_user = g_strdup( maj_user );
	}
}

/**
 * ofo_mod_family_set_maj_stamp:
 */
void
ofo_mod_family_set_maj_stamp( ofoModFamily *family, const GTimeVal *maj_stamp )
{
	g_return_if_fail( OFO_IS_MOD_FAMILY( family ));

	if( !family->private->dispose_has_run ){

		memcpy( &family->private->maj_stamp, maj_stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_mod_family_insert:
 *
 * we deal here with an update of publicly modifiable mod_family properties
 * so it is not needed to check the date of closing
 */
gboolean
ofo_mod_family_insert( ofoModFamily *family, ofaSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;
	GSList *result, *icol;

	g_return_val_if_fail( OFO_IS_MOD_FAMILY( family ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_mod_family_get_label( family ));
	notes = my_utils_quote( ofo_mod_family_get_notes( family ));
	stamp = my_utils_timestamp();

	query = g_string_new( "INSERT INTO OFA_T_MOD_FAMILY" );

	g_string_append_printf( query,
			"	(FAM_LABEL,FAM_NOTES,"
			"	FAM_MAJ_USER, FAM_MAJ_STAMP) VALUES ('%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", user, stamp );

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		ofo_mod_family_set_maj_user( family, user );
		ofo_mod_family_set_maj_stamp( family, my_utils_stamp_from_str( stamp ));

		g_string_printf( query,
				"SELECT FAM_ID FROM OFA_T_MOD_FAMILY"
				"	WHERE FAM_LABEL='%s' AND FAM_MAJ_STAMP='%s'",
				label, stamp );

		result = ofa_sgbd_query_ex( sgbd, NULL, query->str );

		if( result ){
			icol = ( GSList * ) result->data;
			ofo_mod_family_set_id( family, atoi(( gchar * ) icol->data ));

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
 * ofo_mod_family_update:
 */
gboolean
ofo_mod_family_update( ofoModFamily *family, ofaSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp;

	g_return_val_if_fail( OFO_IS_MOD_FAMILY( family ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_mod_family_get_label( family ));
	notes = my_utils_quote( ofo_mod_family_get_notes( family ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_MOD_FAMILY SET " );

	g_string_append_printf( query, "FAM_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "FAM_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "FAM_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	FAM_MAJ_USER='%s',FAM_MAJ_STAMP='%s'"
			"	WHERE FAM_ID=%d",
					user,
					stamp,
					ofo_mod_family_get_id( family ));

	if( ofa_sgbd_query( sgbd, NULL, query->str )){

		ofo_mod_family_set_maj_user( family, user );
		ofo_mod_family_set_maj_stamp( family, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );

	return( ok );
}

/**
 * ofo_mod_family_delete:
 */
gboolean
ofo_mod_family_delete( ofoModFamily *family, ofaSgbd *sgbd, const gchar *user )
{
	gchar *query;
	gboolean ok;

	g_return_val_if_fail( OFO_IS_MOD_FAMILY( family ), FALSE );
	g_return_val_if_fail( OFA_IS_SGBD( sgbd ), FALSE );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_MOD_FAMILY"
			"	WHERE FAM_ID=%d",
					ofo_mod_family_get_id( family ));

	ok = ofa_sgbd_query( sgbd, NULL, query );

	g_free( query );

	return( ok );
}
