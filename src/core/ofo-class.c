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

#include "api/my-utils.h"
#include "api/ofa-boxed.h"
#include "api/ofa-dbms.h"
#include "api/ofa-iexportable.h"
#include "api/ofo-account.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

/* priv instance data
 */
enum {
	CLA_NUMBER = 1,
	CLA_LABEL,
	CLA_NOTES,
	CLA_UPD_USER,
	CLA_UPD_STAMP,
};

static const ofsBoxedDef st_boxed_defs[] = {
		{ OFA_BOXED_CSV( CLA_NUMBER ),
				OFA_TYPE_INTEGER,
				TRUE,					/* importable */
				FALSE },				/* export_csv_zero_as_empty */
		{ OFA_BOXED_CSV( CLA_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( CLA_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOXED_CSV( CLA_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOXED_CSV( CLA_UPD_STAMP ),
				OFA_TYPE_STRING,
				FALSE,
				TRUE },
		{ 0 }
};

/* priv instance data
 */
struct _ofoClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

OFO_BASE_DEFINE_GLOBAL( st_global, class )

static void       iexportable_iface_init( ofaIExportableInterface *iface );
static guint      iexportable_get_interface_version( const ofaIExportable *instance );
static GList     *class_load_dataset( void );
static ofoClass  *class_find_by_number( GList *set, gint number );
static void       class_set_upd_user( ofoClass *class, const gchar *user );
static void       class_set_upd_stamp( ofoClass *class, const GTimeVal *stamp );
static gboolean   class_do_insert( ofoClass *class, const ofaDbms *dbms, const gchar *user );
static gboolean   class_do_update( ofoClass *class, gint prev_id, const ofaDbms *dbms, const gchar *user );
static gboolean   class_do_delete( ofoClass *class, const ofaDbms *dbms );
static gint       class_cmp_by_number( const ofoClass *a, gpointer pnum );
static gint       class_cmp_by_ptr( const ofoClass *a, const ofoClass *b );
static gboolean   iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, const ofoDossier *dossier );
static gboolean   class_do_drop_content( const ofaDbms *dbms );

G_DEFINE_TYPE_EXTENDED( ofoClass, ofo_class, OFO_TYPE_BASE, 0, \
		G_IMPLEMENT_INTERFACE (OFA_TYPE_IEXPORTABLE, iexportable_iface_init ));

static void
class_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_class_finalize";

	g_debug( "%s: instance=%p (%s): %d - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_boxed_get_int( OFO_BASE( instance )->prot->fields, CLA_NUMBER ),
			ofa_boxed_get_string( OFO_BASE( instance )->prot->fields, CLA_LABEL ));

	g_return_if_fail( instance && OFO_IS_CLASS( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_class_parent_class )->finalize( instance );
}

static void
class_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_CLASS( instance ));

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

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_CLASS, ofoClassPrivate );
}

static void
ofo_class_class_init( ofoClassClass *klass )
{
	static const gchar *thisfn = "ofo_class_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_finalize;

	g_type_class_add_private( klass, sizeof( ofoClassPrivate ));
}

static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_class_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/**
 * ofo_class_new:
 */
ofoClass *
ofo_class_new( void )
{
	ofoClass *class;

	class = g_object_new( OFO_TYPE_CLASS, NULL );
	ofo_base_init_fields_list( st_boxed_defs, OFO_BASE( class ));

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
	return(
			ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( OFO_DOSSIER( st_global->dossier )),
					"OFA_T_CLASSES ORDER BY CLA_NUMBER ASC",
					OFO_TYPE_CLASS ));
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
	ofo_base_getter( CLASS, class , int, 0, CLA_NUMBER );
}

/**
 * ofo_class_get_label:
 */
const gchar *
ofo_class_get_label( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_LABEL );
}

/**
 * ofo_class_get_notes:
 */
const gchar *
ofo_class_get_notes( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_NOTES );
}

/**
 * ofo_class_get_upd_user:
 */
const gchar *
ofo_class_get_upd_user( const ofoClass *class )
{
	ofo_base_getter( CLASS, class , string, NULL, CLA_UPD_USER );
}

/**
 * ofo_class_get_upd_stamp:
 */
const GTimeVal *
ofo_class_get_upd_stamp( const ofoClass *class )
{
	ofo_base_getter( CLASS, class, timestamp, NULL, CLA_UPD_STAMP );
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
	return( ofo_class_is_valid_number( number ) &&
			ofo_class_is_valid_label( label ));
}

/**
 * ofo_class_is_valid_number:
 *
 * Returns: %TRUE if the provided number is a valid class number
 */
gboolean
ofo_class_is_valid_number( gint number )
{
	return( number > 0 && number < 10 );
}

/**
 * ofo_class_is_valid_label:
 *
 * Returns: %TRUE if the provided label is a valid class label
 */
gboolean
ofo_class_is_valid_label( const gchar *label )
{
	return( label && g_utf8_strlen( label, -1 ) > 0 );
}

/**
 * ofo_class_is_deletable:
 *
 * Returns: %TRUE if the provided object may be safely deleted.
 *
 * Though the class in only used as tabs title in the accounts notebook,
 * and though we are providing default values, a class stay a reference
 * table. A row is only deletable if it is not referenced by any other
 * object.
 */
gboolean
ofo_class_is_deletable( const ofoClass *class )
{
	gboolean used_by_accounts;
	gboolean deletable;

	used_by_accounts = ofo_account_use_class(
								OFO_DOSSIER( st_global->dossier ), ofo_class_get_number( class ));
	deletable = !used_by_accounts;

	return( deletable );
}

/**
 * ofo_class_set_number:
 */
void
ofo_class_set_number( ofoClass *class, gint number )
{
	if( !ofo_class_is_valid_number( number )){
		g_return_if_reached();
	}

	ofo_base_setter( CLASS, class, int, CLA_NUMBER, number );
}

/**
 * ofo_class_set_label:
 */
void
ofo_class_set_label( ofoClass *class, const gchar *label )
{
	if( !ofo_class_is_valid_label( label )){
		g_return_if_reached();
	}

	ofo_base_setter( CLASS, class, string, CLA_LABEL, label );
}

/**
 * ofo_class_set_notes:
 */
void
ofo_class_set_notes( ofoClass *class, const gchar *notes )
{
	ofo_base_setter( CLASS, class, string, CLA_NOTES, notes );
}

/*
 * ofo_class_set_upd_user:
 */
static void
class_set_upd_user( ofoClass *class, const gchar *user )
{
	ofo_base_setter( CLASS, class, string, CLA_UPD_USER, user );
}

/*
 * ofo_class_set_upd_stamp:
 */
static void
class_set_upd_stamp( ofoClass *class, const GTimeVal *stamp )
{
	ofo_base_setter( CLASS, class, timestamp, CLA_UPD_STAMP, stamp );
}

/**
 * ofo_class_insert:
 */
gboolean
ofo_class_insert( ofoClass *class )
{
	static const gchar *thisfn = "ofo_class_insert";

	g_return_val_if_fail( OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_debug( "%s: class=%p", thisfn, ( void * ) class );

		if( class_do_insert(
					class,
					ofo_dossier_get_dbms( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_ADD_TO_DATASET( st_global, class );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
class_do_insert( ofoClass *class, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	query = g_string_new( "INSERT INTO OFA_T_CLASSES " );

	label = my_utils_quote( ofo_class_get_label( class ));
	notes = my_utils_quote( ofo_class_get_notes( class ));

	g_string_append_printf( query,
			"	(CLA_NUMBER,CLA_LABEL,CLA_NOTES,"
			"	 CLA_UPD_USER,CLA_UPD_STAMP) VALUES "
			"	(%d,'%s',",
					ofo_class_get_number( class ),
					label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query, "'%s','%s')", user, stamp_str );

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	if( ok ){
		class_set_upd_user( class, user );
		class_set_upd_stamp( class, &stamp );
	}

	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_class_update:
 */
gboolean
ofo_class_update( ofoClass *class, gint prev_id )
{
	static const gchar *thisfn = "ofo_class_update";
	gchar *str;

	g_return_val_if_fail( OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_debug( "%s: class=%p", thisfn, ( void * ) class );

		if( class_do_update(
					class,
					prev_id,
					ofo_dossier_get_dbms( OFO_DOSSIER( st_global->dossier )),
					ofo_dossier_get_user( OFO_DOSSIER( st_global->dossier )))){

			str = g_strdup_printf( "%d", prev_id );
			OFO_BASE_UPDATE_DATASET( st_global, class, str );
			g_free( str );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
class_do_update( ofoClass *class, gint prev_id, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes, *stamp_str;
	GTimeVal stamp;
	gboolean ok;

	ok = FALSE;
	label = my_utils_quote( ofo_class_get_label( class ));
	notes = my_utils_quote( ofo_class_get_notes( class ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_CLASSES SET " );

	g_string_append_printf( query, "	CLA_NUMBER=%d,", ofo_class_get_number( class ));
	g_string_append_printf( query, "	CLA_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "CLA_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "CLA_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	CLA_UPD_USER='%s',CLA_UPD_STAMP='%s'"
			"	WHERE CLA_NUMBER=%d", user, stamp_str, prev_id );

	if( ofa_dbms_query( dbms, query->str, TRUE )){
		class_set_upd_user( class, user );
		class_set_upd_stamp( class, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( label );
	g_free( notes );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_class_delete:
 */
gboolean
ofo_class_delete( ofoClass *class )
{
	static const gchar *thisfn = "ofo_class_delete";

	g_return_val_if_fail( OFO_IS_CLASS( class ), FALSE );
	g_return_val_if_fail( st_global && st_global->dossier && OFO_IS_DOSSIER( st_global->dossier ), FALSE );
	g_return_val_if_fail( ofo_class_is_deletable( class ), FALSE );

	if( !OFO_BASE( class )->prot->dispose_has_run ){

		g_debug( "%s: class=%p", thisfn, ( void * ) class );

		if( class_do_delete(
					class,
					ofo_dossier_get_dbms( OFO_DOSSIER( st_global->dossier )))){

			OFO_BASE_REMOVE_FROM_DATASET( st_global, class );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
class_do_delete( ofoClass *class, const ofaDbms *dbms )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_CLASSES"
			"	WHERE CLA_NUMBER=%d",
					ofo_class_get_number( class ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

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
	gint bnum;

	bnum = ofo_class_get_number( b );
	return( class_cmp_by_number( a, GINT_TO_POINTER( bnum )));
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaExportSettings *settings, const ofoDossier *dossier )
{
	GList *it;
	GSList *lines;
	gchar *str;
	gboolean with_headers, ok;
	gulong count;
	gchar field_sep;

	OFO_BASE_SET_GLOBAL( st_global, dossier, class );

	with_headers = ofa_export_settings_get_headers( settings );
	field_sep = ofa_export_settings_get_field_sep( settings );

	count = ( gulong ) g_list_length( st_global->dataset );
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_boxed_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=st_global->dataset ; it ; it=it->next ){
		str = ofa_boxed_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, '\0' );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofo_class_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - class number
 * - label
 * - notes (opt)
 *
 * Replace the whole table with the provided datas.
 */
void
ofo_class_import_csv( const ofoDossier *dossier, GSList *lines, gboolean with_header )
{
	static const gchar *thisfn = "ofo_class_import_csv";
	ofoClass *class;
	GSList *ili, *ico;
	GList *new_set, *ise;
	gint count;
	gint errors;
	gint number;
	const gchar *str;
	gchar *splitted;

	g_debug( "%s: dossier=%p, lines=%p (count=%d), with_header=%s",
			thisfn,
			( void * ) dossier,
			( void * ) lines, g_slist_length( lines ),
			with_header ? "True":"False" );

	OFO_BASE_SET_GLOBAL( st_global, dossier, class );

	new_set = NULL;
	count = 0;
	errors = 0;

	for( ili=lines ; ili ; ili=ili->next ){
		count += 1;
		if( !( count == 1 && with_header )){
			class = ofo_class_new();
			ico=ili->data;

			/* class number */
			str = ( const gchar * ) ico->data;
			number = atoi( str );
			if( number < 1 || number > 9 ){
				g_warning( "%s: (line %d) invalid class number: %d", thisfn, count, number );
				errors += 1;
				continue;
			}
			ofo_class_set_number( class, number );

			/* class label */
			ico = ico->next;
			str = ( const gchar * ) ico->data;
			if( !str || !g_utf8_strlen( str, -1 )){
				g_warning( "%s: (line %d) empty label", thisfn, count );
				errors += 1;
				continue;
			}
			ofo_class_set_label( class, str );

			/* notes
			 * we are tolerant on the last field... */
			ico = ico->next;
			if( ico ){
				str = ( const gchar * ) ico->data;
				if( str && g_utf8_strlen( str, -1 )){
					splitted = my_utils_import_multi_lines( str );
					ofo_class_set_notes( class, splitted );
					g_free( splitted );
				}
			} else {
				continue;
			}

			new_set = g_list_prepend( new_set, class );
		}
	}

	if( !errors ){
		st_global->send_signal_new = FALSE;

		class_do_drop_content( ofo_dossier_get_dbms( dossier ));

		for( ise=new_set ; ise ; ise=ise->next ){
			class_do_insert(
					OFO_CLASS( ise->data ),
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ));
		}

		g_list_free( new_set );

		if( st_global ){
			g_list_free_full( st_global->dataset, ( GDestroyNotify ) g_object_unref );
			st_global->dataset = NULL;
		}
		g_signal_emit_by_name( G_OBJECT( dossier ), OFA_SIGNAL_RELOAD_DATASET, OFO_TYPE_CLASS );

		st_global->send_signal_new = TRUE;
	}
}

static gboolean
class_do_drop_content( const ofaDbms *dbms )
{
	return( ofa_dbms_query( dbms, "DELETE FROM OFA_T_CLASSES", TRUE ));
}
