/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "api/my-utils.h"
#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "ofo-recurrent-run.h"

/* priv instance data
 */
enum {
	REC_MNEMO = 1,
	REC_DATE,
	REC_STATUS,
	REC_UPD_USER,
	REC_UPD_STAMP,
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( REC_MNEMO ),
				OFA_TYPE_STRING,
				TRUE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( REC_DATE ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_STATUS ),
				OFA_TYPE_STRING,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_UPD_USER ),
				OFA_TYPE_STRING,
				FALSE,
				FALSE },
		{ OFA_BOX_CSV( REC_UPD_STAMP ),
				OFA_TYPE_TIMESTAMP,
				FALSE,
				TRUE },
		{ 0 }
};

struct _ofoRecurrentRunPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

static void     recurrent_run_set_upd_user( ofoRecurrentRun *model, const gchar *upd_user );
static void     recurrent_run_set_upd_stamp( ofoRecurrentRun *model, const GTimeVal *upd_stamp );
static gboolean model_do_insert( ofoRecurrentRun *model, const ofaIDBConnect *connect );
static gboolean model_insert_main( ofoRecurrentRun *model, const ofaIDBConnect *connect );
static gint     model_cmp_by_mnemo( const ofoRecurrentRun *a, const gchar *mnemo );
static gint     recurrent_run_cmp_by_ptr( const ofoRecurrentRun *a, const ofoRecurrentRun *b );
static void     icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint    icollectionable_get_interface_version( const ofaICollectionable *instance );
static GList   *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentRun, ofo_recurrent_run, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentRun )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init ))

static void
recurrent_run_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_recurrent_run_finalize";

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_string( OFO_BASE( instance )->prot->fields, REC_MNEMO ));

	g_return_if_fail( instance && OFO_IS_RECURRENT_RUN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_run_parent_class )->finalize( instance );
}

static void
recurrent_run_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_RECURRENT_RUN( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_run_parent_class )->dispose( instance );
}

static void
ofo_recurrent_run_init( ofoRecurrentRun *self )
{
	static const gchar *thisfn = "ofo_recurrent_run_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_recurrent_run_class_init( ofoRecurrentRunClass *klass )
{
	static const gchar *thisfn = "ofo_recurrent_run_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_finalize;
}

/**
 * ofo_recurrent_run_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoRecurrentRun dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_recurrent_run_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_RECURRENT_RUN ));
}

/**
 * ofo_recurrent_run_new:
 */
ofoRecurrentRun *
ofo_recurrent_run_new( void )
{
	ofoRecurrentRun *model;

	model = g_object_new( OFO_TYPE_RECURRENT_RUN, NULL );
	OFO_BASE( model )->prot->fields = ofo_base_init_fields_list( st_boxed_defs );

	return( model );
}

/**
 * ofo_recurrent_run_get_mnemo:
 * @model:
 */
const gchar *
ofo_recurrent_run_get_mnemo( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_MNEMO );
}

/**
 * ofo_recurrent_run_get_date:
 */
const GDate *
ofo_recurrent_run_get_date( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, date, NULL, REC_DATE );
}

/**
 * ofo_recurrent_run_get_status:
 */
const gchar *
ofo_recurrent_run_get_status( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_STATUS );
}

/**
 * ofo_recurrent_run_get_upd_user:
 */
const gchar *
ofo_recurrent_run_get_upd_user( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, string, NULL, REC_UPD_USER );
}

/**
 * ofo_recurrent_run_get_upd_stamp:
 */
const GTimeVal *
ofo_recurrent_run_get_upd_stamp( const ofoRecurrentRun *model )
{
	ofo_base_getter( RECURRENT_RUN, model, timestamp, NULL, REC_UPD_STAMP );
}

/**
 * ofo_recurrent_run_set_mnemo:
 */
void
ofo_recurrent_run_set_mnemo( ofoRecurrentRun *model, const gchar *mnemo )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_MNEMO, mnemo );
}

/**
 * ofo_recurrent_run_set_date:
 */
void
ofo_recurrent_run_set_date( ofoRecurrentRun *model, const GDate *date )
{
	ofo_base_setter( RECURRENT_RUN, model, date, REC_DATE, date );
}

/**
 * ofo_recurrent_run_set_status:
 */
void
ofo_recurrent_run_set_status( ofoRecurrentRun *model, const gchar *status )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_STATUS, status );
}

/*
 * ofo_recurrent_run_set_upd_user:
 */
static void
recurrent_run_set_upd_user( ofoRecurrentRun *model, const gchar *upd_user )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_UPD_USER, upd_user );
}

/*
 * ofo_recurrent_run_set_upd_stamp:
 */
static void
recurrent_run_set_upd_stamp( ofoRecurrentRun *model, const GTimeVal *upd_stamp )
{
	ofo_base_setter( RECURRENT_RUN, model, string, REC_UPD_STAMP, upd_stamp );
}

/**
 * ofo_recurrent_run_insert:
 */
gboolean
ofo_recurrent_run_insert( ofoRecurrentRun *recurrent_run, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_recurrent_run_insert";
	gboolean ok;

	g_debug( "%s: model=%p, hub=%p",
			thisfn, ( void * ) recurrent_run, ( void * ) hub );

	g_return_val_if_fail( recurrent_run && OFO_IS_RECURRENT_RUN( recurrent_run ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( recurrent_run )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	if( model_do_insert( recurrent_run, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( recurrent_run ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( recurrent_run ), ( GCompareFunc ) recurrent_run_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, recurrent_run );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
model_do_insert( ofoRecurrentRun *model, const ofaIDBConnect *connect )
{
	return( model_insert_main( model, connect ));
}

static gboolean
model_insert_main( ofoRecurrentRun *model, const ofaIDBConnect *connect )
{
	gboolean ok;
	GString *query;
	const GDate *date;
	const gchar *status;
	gchar *sdate, *userid, *stamp_str;
	GTimeVal stamp;

	userid = ofa_idbconnect_get_account( connect );
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );

	query = g_string_new( "INSERT INTO REC_T_RUN " );

	g_string_append_printf( query,
			"	(REC_MNEMO,REC_DATE,REC_STATUS,"
			"	 REC_UPD_USER, REC_UPD_STAMP) VALUES ('%s',",
			ofo_recurrent_run_get_mnemo( model ));

	date = ofo_recurrent_run_get_date( model );
	g_return_val_if_fail( my_date_is_valid( date ), FALSE );
	sdate = my_date_to_str( date, MY_DATE_SQL );
	g_string_append_printf( query, "'%s',", sdate );
	g_free( sdate );

	status = ofo_recurrent_run_get_status( model );
	if( my_strlen( status )){
		g_string_append_printf( query, "'%s',", status );
	} else {
		query = g_string_append( query, "NULL," );
	}

	g_string_append_printf( query,
			"'%s','%s')", userid, stamp_str );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	recurrent_run_set_upd_user( model, userid );
	recurrent_run_set_upd_stamp( model, &stamp );

	g_string_free( query, TRUE );
	g_free( stamp_str );
	g_free( userid );

	return( ok );
}

static gint
model_cmp_by_mnemo( const ofoRecurrentRun *a, const gchar *mnemo )
{
	return( g_utf8_collate( ofo_recurrent_run_get_mnemo( a ), mnemo ));
}

static gint
recurrent_run_cmp_by_ptr( const ofoRecurrentRun *a, const ofoRecurrentRun *b )
{
	return( model_cmp_by_mnemo( a, ofo_recurrent_run_get_mnemo( b )));
}

/*
 * ofaICollectionable interface management
 */
static void
icollectionable_iface_init( ofaICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( const ofaICollectionable *instance )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub )
{
	GList *dataset;

	dataset = ofo_base_load_dataset(
					st_boxed_defs,
					"REC_T_RUN ORDER BY REC_MNEMO ASC, REC_DATE ASC",
					OFO_TYPE_RECURRENT_RUN,
					hub );

	return( dataset );
}
