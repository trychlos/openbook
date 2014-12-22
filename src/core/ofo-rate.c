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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-dbms.h"
#include "api/ofa-file-format.h"
#include "api/ofa-idataset.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* priv instance data
 */
enum {
	RAT_MNEMO = 1,
	RAT_LABEL,
	RAT_NOTES,
	RAT_UPD_USER,
	RAT_UPD_STAMP,
	RAT_BEGIN,
	RAT_END,
	RAT_RATE,
};

static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( RAT_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( RAT_LABEL ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( RAT_NOTES ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( RAT_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( RAT_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

static const ofsBoxDef st_validities_defs[] = {
		{ RAT_MNEMO,
				"RAT_MNEMO",
				NULL,
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ RAT_BEGIN,
				"RAT_VAL_BEG",
				"RatValidityBegin",
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ RAT_END,
				"RAT_VAL_END",
				"RatValidityEnd",
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ RAT_RATE,
				"RAT_VAL_RATE",
				"RatRate",
				OFA_TYPE_AMOUNT,
				TRUE,
				FALSE },
		{ 0 }
};

struct _ofoRatePrivate {

	/* the validities of the rate as a GList of GList fields
	 */
	GList *validities;
};

static ofoBaseClass *ofo_rate_parent_class = NULL;

static GList    *rate_load_dataset( ofoDossier *dossier );
static ofoRate  *rate_find_by_mnemo( GList *set, const gchar *mnemo );
static void      rate_set_upd_user( ofoRate *rate, const gchar *user );
static void      rate_set_upd_stamp( ofoRate *rate, const GTimeVal *stamp );
static GList    *rate_val_new_detail( const gchar *mnemo, const GDate *begin, const GDate *end, ofxAmount value );
static void      rate_val_add_detail( ofoRate *rate, GList *detail );
static gboolean  rate_do_insert( ofoRate *rate, const ofaDbms *dbms, const gchar *user );
static gboolean  rate_insert_main( ofoRate *rate, const ofaDbms *dbms, const gchar *user );
static gboolean  rate_delete_validities( ofoRate *rate, const ofaDbms *dbms );
static gboolean  rate_insert_validities( ofoRate *rate, const ofaDbms *dbms );
static gboolean  rate_insert_validity( ofoRate *rate, GList *detail, const ofaDbms *dbms );
static gboolean  rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user );
static gboolean  rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user );
static gboolean  rate_do_delete( ofoRate *rate, const ofaDbms *dbms );
static gint      rate_cmp_by_mnemo( const ofoRate *a, const gchar *mnemo );
static gint      rate_cmp_by_ptr( const ofoRate *a, const ofoRate *b );
static gint      rate_cmp_by_validity( const ofsRateValidity *a, const ofsRateValidity *b, gboolean *consistent );
static void      iexportable_iface_init( ofaIExportableInterface *iface );
static guint     iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean  iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier );
static void      iimportable_iface_init( ofaIImportableInterface *iface );
static guint     iimportable_get_interface_version( const ofaIImportable *instance );
static gboolean  iimportable_import( ofaIImportable *exportable, GSList *lines, ofoDossier *dossier );
static ofoRate  *rate_import_csv_rate( ofaIImportable *exportable, GSList *fields, guint count, guint *errors );
static GList    *rate_import_csv_validity( ofaIImportable *exportable, GSList *fields, guint count, guint *errors, gchar **mnemo );
static gboolean  rate_do_drop_content( const ofaDbms *dbms );

OFA_IDATASET_LOAD( RATE, rate );

static void
rate_free_validity( GList *fields )
{
	ofa_box_free_fields_list( fields );
}

static void
rate_free_validities( ofoRate *rate )
{
	g_list_free_full( rate->priv->validities, ( GDestroyNotify ) rate_free_validity );
	rate->priv->validities = NULL;
}

static void
rate_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_rate_finalize";

	g_debug( "%s: instance=%p (%s): %s - %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, RAT_MNEMO ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, RAT_LABEL ));

	g_return_if_fail( instance && OFO_IS_RATE( instance ));

	/* free data members here */

	rate_free_validities( OFO_RATE( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_rate_parent_class )->finalize( instance );
}

static void
rate_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_RATE( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_rate_parent_class )->dispose( instance );
}

static void
ofo_rate_init( ofoRate *self )
{
	static const gchar *thisfn = "ofo_rate_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_RATE, ofoRatePrivate );
}

static void
ofo_rate_class_init( ofoRateClass *klass )
{
	static const gchar *thisfn = "ofo_rate_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_rate_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = rate_dispose;
	G_OBJECT_CLASS( klass )->finalize = rate_finalize;

	g_type_class_add_private( klass, sizeof( ofoRatePrivate ));
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_rate_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoRateClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) ofo_rate_class_init,
		NULL,
		NULL,
		sizeof( ofoRate ),
		0,
		( GInstanceInitFunc ) ofo_rate_init
	};

	static const GInterfaceInfo iexportable_iface_info = {
		( GInterfaceInitFunc ) iexportable_iface_init,
		NULL,
		NULL
	};

	static const GInterfaceInfo iimportable_iface_info = {
		( GInterfaceInitFunc ) iimportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoRate", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	g_type_add_interface_static( type, OFA_TYPE_IIMPORTABLE, &iimportable_iface_info );

	return( type );
}

GType
ofo_rate_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

static GList *
rate_load_dataset( ofoDossier *dossier )
{
	GList *dataset, *it;
	ofoRate *rate;
	gchar *from;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					ofo_dossier_get_dbms( dossier ),
					"OFA_T_RATES ORDER BY RAT_MNEMO ASC",
					OFO_TYPE_RATE );

	for( it=dataset ; it ; it=it->next ){

		rate = OFO_RATE( it->data );
		from = g_strdup_printf( "OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'", ofo_rate_get_mnemo( rate ));
		rate->priv->validities =
				ofo_base_load_rows( st_validities_defs, ofo_dossier_get_dbms( dossier ), from );
		g_free( from );

		/*GList *iv;
		for( iv=rate->priv->validities ; iv ; iv=iv->next ){
			const gchar *mnemo = ofa_box_get_string( iv->data, RAT_MNEMO );
			const GDate *dbegin = ofa_box_get_date( iv->data, RAT_BEGIN );
			const GDate *dend = ofa_box_get_date( iv->data, RAT_END );
			ofxAmount amount = ofa_box_get_amount( iv->data, RAT_RATE );
			gchar *sdbegin = my_date_to_str( dbegin, MY_DATE_DMYY );
			gchar *sdend = my_date_to_str( dend, MY_DATE_DMYY );
			g_debug( "rate_load_dataset: mnemo=%s, begin=%s, end=%s, amount=%.5lf",
					mnemo, sdbegin, sdend, amount );
			g_free( sdbegin );
			g_free( sdend );
		}*/
	}

	return( dataset );
}

/**
 * ofo_rate_get_by_mnemo:
 *
 * Returns: the searched rate, or %NULL.
 *
 * The returned object is owned by the #ofoRate class, and should
 * not be unreffed by the caller.
 */
ofoRate *
ofo_rate_get_by_mnemo( ofoDossier *dossier, const gchar *mnemo )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( mnemo && g_utf8_strlen( mnemo, -1 ), NULL );

	OFA_IDATASET_GET( dossier, RATE, rate );

	return( rate_find_by_mnemo( rate_dataset, mnemo ));
}

static ofoRate *
rate_find_by_mnemo( GList *set, const gchar *mnemo )
{
	GList *found;

	found = g_list_find_custom(
				set, mnemo, ( GCompareFunc ) rate_cmp_by_mnemo );
	if( found ){
		return( OFO_RATE( found->data ));
	}

	return( NULL );
}

/**
 * ofo_rate_new:
 */
ofoRate *
ofo_rate_new( void )
{
	ofoRate *rate;

	rate = g_object_new( OFO_TYPE_RATE, NULL );
	OFO_BASE( rate )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( rate );
}

/**
 * ofo_rate_get_mnemo:
 */
const gchar *
ofo_rate_get_mnemo( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_MNEMO );
}

/**
 * ofo_rate_get_label:
 */
const gchar *
ofo_rate_get_label( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_LABEL );
}

/**
 * ofo_rate_get_notes:
 */
const gchar *
ofo_rate_get_notes( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_NOTES );
}

/**
 * ofo_rate_get_upd_user:
 */
const gchar *
ofo_rate_get_upd_user( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, string, NULL, RAT_UPD_USER );
}

/**
 * ofo_rate_get_upd_stamp:
 */
const GTimeVal *
ofo_rate_get_upd_stamp( const ofoRate *rate )
{
	ofo_base_getter( RATE, rate, timestamp, NULL, RAT_UPD_STAMP );
}

/**
 * ofo_rate_get_min_valid:
 *
 * Returns the smallest beginning date, all validities included.
 * If the returned #GDate struct is invalid, then it must be considered
 * as infinite in the past.
 */
const GDate *
ofo_rate_get_min_valid( const ofoRate *rate )
{
	GList *it;
	const GDate *val_begin, *min;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		for( min=NULL, it=rate->priv->validities ; it ; it=it->next ){
			val_begin = ofa_box_get_date( it->data, RAT_BEGIN );
			if( !min ){
				min = val_begin;
			} else if( my_date_compare_ex( val_begin, min, TRUE ) < 0 ){
				min = val_begin;
			}
		}

		return( min );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_max_valid:
 *
 * Returns the greatest ending date, all validities included.
 * The returned #GDate object may be %NULL or invalid if it is
 * infinite in the future.
 */
const GDate *
ofo_rate_get_max_valid( const ofoRate *rate )
{
	GList *it;
	const GDate *val_end, *max;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		for( max=NULL, it=rate->priv->validities ; it ; it=it->next ){
			val_end = ofa_box_get_date( it->data, RAT_END );
			if( !max ){
				max = val_end;
			} else if( my_date_compare_ex( val_end, max, FALSE ) > 0 ){
				max = val_end;
			}
		}

		return( max );
	}

	g_assert_not_reached();
	return( NULL );
}

/**
 * ofo_rate_get_val_count:
 *
 * Returns: the count of validity rows for this @rate
 */
gint
ofo_rate_get_val_count( const ofoRate *rate )
{
	g_return_val_if_fail( OFO_IS_RATE( rate ), 0 );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return( g_list_length( rate->priv->validities ));
	}

	return( 0 );
}

/**
 * ofo_rate_get_val_begin:
 */
const GDate *
ofo_rate_get_val_begin( const ofoRate *rate, gint idx )
{
	GList *nth;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		nth = g_list_nth( rate->priv->validities, idx );
		if( nth ){
			return( ofa_box_get_date( nth->data, RAT_BEGIN ));
		}
	}

	return( NULL );
}

/**
 * ofo_rate_get_val_end:
 */
const GDate *
ofo_rate_get_val_end( const ofoRate *rate, gint idx )
{
	GList *nth;

	g_return_val_if_fail( OFO_IS_RATE( rate ), NULL );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		nth = g_list_nth( rate->priv->validities, idx );
		if( nth ){
			return( ofa_box_get_date( nth->data, RAT_END ));
		}
	}

	return( NULL );
}

/**
 * ofo_rate_get_val_rate:
 */
ofxAmount
ofo_rate_get_val_rate( const ofoRate *rate, gint idx )
{
	GList *nth;

	g_return_val_if_fail( OFO_IS_RATE( rate ), 0 );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		nth = g_list_nth( rate->priv->validities, idx );
		if( nth ){
			return( ofa_box_get_amount( nth->data, RAT_RATE ));
		}
	}

	return( 0 );
}

/**
 * ofo_rate_get_rate_at_date:
 * @rate: the #ofoRate object.
 * @date: the date at which we are searching the rate value; this must
 *  be a valid date.
 *
 * Returns the value of the rate at the given date, or zero.
 */
ofxAmount
ofo_rate_get_rate_at_date( const ofoRate *rate, const GDate *date )
{
	GList *it;
	const GDate *val_begin, *val_end;
	ofxAmount amount;

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), 0 );
	g_return_val_if_fail( date, 0 );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		for( it=rate->priv->validities ; it ; it=it->next ){
			val_begin = ofa_box_get_date( it->data, RAT_BEGIN );
			val_end = ofa_box_get_date( it->data, RAT_END );
			if( my_date_compare_ex( val_begin, date, TRUE ) > 0 ){
				continue;
			}
			if( my_date_compare_ex( val_end, date, FALSE ) >= 0 ){
				amount = ofa_box_get_amount( it->data, RAT_RATE );
				return( amount );
			}
		}
	}

	return( 0 );
}

/**
 * ofo_rate_is_deletable:
 *
 * A rate cannot be deleted if it is referenced in the debit or the
 * credit formulas of a model detail line.
 */
gboolean
ofo_rate_is_deletable( const ofoRate *rate, ofoDossier *dossier )
{
	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		return( !ofo_ope_template_use_rate( dossier, ofo_rate_get_mnemo( rate )));
	}

	g_assert_not_reached();
	return( FALSE );
}

/**
 * ofo_rate_is_valid:
 *
 * Note that we only check for the intrinsec validity of the provided
 * data. This does NOT check for an possible duplicate mnemo or so.
 *
 * In order to check that all provided periods of validity are
 * consistent between each others, we are trying to sort them from the
 * infinite past to the infinite future - if this doesn't work
 * (probably because overlapping each others), then the provided data
 * is considered as not valid
 */
gboolean
ofo_rate_is_valid( const gchar *mnemo, const gchar *label, GList *validities )
{
	gboolean ok;
	gboolean consistent;

	ok = mnemo && g_utf8_strlen( mnemo, -1 ) &&
			label && g_utf8_strlen( label, -1 );

	consistent = TRUE;
	validities = g_list_sort_with_data(
			validities, ( GCompareDataFunc ) rate_cmp_by_validity, &consistent );
	ok &= consistent;

	return( ok );
}

/**
 * ofo_rate_set_mnemo:
 */
void
ofo_rate_set_mnemo( ofoRate *rate, const gchar *mnemo )
{
	ofo_base_setter( RATE, rate, string, RAT_MNEMO, mnemo );
}

/**
 * ofo_rate_set_label:
 */
void
ofo_rate_set_label( ofoRate *rate, const gchar *label )
{
	ofo_base_setter( RATE, rate, string, RAT_LABEL, label );
}

/**
 * ofo_rate_set_notes:
 */
void
ofo_rate_set_notes( ofoRate *rate, const gchar *notes )
{
	ofo_base_setter( RATE, rate, string, RAT_NOTES, notes );
}

/*
 * ofo_rate_set_upd_user:
 */
static void
rate_set_upd_user( ofoRate *rate, const gchar *upd_user )
{
	ofo_base_setter( RATE, rate, string, RAT_UPD_USER, upd_user );
}

/*
 * ofo_rate_set_upd_stamp:
 */
static void
rate_set_upd_stamp( ofoRate *rate, const GTimeVal *upd_stamp )
{
	ofo_base_setter( RATE, rate, timestamp, RAT_UPD_STAMP, upd_stamp );
}

/**
 * ofo_rate_free_all_val:
 *
 * Clear all validities of the rate object.
 * This is normally done just before adding new validities, when
 * preparing for a dbms update.
 */
void
ofo_rate_free_all_val( ofoRate *rate )
{

	g_return_if_fail( rate && OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		rate_free_validities( rate );
	}
}

/**
 * ofo_rate_add_val:
 * @rate: the #ofoRate to which the new validity must be added
 * @begin: the beginning of the validity, or %NULL
 * @end: the end of the validity, or %NULL
 * @value: the value of the rate for this validity.
 *
 * Add a validity record to the rate.
 */
void
ofo_rate_add_val( ofoRate *rate, const GDate *begin, const GDate *end, ofxAmount value )
{
	GList *fields;

	g_return_if_fail( rate && OFO_IS_RATE( rate ));

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		fields = rate_val_new_detail( ofo_rate_get_mnemo( rate ), begin, end, value );
		rate_val_add_detail( rate, fields );
	}
}

static GList *
rate_val_new_detail( const gchar *mnemo, const GDate *begin, const GDate *end, ofxAmount value )
{
	GList *fields;

	fields = ofa_box_init_fields_list( st_validities_defs );
	ofa_box_set_string( fields, RAT_MNEMO, mnemo );
	ofa_box_set_date( fields, RAT_BEGIN, begin );
	ofa_box_set_date( fields, RAT_END, end );
	ofa_box_set_amount( fields, RAT_RATE, value );

	return( fields );
}

static void
rate_val_add_detail( ofoRate *rate, GList *detail )
{
	rate->priv->validities = g_list_append( rate->priv->validities, detail );
}

/**
 * ofo_rate_insert:
 *
 * First creation of a new rate. This may contain zéro to n validity
 * datail rows. But, if it doesn't, then we take care of removing all
 * previously existing old validity rows.
 */
gboolean
ofo_rate_insert( ofoRate *rate, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_rate_insert";

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_debug( "%s: rate=%p, dossier=%p",
				thisfn, ( void * ) rate, ( void * ) dossier );

		if( rate_do_insert(
					rate,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_ADD( dossier, RATE, rate );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
rate_do_insert( ofoRate *rate, const ofaDbms *dbms, const gchar *user )
{
	return( rate_insert_main( rate, dbms, user ) &&
			rate_delete_validities( rate, dbms ) &&
			rate_insert_validities( rate, dbms ));
}

static gboolean
rate_insert_main( ofoRate *rate, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( OFA_IS_DBMS( dbms ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_rate_get_label( rate ));
	notes = my_utils_quote( ofo_rate_get_notes( rate ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO OFA_T_RATES" );

	g_string_append_printf( query,
			"	(RAT_MNEMO,RAT_LABEL,RAT_NOTES,"
			"	RAT_UPD_USER, RAT_UPD_STAMP) VALUES ('%s','%s',",
			ofo_rate_get_mnemo( rate ),
			label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "'%s',", notes );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "'%s','%s')", user, stamp_str );

	if( ofa_dbms_query( dbms, query->str, TRUE )){

		rate_set_upd_user( rate, user );
		rate_set_upd_stamp( rate, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

static gboolean
rate_delete_validities( ofoRate *rate, const ofaDbms *dbms )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
rate_insert_validities( ofoRate *rate, const ofaDbms *dbms )
{
	gboolean ok;
	GList *it;

	ok = TRUE;
	for( it=rate->priv->validities ; it ; it=it->next ){
		ok &= rate_insert_validity( rate, it->data, dbms );
	}

	return( ok );
}

static gboolean
rate_insert_validity( ofoRate *rate, GList *fields, const ofaDbms *dbms )
{
	gboolean ok;
	GString *query;
	const GDate *dbegin, *dend;
	ofxAmount amount;
	gchar *sdbegin, *sdend, *samount;

	dbegin = ofa_box_get_date( fields, RAT_BEGIN );
	sdbegin = my_date_to_str( dbegin, MY_DATE_SQL );

	dend = ofa_box_get_date( fields, RAT_END );
	sdend = my_date_to_str( dend, MY_DATE_SQL );

	amount = ofa_box_get_amount( fields, RAT_RATE );
	samount = my_double_to_sql( amount );

	query = g_string_new( "INSERT INTO OFA_T_RATES_VAL " );

	g_string_append_printf( query,
			"	(RAT_MNEMO,"
			"	RAT_VAL_BEG,RAT_VAL_END,RAT_VAL_RATE) "
			"	VALUES ('%s',",
					ofo_rate_get_mnemo( rate ));

	if( sdbegin && g_utf8_strlen( sdbegin, -1 )){
		g_string_append_printf( query, "'%s',", sdbegin );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( sdend && g_utf8_strlen( sdend, -1 )){
		g_string_append_printf( query, "'%s',", sdend );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query, "%s)", samount );

	ok = ofa_dbms_query( dbms, query->str, TRUE );

	g_string_free( query, TRUE );
	g_free( sdbegin );
	g_free( sdend );
	g_free( samount );

	return( ok );
}

/**
 * ofo_rate_update:
 *
 * Only update here the main properties.
 */
gboolean
ofo_rate_update( ofoRate *rate, ofoDossier *dossier, const gchar *prev_mnemo )
{
	static const gchar *thisfn = "ofo_rate_update";

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( prev_mnemo && g_utf8_strlen( prev_mnemo, -1 ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_debug( "%s: rate=%p, dossier=%p, prev_mnemo=%s",
				thisfn, ( void * ) rate, ( void * ) dossier, prev_mnemo );

		if( rate_do_update(
					rate,
					prev_mnemo,
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ))){

			OFA_IDATASET_UPDATE( dossier, RATE, rate, prev_mnemo );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
rate_do_update( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user )
{
	return( rate_update_main( rate, prev_mnemo, dbms, user ) &&
			rate_delete_validities( rate, dbms ) &&
			rate_insert_validities( rate, dbms ));
}

static gboolean
rate_update_main( ofoRate *rate, const gchar *prev_mnemo, const ofaDbms *dbms, const gchar *user )
{
	GString *query;
	gchar *label, *notes;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;

	g_return_val_if_fail( OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( OFA_IS_DBMS( dbms ), FALSE );

	ok = FALSE;
	label = my_utils_quote( ofo_rate_get_label( rate ));
	notes = my_utils_quote( ofo_rate_get_notes( rate ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "UPDATE OFA_T_RATES SET " );

	g_string_append_printf( query, "RAT_MNEMO='%s',", ofo_rate_get_mnemo( rate ));
	g_string_append_printf( query, "RAT_LABEL='%s',", label );

	if( notes && g_utf8_strlen( notes, -1 )){
		g_string_append_printf( query, "RAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "RAT_NOTES=NULL," );
	}

	g_string_append_printf( query,
			"	RAT_UPD_USER='%s',RAT_UPD_STAMP='%s'"
			"	WHERE RAT_MNEMO='%s'",
					user, stamp_str, prev_mnemo );

	if( ofa_dbms_query( dbms, query->str, TRUE )){

		rate_set_upd_user( rate, user );
		rate_set_upd_stamp( rate, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( notes );
	g_free( label );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_rate_delete:
 */
gboolean
ofo_rate_delete( ofoRate *rate, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_rate_delete";

	g_return_val_if_fail( rate && OFO_IS_RATE( rate ), FALSE );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );
	g_return_val_if_fail( ofo_rate_is_deletable( rate, dossier ), FALSE );

	if( !OFO_BASE( rate )->prot->dispose_has_run ){

		g_debug( "%s: rate=%p, dossier=%p",
				thisfn, ( void * ) rate, ( void * ) dossier );

		if( rate_do_delete(
					rate,
					ofo_dossier_get_dbms( dossier ))){

			OFA_IDATASET_REMOVE( dossier, RATE, rate );

			return( TRUE );
		}
	}

	return( FALSE );
}

static gboolean
rate_do_delete( ofoRate *rate, const ofaDbms *dbms )
{
	gboolean ok;
	gchar *query;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok = ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	query = g_strdup_printf(
			"DELETE FROM OFA_T_RATES_VAL WHERE RAT_MNEMO='%s'",
					ofo_rate_get_mnemo( rate ));

	ok &= ofa_dbms_query( dbms, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
rate_cmp_by_mnemo( const ofoRate *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_rate_get_mnemo( a ), mnemo ));
}

static gint
rate_cmp_by_ptr( const ofoRate *a, const ofoRate *b )
{
	return( rate_cmp_by_mnemo( a, ofo_rate_get_mnemo( b )));
}

/*
 * sorting two period of validities
 * setting consistent to %FALSE if the two overlaps each other
 *
 * A period "a" is said lesser than a period "b" if "a" begins before "b".
 * If "a" and "b" begins on the same date, (this is an inconsistent
 * case), then "a" is said lesser than "b" if "a" ends before "b".
 * If "a" and "b" ends on the same date, then periods are said equal.
 *
 *                     +----------------------------------------------+
 *                     |                    "b"                       |
 *                     |         begin                  end           |
 *                     |    set      invalid      set      invalid    |
 * +-------------------+-----------+---------+-----------+------------+
 * | "a" begin set     |   bs-bs   |  bs-bi  |           |            |
 * | "a" begin invalid |   bi-bs   |  bi-bi  |           |            |
 * | "a" end set       |           |         |  es-es    |   es-ei    |
 * | "a" end invalid   |           |         |  ei-es    |   ei-ei    |
 * +-----+-------------+-----------+---------+-----------+------------+
 */
static gint
rate_cmp_by_validity( const ofsRateValidity *a, const ofsRateValidity *b, gboolean *consistent )
{
	/* does 'a' start from the infinite ? */
	if( !my_date_is_valid( &a->begin )){
		/* 'a' starts from the infinite */
		if( !my_date_is_valid( &b->begin )){
			/* 'bi-bi' case
			 * the two dates start from the infinite: this is not
			 * consistent - compare the end dates */
			if( consistent ){
				*consistent = FALSE;
			}
			if( !my_date_is_valid( &a->end )){
				return( my_date_is_valid( &b->end ) ? -1 : 0 );

			} else if( !my_date_is_valid( &b->end )){
				return( 1 );
			} else {
				return( my_date_compare_ex( &a->end, &b->end, FALSE ));
			}
		}
		/* 'bi-bs' case
		 *  'a' starts from the infinite while 'b-begin' is set
		 * for this be consistant, a must ends before b starts
		 * whatever be the case, 'a' is said lesser than 'b' */
		if( !my_date_is_valid( &a->end ) || my_date_compare_ex( &a->end, &b->begin, TRUE ) >= 0 ){
			if( consistent ){
				*consistent = FALSE;
			}
		}
		return( -1 );
	}

	/* a starts from a fixed date */
	if( !my_date_is_valid( &b->begin )){
		/* 'bs-bi' case
		 * 'b' is said lesser than 'a'
		 * for this be consistent, 'b' must ends before 'a' starts */
		if( !my_date_is_valid( &b->end ) || my_date_compare_ex( &b->end, &a->begin, TRUE ) >= 0 ){
			if( consistent ){
				*consistent = FALSE;
			}
		}
		return( 1 );
	}

	/* 'bs-bs' case
	 * 'a' and 'b' both starts from a set date: b must ends before 'a' starts */
	if( !my_date_is_valid( &b->end ) || my_date_compare_ex( &b->end, &a->begin, TRUE ) >= 0 ){
		if( consistent ){
			*consistent = FALSE;
		}
	}

	g_return_val_if_fail( my_date_is_valid( &a->begin ), 0 );
	g_return_val_if_fail( my_date_is_valid( &a->end ), 0 );

	return( my_date_compare( &a->begin, &a->end ));
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_rate_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofoDossier *dossier )
{
	GList *it, *det;
	GSList *lines;
	ofoRate *rate;
	gchar *str;
	gboolean ok, with_headers;
	gulong count;
	gchar field_sep, decimal_sep;

	OFA_IDATASET_GET( dossier, RATE, rate );

	with_headers = ofa_file_format_has_headers( settings );
	field_sep = ofa_file_format_get_field_sep( settings );
	decimal_sep = ofa_file_format_get_decimal_sep( settings );

	count = ( gulong ) g_list_length( rate_dataset );
	if( with_headers ){
		count += 2;
	}
	for( it=rate_dataset ; it ; it=it->next ){
		rate = OFO_RATE( it->data );
		count += g_list_length( rate->priv->validities );
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = ofa_box_get_csv_header( st_boxed_defs, field_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		str = ofa_box_get_csv_header( st_validities_defs, field_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	for( it=rate_dataset ; it ; it=it->next ){
		str = ofa_box_get_csv_line( OFO_BASE( it->data )->prot->fields, field_sep, decimal_sep );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}

		rate = OFO_RATE( it->data );
		for( det=rate->priv->validities ; det ; det=det->next ){
			str = ofa_box_get_csv_line( det->data, field_sep, decimal_sep );
			lines = g_slist_prepend( NULL, str );
			ok = ofa_iexportable_export_lines( exportable, lines );
			g_slist_free_full( lines, ( GDestroyNotify ) g_free );
			if( !ok ){
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_class_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( const ofaIImportable *instance )
{
	return( 1 );
}

/*
 * ofo_rate_import_csv:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 * Fields must be:
 * - 1:
 * - rate mnemo
 * - label
 * - notes (opt)
 *
 * - 2:
 * - rate mnemo
 * - begin validity (opt - yyyy-mm-dd)
 * - end validity (opt - yyyy-mm-dd)
 * - rate
 *
 * It is not required that the input csv files be sorted by mnemo. We
 * may have all 'rate' records, then all 'validity' records...
 *
 * Replace the whole table with the provided datas.
 */
static gint
iimportable_import( ofaIImportable *importable, GSList *lines, ofoDossier *dossier )
{
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	ofoRate *rate;
	GList *dataset, *it;
	guint errors, line;
	gchar *msg, *mnemo;
	gint type;
	GList *detail;

	line = 0;
	errors = 0;
	dataset = NULL;

	for( itl=lines ; itl ; itl=itl->next ){

		line += 1;
		fields = ( GSList * ) itl->data;
		ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_IMPORT, 1 );

		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		type = atoi( cstr );
		switch( type ){
			case 1:
				rate = rate_import_csv_rate( importable, fields, line, &errors );
				if( rate ){
					dataset = g_list_prepend( dataset, rate );
				}
				break;
			case 2:
				mnemo = NULL;
				detail = rate_import_csv_validity( importable, fields, line, &errors, &mnemo );
				if( detail ){
					rate = rate_find_by_mnemo( dataset, mnemo );
					if( rate ){
						rate_val_add_detail( rate, detail );
					}
					g_free( mnemo );
				}
				break;
			default:
				msg = g_strdup_printf( _( "invalid rate line type: %s" ), cstr );
				ofa_iimportable_set_message(
						importable, line, IMPORTABLE_MSG_ERROR, msg );
				g_free( msg );
				errors += 1;
				continue;
		}
	}

	if( !errors ){
		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_RATE, FALSE );

		rate_do_drop_content( ofo_dossier_get_dbms( dossier ));

		for( it=dataset ; it ; it=it->next ){
			rate_do_insert(
					OFO_RATE( it->data ),
					ofo_dossier_get_dbms( dossier ),
					ofo_dossier_get_user( dossier ));

			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
		}

		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
		ofa_idataset_free_dataset( dossier, OFO_TYPE_RATE );

		g_signal_emit_by_name(
				G_OBJECT( dossier ), SIGNAL_DOSSIER_RELOAD_DATASET, OFO_TYPE_RATE );

		ofa_idataset_set_signal_new_allowed( dossier, OFO_TYPE_RATE, TRUE );
	}

	return( errors );
}

static ofoRate *
rate_import_csv_rate( ofaIImportable *importable, GSList *fields, guint line, guint *errors )
{
	ofoRate *rate;
	const gchar *cstr;
	GSList *itf;
	gchar *splitted;

	rate = ofo_rate_new();
	itf = fields;

	/* rate mnemo */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty rate mnemonic" ));
		*errors += 1;
		g_object_unref( rate );
		return( NULL );
	}
	ofo_rate_set_mnemo( rate, cstr );

	/* rate label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty rate label" ));
		*errors += 1;
		g_object_unref( rate );
		return( NULL );
	}
	ofo_rate_set_label( rate, cstr );

	/* notes
	 * we are tolerant on the last field... */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	splitted = my_utils_import_multi_lines( cstr );
	ofo_rate_set_notes( rate, splitted );
	g_free( splitted );

	return( rate );
}

static GList *
rate_import_csv_validity( ofaIImportable *importable, GSList *fields, guint line, guint *errors, gchar **mnemo )
{
	GList *detail;
	const gchar *cstr;
	GSList *itf;
	GDate begin, end;
	ofxAmount amount;

	detail = NULL;
	itf = fields;

	/* rate mnemo */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( !cstr || !g_utf8_strlen( cstr, -1 )){
		ofa_iimportable_set_message(
				importable, line, IMPORTABLE_MSG_ERROR, _( "empty rate mnemonic" ));
		*errors += 1;
		g_free( detail );
		return( NULL );
	}
	*mnemo = g_strdup( cstr );

	/* rate begin validity */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	my_date_set_from_sql( &begin, cstr );

	/* rate end validity */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	my_date_set_from_sql( &end, cstr );

	/* rate rate */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	amount = my_double_set_from_sql( cstr );

	detail = rate_val_new_detail( *mnemo, &begin, &end, amount );

	return( detail );
}

static gboolean
rate_do_drop_content( const ofaDbms *dbms )
{
	return( ofa_dbms_query( dbms, "DELETE FROM OFA_T_RATES", TRUE ) &&
			ofa_dbms_query( dbms, "DELETE FROM OFA_T_RATES_VAL", TRUE ));
}
