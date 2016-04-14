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

#include "api/ofa-isingle-keeper.h"

#define ISINGLE_KEEPER_LAST_VERSION           1

#define ISINGLE_KEEPER_DATA                   "ofa-isingle_keeper-data"

/* a data structure attached to the implementor
 * @kepts: a list of kept objects as sKept structures
 */
typedef struct {
	GList *kepts;
}
	sISingleKeeper;

/* the data structure instanciated for each kept GType
 */
typedef struct {
	GType    type;
	GObject *object;
}
	sKept;

static guint st_initializations = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaISingleKeeperInterface *klass );
static void            interface_base_finalize( ofaISingleKeeperInterface *klass );
static sKept          *find_kept_by_type( GList *kepts, GType type );
static sISingleKeeper *get_isingle_keeper_data( const ofaISingleKeeper *instance );
static void            on_instance_finalized( sISingleKeeper *sdata, GObject *finalized_single_keeper );
static void            on_object_finalized( sISingleKeeper *sdata, GObject *finalized_object );
static void            free_kept( sKept *kept );

/**
 * ofa_isingle_keeper_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_isingle_keeper_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_isingle_keeper_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_isingle_keeper_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaISingleKeeperInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaISingleKeeper", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaISingleKeeperInterface *klass )
{
	static const gchar *thisfn = "ofa_isingle_keeper_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaISingleKeeperInterface *klass )
{
	static const gchar *thisfn = "ofa_isingle_keeper_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_isingle_keeper_get_interface_last_version:
 * @instance: this #ofaISingleKeeper instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_isingle_keeper_get_interface_last_version( void )
{
	return( ISINGLE_KEEPER_LAST_VERSION );
}

/**
 * ofa_isingle_keeper_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
ofa_isingle_keeper_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ISINGLE_KEEPER );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaISingleKeeperInterface * ) iface )->get_interface_version ){
		version = (( ofaISingleKeeperInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaISingleKeeper::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_isingle_keeper_get_object:
 * @instance: this #ofaISingleKeeper instance.
 * @type: the desired GType type.
 *
 * Returns: a #GObject of @type, or %NULL.
 *
 * The returned object is owned by the @instance, and should not be
 * released by the caller.
 */
GObject *
ofa_isingle_keeper_get_object( const ofaISingleKeeper *instance, GType type )
{
	sISingleKeeper *sdata;
	sKept *kept;
	GObject *found;

	g_return_val_if_fail( instance && OFA_IS_ISINGLE_KEEPER( instance ), NULL );

	sdata = get_isingle_keeper_data( instance );
	kept = find_kept_by_type( sdata->kepts, type );
	found = kept ? kept->object : NULL;

	return( found );
}

static sKept *
find_kept_by_type( GList *kepts, GType type )
{
	GList *it;
	sKept *kept;

	for( it=kepts ; it ; it=it->next ){
		kept = ( sKept * ) it->data;
		if( kept->type == type ){
			return( kept );
		}
	}

	return( NULL );
}

/**
 * ofa_isingle_keeper_set_object:
 * @instance: this #ofaISingleKeeper instance.
 * @object: [allow-none]: the object to be added.
 *
 * Let the @instance keep the @object.
 */
void
ofa_isingle_keeper_set_object( ofaISingleKeeper *instance, void *object )
{
	sISingleKeeper *sdata;
	sKept *kept;

	g_return_if_fail( instance && OFA_IS_ISINGLE_KEEPER( instance ));
	g_return_if_fail( !object || G_IS_OBJECT( object ));

	sdata = get_isingle_keeper_data( instance );
	kept = find_kept_by_type( sdata->kepts, G_OBJECT_TYPE( object ));

	if( kept ){
		g_clear_object( &kept->object );
		kept->object = object;

	} else {
		kept = g_new0( sKept, 1 );
		kept->type = G_OBJECT_TYPE( object );
		kept->object = object;
		sdata->kepts = g_list_prepend( sdata->kepts, kept );
		g_object_weak_ref( G_OBJECT( object ), ( GWeakNotify ) on_object_finalized, sdata );
	}
}

/**
 * ofa_isingle_keeper_free_all:
 * @instance: this #ofaISingleKeeper instance.
 *
 * Free all the current objects.
 */
void
ofa_isingle_keeper_free_all( ofaISingleKeeper *instance )
{
	sISingleKeeper *sdata;

	g_return_if_fail( instance && OFA_IS_ISINGLE_KEEPER( instance ));

	sdata = get_isingle_keeper_data( instance );
	g_list_free_full( sdata->kepts, ( GDestroyNotify ) free_kept );
	sdata->kepts = NULL;
}

static sISingleKeeper *
get_isingle_keeper_data( const ofaISingleKeeper *instance )
{
	sISingleKeeper *sdata;

	sdata = ( sISingleKeeper * ) g_object_get_data( G_OBJECT( instance ), ISINGLE_KEEPER_DATA );

	if( !sdata ){
		sdata = g_new0( sISingleKeeper, 1 );
		g_object_set_data( G_OBJECT( instance ), ISINGLE_KEEPER_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

static void
on_instance_finalized( sISingleKeeper *sdata, GObject *finalized_single_keeper )
{
	static const gchar *thisfn = "ofa_isinglee_keeper_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_single_keeper=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_single_keeper );

	g_list_free_full( sdata->kepts, ( GDestroyNotify ) free_kept );
	g_free( sdata );
}

static void
on_object_finalized( sISingleKeeper *sdata, GObject *finalized_object )
{
	static const gchar *thisfn = "ofa_isinglee_keeper_on_object_finalized";
	sKept *kept;

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	kept = find_kept_by_type( sdata->kepts, G_OBJECT_TYPE( finalized_object ));
	if( kept ){
		sdata->kepts = g_list_remove( sdata->kepts, kept );
		g_free( kept );
	}
}

static void
free_kept( sKept *kept )
{
	if( G_IS_OBJECT( kept->object )){
		g_object_unref( kept->object );
	}
	//g_clear_object( &kept->object );
	g_free( kept );
}
