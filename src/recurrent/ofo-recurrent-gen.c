/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-date.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "ofo-recurrent-gen.h"

/* priv instance data
 */
enum {
	REC_ID = 1,
	REC_LAST_RUN,
	REC_LAST_NUMSEQ
};

/*
 * MAINTAINER NOTE: the dataset is exported in this same order. So:
 * 1/ put in in an order compatible with import
 * 2/ no more modify it
 * 3/ take attention to be able to support the import of a previously
 *    exported file
 */
static const ofsBoxDef st_boxed_defs[] = {
		{ OFA_BOX_CSV( REC_ID ),
				OFA_TYPE_INTEGER,
				FALSE,					/* importable */
				FALSE },				/* export zero as empty */
		{ OFA_BOX_CSV( REC_LAST_RUN ),
				OFA_TYPE_DATE,
				TRUE,
				FALSE },
		{ OFA_BOX_CSV( REC_LAST_NUMSEQ ),
				OFA_TYPE_COUNTER,
				TRUE,
				FALSE },
		{ 0 }
};

typedef struct {
	void *empty;						/* so that gcc -pedantic is happy */
}
	ofoRecurrentGenPrivate;

static ofoRecurrentGen *get_this( ofaIGetter *getter );
static ofoRecurrentGen *gen_do_read( ofaIGetter *getter );
static ofxCounter       recurrent_gen_get_last_numseq( ofoRecurrentGen *gen );
static void             recurrent_gen_set_last_numseq( ofoRecurrentGen *gen, ofxCounter counter );
static gboolean         gen_do_update( ofoRecurrentGen *gen );

G_DEFINE_TYPE_EXTENDED( ofoRecurrentGen, ofo_recurrent_gen, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoRecurrentGen ))

static void
recurrent_gen_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_recurrent_gen_finalize";

	g_debug( "%s: instance=%p (%s): id=%d",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			ofa_box_get_int( OFO_BASE( instance )->prot->fields, REC_ID ));

	g_return_if_fail( instance && OFO_IS_RECURRENT_GEN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_gen_parent_class )->finalize( instance );
}

static void
recurrent_gen_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_RECURRENT_GEN( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_recurrent_gen_parent_class )->dispose( instance );
}

static void
ofo_recurrent_gen_init( ofoRecurrentGen *self )
{
	static const gchar *thisfn = "ofo_recurrent_gen_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_recurrent_gen_class_init( ofoRecurrentGenClass *klass )
{
	static const gchar *thisfn = "ofo_recurrent_gen_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_gen_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_gen_finalize;
}

static ofoRecurrentGen *
get_this( ofaIGetter *getter )
{
	ofoRecurrentGen *gen;
	myICollector *collector;

	collector = ofa_igetter_get_collector( getter );
	gen = ( ofoRecurrentGen * ) my_icollector_single_get_object( collector, OFO_TYPE_RECURRENT_GEN );

	if( !gen ){
		gen = gen_do_read( getter );
		my_icollector_single_set_object( collector, gen );
	}

	return( gen );
}
static ofoRecurrentGen *
gen_do_read( ofaIGetter *getter )
{
	ofoRecurrentGen *gen;
	gchar *where;
	GList *list;

	gen = NULL;

	where = g_strdup_printf( "REC_T_GEN WHERE REC_ID=%d", RECURRENT_ROW_ID );
	list = ofo_base_load_dataset(
					st_boxed_defs,
					where,
					OFO_TYPE_RECURRENT_GEN,
					getter );
	g_free( where );

	if( g_list_length( list ) > 0 ){
		gen = OFO_RECURRENT_GEN( list->data );
	}
	g_list_free( list );

	return( gen );
}

/**
 * ofo_recurrent_gen_get_last_run_date:
 */
const GDate *
ofo_recurrent_gen_get_last_run_date( ofaIGetter *getter )
{
	ofoRecurrentGen *gen = get_this( getter );
	g_return_val_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ), NULL );

	ofo_base_getter( RECURRENT_GEN, gen, date, NULL, REC_LAST_RUN );
}

/**
 * ofo_recurrent_gen_set_last_run_date:
 */
void
ofo_recurrent_gen_set_last_run_date( ofaIGetter *getter, const GDate *date )
{
	ofoRecurrentGen *gen = get_this( getter );
	g_return_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ));

	ofo_base_setter( RECURRENT_GEN, gen, date, REC_LAST_RUN, date );
	gen_do_update( gen );
}

/**
 * ofo_recurrent_gen_get_next_numseq:
 */
ofxCounter
ofo_recurrent_gen_get_next_numseq( ofaIGetter *getter )
{
	ofxCounter last, next;

	ofoRecurrentGen *gen = get_this( getter );
	g_return_val_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ), 0 );

	last = recurrent_gen_get_last_numseq( gen );
	next = last+1;
	recurrent_gen_set_last_numseq( gen, next );
	gen_do_update( gen );

	return( next );
}

/*
 * ofo_recurrent_gen_get_last_numseq:
 */
static ofxCounter
recurrent_gen_get_last_numseq( ofoRecurrentGen *gen )
{
	g_return_val_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ), 0 );

	ofo_base_getter( RECURRENT_GEN, gen, counter, 0, REC_LAST_NUMSEQ );
}

/*
 * ofo_recurrent_gen_set_next_numseq:
 */
static void
recurrent_gen_set_last_numseq( ofoRecurrentGen *gen, ofxCounter counter )
{
	g_return_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ));

	ofo_base_setter( RECURRENT_GEN, gen, counter, REC_LAST_NUMSEQ, counter );
}

static gboolean
gen_do_update( ofoRecurrentGen *gen )
{
	gboolean ok;
	ofaIGetter *getter;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	const GDate *lastrun;
	gchar *slastrun;
	ofxCounter last_numseq;

	getter = ofo_base_get_getter( OFO_BASE( gen ));
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	query = g_string_new( "UPDATE REC_T_GEN SET " );

	lastrun = ofo_recurrent_gen_get_last_run_date( getter );
	if( my_date_is_valid( lastrun )){
		slastrun = my_date_to_str( lastrun, MY_DATE_SQL );
		g_string_append_printf( query, "REC_LAST_RUN='%s'", slastrun );
		g_free( slastrun );
	} else {
		query = g_string_append( query, "REC_LAST_RUN=NULL" );
	}

	last_numseq = recurrent_gen_get_last_numseq( gen );
	g_string_append_printf( query, ", REC_LAST_NUMSEQ=%ld", last_numseq );

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}
