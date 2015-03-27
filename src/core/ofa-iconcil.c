/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"

#include "core/ofa-concil-collection.h"
#include "core/ofa-icollector.h"
#include "core/ofa-iconcil.h"

/* this structure is held by the object
 */
typedef struct {
	ofoConcil  *concil;
	gchar      *type;
}
	sIConcil;

#define ICONCIL_LAST_VERSION            1
#define ICONCIL_DATA                    "ofa-iconcil-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType        register_type( void );
static void         interface_base_init( ofaIConcilInterface *klass );
static void         interface_base_finalize( ofaIConcilInterface *klass );
static const gchar *iconcil_get_type( const ofaIConcil *instance );
static ofxCounter   iconcil_get_id( const ofaIConcil *instance );

/*
static sIConcil *get_iconcil_data( ofaIConcil *instance );
static void      on_updated_concil( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaIConcil *instance );
static void      on_deleted_concil( ofoDossier *dossier, ofoBase *object, ofaIConcil *instance );
static void      on_finalized_instance( void *empty, void *finalized_instance );
static void      on_finalized_concil( ofaIConcil *instance, void *finalized_concil );
static void      free_iconcil_data( sIConcil *sdata );
*/

/**
 * ofa_iconcil_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iconcil_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iconcil_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iconcil_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIConcilInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIConcil", &info, 0 );

	g_type_interface_add_prerequisite( type, OFO_TYPE_BASE );

	return( type );
}

static void
interface_base_init( ofaIConcilInterface *klass )
{
	static const gchar *thisfn = "ofa_iconcil_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	if( st_initializations == 0 ){

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIConcilInterface *klass )
{
	static const gchar *thisfn = "ofa_iconcil_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iconcil_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iconcil_get_interface_last_version( void )
{
	return( ICONCIL_LAST_VERSION );
}

/**
 * ofa_iconcil_get_interface_version:
 * @instance: this #ofaIConcil instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_iconcil_get_interface_version( const ofaIConcil *instance )
{
	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), 0 );

	if( OFA_ICONCIL_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_ICONCIL_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	return( 1 );
}

/**
 * ofa_iconcil_get_concil:
 * @instance:
 * @dossier:
 *
 * Returns: the reconciliation group this instance belongs to, or %NULL.
 *
 * The reconciliation group is first searched in the in-memory
 * collection which is attached to the dossier, then only is requested
 * from the database.
 */
ofoConcil *
ofa_iconcil_get_concil( const ofaIConcil *instance, ofoDossier *dossier )
{
	ofoConcil *concil;
	GObject *collection;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
	g_return_val_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ), NULL );

	concil = ofa_concil_collection_get_by_other_id(
					OFA_CONCIL_COLLECTION( collection ),
					iconcil_get_type( instance ),
					iconcil_get_id( instance ),
					dossier );

	return( concil );
}

/**
 * ofa_iconcil_new_concil:
 * @instance:
 * @dossier:
 *
 * Returns: a newly created conciliation group from the @instance.
 */
ofoConcil *
ofa_iconcil_new_concil( ofaIConcil *instance, const GDate *dval, ofoDossier *dossier )
{
	ofoConcil *concil;
	GObject *collection;

	g_return_val_if_fail( instance && OFA_IS_ICONCIL( instance ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	concil = ofo_concil_new();
	ofo_concil_set_dval( concil, dval );
	ofo_concil_insert( concil, dossier );

	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ), dossier );

	collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
	g_return_val_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ), NULL );

	ofa_concil_collection_add( OFA_CONCIL_COLLECTION( collection ), concil );

	return( concil );
}

/**
 * ofa_iconcil_add_to_concil:
 * @instance:
 * @concil:
 * @dossier:
 */
void
ofa_iconcil_add_to_concil( ofaIConcil *instance, ofoConcil *concil, ofoDossier *dossier )
{
	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	ofo_concil_add_id( concil, iconcil_get_type( instance ), iconcil_get_id( instance ), dossier );
}

/**
 * ofa_iconcil_remove_concil:
 * @concil:
 * @dossier:
 */
void
ofa_iconcil_remove_concil( ofoConcil *concil, ofoDossier *dossier )
{
	GObject *collection;

	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( ofo_concil_delete( concil, dossier )){

		collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
		g_return_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ));

		ofa_concil_collection_remove( OFA_CONCIL_COLLECTION( collection ), concil );
	}
}

static const gchar *
iconcil_get_type( const ofaIConcil *instance )
{
	const gchar *type;

	g_return_val_if_fail( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_type, -1 );

	type = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_type( instance );

	return( type );
}

static ofxCounter
iconcil_get_id( const ofaIConcil *instance )
{
	ofxCounter id;

	g_return_val_if_fail( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id, -1 );

	id = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id( instance );

	return( id );
}

#if 0
/**
 * ofa_iconcil_dispose:
 * @instance:
 *
 * Remove the attached #ofoConcil object if any.
 */
void
ofa_iconcil_dispose( ofaIConcil *instance )
{
sIConcil *sdata;

	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));

	sdata = get_iconcil_data( instance );
	if( sdata->concil ){
		g_object_unref( sdata->concil );
	}
}

/**
 * ofa_iconcil_set_concil_group:
 * @instance:
 * @concil_type:
 * @dossier:
 *
 * Read from the database the reconciliation group as a #ofoConcil object,
 * attaching its to @instance.
 */
void
ofa_iconcil_set_concil_group( ofaIConcil *instance, const gchar *concil_type, ofoDossier *dossier )
{
	ofxCounter obj_id;
	ofoConcil *concil;
	sIConcil *sdata;

	g_return_if_fail( instance && OFA_IS_ICONCIL( instance ));

	if( OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id ){
		obj_id = OFA_ICONCIL_GET_INTERFACE( instance )->get_object_id( instance );
		concil = ofo_concil_get_by_other_id( dossier, concil_type, obj_id );
		sdata = get_iconcil_data( instance );
		g_return_if_fail( sdata->concil == NULL );

		if( concil ){
			g_object_weak_ref( G_OBJECT( concil ), ( GWeakNotify ) on_finalized_concil, instance );
			sdata->concil = concil;

		} else {
			g_free( sdata );
			g_object_set_data( G_OBJECT( instance ), ICONCIL_DATA, NULL );
		}
	}
}

static sIConcil *
get_iconcil_data( ofaIConcil *instance )
{
	sIConcil *sdata;

	sdata = ( sIConcil * ) g_object_get_data( G_OBJECT( instance ), ICONCIL_DATA );
	if( !sdata ){
		sdata = g_new0( sIConcil, 1 );
		g_object_set_data( G_OBJECT( instance ), ICONCIL_DATA, sdata );
	}

	return( sdata );
}

/*
 * we are dealing here with signals emitted when updating/deleting a
 * #ofoConcil object, and we are only interested in the particular
 * reconciliation group which this entry belongs to
 */
static void
on_updated_concil( ofoDossier *dossier, ofoBase *object, const gchar *prev_id, ofaIConcil *instance )
{
	static const gchar *thisfn = "ofa_iconcil_on_updated_concil";
	ofoConcil *instance_concil;
	ofxCounter instance_concil_id, obj_id;
	sIConcil *sdata;
	gchar *type;

	g_debug( "%s: dossier=%p, object=%p (%s), prev_id=%s, instance=%p (%s)",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( OFO_IS_CONCIL( object )){
		instance_concil = ofa_iconcil_get_concil( instance );
		instance_concil_id = ofo_concil_get_id( instance_concil );
		obj_id = ofo_concil_get_id( OFO_CONCIL( object ));
		if( instance_concil_id == obj_id ){
			sdata = get_iconcil_data( instance );
			type = g_strdup( sdata->type );
			g_object_unref( sdata->concil );
			ofa_iconcil_set_concil_group( instance, type, dossier );
			g_free( type );
		}
	}
}

static void
on_deleted_concil( ofoDossier *dossier, ofoBase *object, ofaIConcil *instance )
{
	static const gchar *thisfn = "ofa_iconcil_on_deleted_concil";
	ofoConcil *instance_concil;
	ofxCounter instance_concil_id, obj_id;
	sIConcil *sdata;

	g_debug( "%s: dossier=%p, object=%p (%s), instance=%p (%s)",
			thisfn,
			( void * ) dossier,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( OFO_IS_CONCIL( object )){
		instance_concil = ofa_iconcil_get_concil( instance );
		instance_concil_id = ofo_concil_get_id( instance_concil );
		obj_id = ofo_concil_get_id( OFO_CONCIL( object ));
		if( instance_concil_id == obj_id ){
			sdata = get_iconcil_data( instance );
			g_object_unref( sdata->concil );
		}
	}
}

static void
on_finalized_instance( void *empty, void *finalized_instance )
{
	static const gchar *thisfn = "ofa_iconcil_on_finalized_instance";
	sIConcil *sdata;

	g_debug( "%s: empty=%p, finalized_instance=%p",
			thisfn,
			( void * ) empty,
			( void * ) finalized_instance );

	sdata = get_iconcil_data( finalized_instance );

	if( sdata->concil ){
		g_object_unref( sdata->concil );
	}
}

static void
on_finalized_concil( ofaIConcil *instance, void *finalized_concil )
{
	static const gchar *thisfn = "ofa_iconcil_on_finalized_concil";
	sIConcil *sdata;

	g_debug( "%s: instance=%p (%s), finalized_concil=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) finalized_concil );

	sdata = get_iconcil_data( instance );
	free_iconcil_data( sdata );
	g_object_set_data( G_OBJECT( instance ), ICONCIL_DATA, NULL );
}

static void
free_iconcil_data( sIConcil *sdata )
{
	/*
	GList *it;

	if( sdata->dossier &&
			OFO_IS_DOSSIER( sdata->dossier ) &&
			!ofo_dossier_has_dispose_run( sdata->dossier )){
		for( it=sdata->dos_handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( sdata->dossier, ( gulong ) it->data );
		}
	}

	g_list_free( sdata->dos_handlers );
	*/
	g_free( sdata->type );
	g_free( sdata );
}
#endif
