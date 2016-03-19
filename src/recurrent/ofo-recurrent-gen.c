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

#include "my/my-date.h"

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

#include "ofo-recurrent-gen.h"

/* priv instance data
 */
enum {
	REC_ID = 1,
	REC_LAST_RUN,
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
		{ 0 }
};

struct _ofoRecurrentGenPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

static ofoRecurrentGen *st_this         = NULL;

static ofoRecurrentGen *get_this( ofaHub *hub );
static ofoRecurrentGen *gen_do_read( ofaHub *hub );
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
get_this( ofaHub *hub )
{
	if( !st_this ){
		st_this = gen_do_read( hub );
	}

	return( st_this );
}
static ofoRecurrentGen *
gen_do_read( ofaHub *hub )
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
					hub );
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
ofo_recurrent_gen_get_last_run_date( ofaHub *hub )
{
	ofoRecurrentGen *gen = get_this( hub );
	g_return_val_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ), NULL );

	ofo_base_getter( RECURRENT_GEN, gen, date, NULL, REC_LAST_RUN );
}

/**
 * ofo_recurrent_gen_set_last_run_date:
 */
void
ofo_recurrent_gen_set_last_run_date( ofaHub *hub, const GDate *date )
{
	ofoRecurrentGen *gen = get_this( hub );
	g_return_if_fail( gen && OFO_IS_RECURRENT_GEN( gen ));

	ofo_base_setter( RECURRENT_GEN, gen, date, REC_LAST_RUN, date );
	gen_do_update( gen );
}

static gboolean
gen_do_update( ofoRecurrentGen *gen )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	const GDate *lastrun;
	gchar *slastrun;

	hub = ofo_base_get_hub( OFO_BASE( gen ));
	connect = ofa_hub_get_connect( hub );

	query = g_string_new( "UPDATE REC_T_GEN SET " );

	lastrun = ofo_recurrent_gen_get_last_run_date( hub );
	if( my_date_is_valid( lastrun )){
		slastrun = my_date_to_str( lastrun, MY_DATE_SQL );
		g_string_append_printf( query, "REC_LAST_RUN='%s'", slastrun );
		g_free( slastrun );
	} else {
		query = g_string_append( query, "REC_LAST_RUN=NULL" );
	}

	ok = ofa_idbconnect_query( connect, query->str, TRUE );

	g_string_free( query, TRUE );

	return( ok );
}
