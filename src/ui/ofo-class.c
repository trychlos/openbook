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
#include "ui/ofo-class.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-sgbd.h"

/* priv instance data
 */
struct _ofoClassPrivate {

	/* sgbd data
	 */
	gint       number;
	gchar     *label;
	gchar     *notes;
	gchar     *maj_user;
	GTimeVal   maj_stamp;
};

G_DEFINE_TYPE( ofoClass, ofo_class, OFO_TYPE_BASE )

OFO_BASE_DEFINE_GLOBAL( st_global, class )

static ofoClass  *ofo_class_new( void );
static GList     *class_load_dataset( void );
static ofoClass  *class_find_by_number( GList *set, gint number );
static gint       class_cmp_by_number( const ofoClass *a, gpointer pnum );
static gint       class_cmp_by_ptr( const ofoClass *a, const ofoClass *b );
static gboolean   class_do_update( ofoClass *class, const ofoSgbd *sgbd, const gchar *user );

static void
ofo_class_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_class_finalize";
	ofoClassPrivate *priv;

	priv = OFO_CLASS( instance )->private;

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			priv->label );

	/* free data members here */
	g_free( priv->label );
	g_free( priv->notes );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_class_parent_class )->finalize( instance );
}

static void
ofo_class_dispose( GObject *instance )
{
	g_return_if_fail( OFO_IS_CLASS( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_class_parent_class )->dispose( instance );
}

static void
ofo_class_init( ofoClass *self )
{
	static const gchar *thisfn = "ofo_class_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofoClassPrivate, 1 );

	self->private->number = OFO_BASE_UNSET_ID;
}

static void
ofo_class_class_init( ofoClassClass *klass )
{
	static const gchar *thisfn = "ofo_class_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_class_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_class_finalize;
}

/**
 * ofo_class_new:
 */
static ofoClass *
ofo_class_new( void )
{
	ofoClass *class;

	class = g_object_new( OFO_TYPE_CLASS, NULL );

	return( class );
}

/**
 * ofo_class_get_dataset:
 * @dossier: the currently opened #ofoDossier dossier.
 *
 * Returns: The list of #ofoClass classs, ordered by ascending
 * number. The returned list is owned by the #ofoClass class, and
 * should not be freed by the caller.
 *
 * Note: The list is returned (and maintained) sorted for debug
 * facility only. Any way, the display treeview (#ofoClasssSet class)
 * makes use of a sortable model which doesn't care of the order of the
 * provided dataset.
 */
GList *
ofo_class_get_dataset( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_class_get_dataset";

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	OFO_BASE_SET_GLOBAL( st_global, dossier, class );

	return( st_global->dataset );
}

static GList *
class_load_dataset( void )
{
	GSList *result, *irow, *icol;
	ofoClass *class;
	GList *dataset;
	const ofoSgbd *sgbd;

	sgbd = ofo_dossier_get_sgbd( OFO_DOSSIER( st_global->dossier ));

	result = ofo_sgbd_query_ex( sgbd,
			"SELECT CLA_NUMBER,CLA_LABEL,"
			"	CLA_NOTES,CLA_MAJ_USER,CLA_MAJ_STAMP "
			"	FROM OFA_T_CLASSES "
			"	ORDER BY CLA_NUMBER ASC" );

	dataset = NULL;

	for( irow=result ; irow ; irow=irow->next ){
		icol = ( GSList * ) irow->data;
		class = ofo_class_new();
		ofo_class_set_number( class, atoi(( gchar * ) icol->data ));
		icol = icol->next;
		ofo_class_set_label( class, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_class_set_notes( class, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_class_set_maj_user( class, ( gchar * ) icol->data );
		icol = icol->next;
		ofo_class_set_maj_stamp( class, my_utils_stamp_from_str(( gchar * ) icol->data ));

		dataset = g_list_prepend( dataset, class );
	}

	ofo_sgbd_free_result( result );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_class_get_by_number:
 *
 * Returns: the searched class, or %NULL.
 *
 * The returned object is owned by the #ofoClass class, and should
 * not be unreffed by the caller.
 */
ofoClass *
ofo_class_get_by_number( const ofoDossier *dossier, gint number )
{
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	OFO_BASE_SET_GLOBAL( st_global, dossier, class );

	return( class_find_by_number( st_global->dataset, number ));
}

static ofoClass *
class_find_by_number( GList *set, gint number )
{
	GList *found;

	found = g_list_find_custom(
				set, GINT_TO_POINTER( number ), ( GCompareFunc ) class_cmp_by_number );
	if( found ){
		return( OFO_CLASS( found->data ));
	}

	return( NULL );
}

/**
 * ofo_class_get_number:
 */
gint
ofo_class_get_number( const ofoClass *class )
{
	g_return_val_if_fail( OFO_IS_CLASS( class ), OFO_BASE_UNSET_ID );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		return( class->private->number );
	}

	g_assert_not_reached();
	return( OFO_BASE_UNSET_ID );
}

/**
 * ofo_class_get_label:
 */
const gchar *
ofo_class_get_label( const ofoClass *class )
{
	g_return_val_if_fail( OFO_IS_CLASS( class ), NULL );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		return(( const gchar * ) class->private->label );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_class_get_notes:
 */
const gchar *
ofo_class_get_notes( const ofoClass *class )
{
	g_return_val_if_fail( OFO_IS_CLASS( class ), NULL );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		return(( const gchar * ) class->private->notes );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_class_get_maj_user:
 */
const gchar *
ofo_class_get_maj_user( const ofoClass *class )
{
	g_return_val_if_fail( OFO_IS_CLASS( class ), NULL );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		return(( const gchar * ) class->private->maj_user );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_class_get_maj_stamp:
 */
const GTimeVal *
ofo_class_get_maj_stamp( const ofoClass *class )
{
	g_return_val_if_fail( OFO_IS_CLASS( class ), NULL );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &class->private->maj_stamp );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_class_is_valid:
 *
 * Returns: %TRUE if the provided data makes the ofoClass a valid
 * object.
 *
 * Note that this does NOT check for key duplicate.
 */
gboolean
ofo_class_is_valid( gint number, const gchar *label )
{
	return( number > 0 && number < 10 &&
			label && g_utf8_strlen( label, -1 ));
}

/**
 * ofo_class_set_number:
 */
void
ofo_class_set_number( ofoClass *class, gint number )
{
	g_return_if_fail( OFO_IS_CLASS( class ));

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		class->private->number = number;
	}
}

/**
 * ofo_class_set_label:
 */
void
ofo_class_set_label( ofoClass *class, const gchar *label )
{
	g_return_if_fail( OFO_IS_CLASS( class ));

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_free( class->private->label );
		class->private->label = g_strdup( label );
	}
}

/**
 * ofo_class_set_notes:
 */
void
ofo_class_set_notes( ofoClass *class, const gchar *notes )
{
	g_return_if_fail( OFO_IS_CLASS( class ));

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_free( class->private->notes );
		class->private->notes = g_strdup( notes );
	}
}

/**
 * ofo_class_set_maj_user:
 */
void
ofo_class_set_maj_user( ofoClass *class, const gchar *user )
{
	g_return_if_fail( OFO_IS_CLASS( class ));

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_free( class->private->maj_user );
		class->private->maj_user = g_strdup( user );
	}
}

/**
 * ofo_class_set_maj_stamp:
 */
void
ofo_class_set_maj_stamp( ofoClass *class, const GTimeVal *stamp )
{
	g_return_if_fail( OFO_IS_CLASS( class ));

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		memcpy( &class->private->maj_stamp, stamp, sizeof( GTimeVal ));
	}
}

/**
 * ofo_class_update:
 */
gboolean
ofo_class_update( ofoClass *class, const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_class_update";

	g_return_val_if_fail( OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_debug( "%s: class=%p, dossier=%p",
				thisfn, ( void * ) class, ( void * ) dossier );

		OFO_BASE_SET_GLOBAL( st_global, dossier, class );

		if( class_do_update(
					class,
					ofo_dossier_get_sgbd( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFO_BASE_UPDATE_DATASET( st_global, class );
			return( TRUE );
		}
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
class_do_update( ofoClass *class, const ofoSgbd *sgbd, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_class_get_label( class ));
	notes = my_utils_quote( ofo_class_get_notes( class ));
	stamp = my_utils_timestamp();

	query = g_string_new( "UPDATE OFA_T_CLASSES SET " );

	g_string_append_printf( query, "	CLA_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "CLA_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CLA_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	CLA_MAJ_USER='%s',CLA_MAJ_STAMP='%s'"
			"	WHERE CLA_NUMBER=%d", user, stamp, ofo_class_get_number( class ));

	if( ofo_sgbd_query( sgbd, query->str )){

		ofo_class_set_maj_user( class, user );
		ofo_class_set_maj_stamp( class, my_utils_stamp_from_str( stamp ));
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp );

	return( ok );
}

static gint
class_cmp_by_number( const ofoClass *a, gpointer pnum )
{
	gint anum, bnum;

	anum = ofo_class_get_number( a );
	bnum = GPOINTER_TO_INT( pnum );

	if( anum < bnum ){
		return( -1 );
	}
	if( anum > bnum ){
		return( 1 );
	}
	return( 0 );
}

static gint
class_cmp_by_ptr( const ofoClass *a, const ofoClass *b )
{
	return( class_cmp_by_number( a, GINT_TO_POINTER( ofo_class_get_number( b ))));
}

/**
 * ofo_class_get_csv:
 */
GSList *
ofo_class_get_csv( const ofoDossier *dossier )
{
	GList *set;
	GSList *lines;
	gchar *str, *stamp;
	const gchar *notes, *muser;
	ofoClass *class;

	OFO_BASE_SET_GLOBAL( st_global, dossier, class );

	lines = NULL;

	str = g_strdup_printf( "Number;Label;Notes;MajUser;MajStamp" );
	lines = g_slist_prepend( lines, str );

	for( set=st_global->dataset ; set ; set=set->next ){
		class = OFO_CLASS( set->data );

		stamp = my_utils_str_from_stamp( ofo_class_get_maj_stamp( class ));
		notes = ofo_class_get_notes( class );
		muser = ofo_class_get_maj_user( class );

		str = g_strdup_printf( "%d;%s;%s;%s;%s",
				ofo_class_get_number( class ),
				ofo_class_get_label( class ),
				notes ? notes : "",
				muser ? muser : "",
				muser ? stamp : "" );

		g_free( stamp );

		lines = g_slist_prepend( lines, str );
	}

	return( g_slist_reverse( lines ));
}
