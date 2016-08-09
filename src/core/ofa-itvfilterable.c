/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-itvfilterable.h"

/* data structure associated to the instance
 */
typedef struct {
	GtkTreeModel *store;
	GtkTreeView  *treeview;
}
	sITVFilterable;

#define ITVFILTERABLE_LAST_VERSION        1
#define ITVFILTERABLE_DATA               "ofa-itvfilterable-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaITVFilterableInterface *klass );
static void            interface_base_finalize( ofaITVFilterableInterface *klass );
static void            setup_filter_model( ofaITVFilterable *instance, sITVFilterable *sdata );
static sITVFilterable *get_itvfilterable_data( ofaITVFilterable *instance );
static void            on_instance_finalized( sITVFilterable *sdata, GObject *finalized_instance );

/**
 * ofa_itvfilterable_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itvfilterable_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itvfilterable_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itvfilterable_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaITVFilterableInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITVFilterable", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaITVFilterableInterface *klass )
{
	static const gchar *thisfn = "ofa_itvfilterable_interface_base_init";

	if( st_initializations == 0 ){

		g_debug( "%s: klass%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaITVFilterableInterface *klass )
{
	static const gchar *thisfn = "ofa_itvfilterable_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){

		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_itvfilterable_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itvfilterable_get_interface_last_version( void )
{
	return( ITVFILTERABLE_LAST_VERSION );
}

/**
 * ofa_itvfilterable_get_interface_version:
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
ofa_itvfilterable_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, OFA_TYPE_ITVFILTERABLE );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( ofaITVFilterableInterface * ) iface )->get_interface_version ){
		version = (( ofaITVFilterableInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'ofaITVFilterable::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * ofa_itvfilterable_set_store:
 * @instance: this #ofaIStorable instance.
 * @store: the underlying store.
 *
 * Setup the underlying store.
 *
 * If both treeview and store are set, then they are associated through
 * a sortable model, sort settings are read and a default sort function
 * is set.
 *
 * At that time, the model starts to sort itself. So it is better if all
 * configuration is set before calling this method.
 */
void
ofa_itvfilterable_set_store( ofaITVFilterable *instance, GtkTreeModel *store )
{
	sITVFilterable *sdata;

	g_return_if_fail( instance && OFA_IS_ITVFILTERABLE( instance ));
	g_return_if_fail( store && GTK_IS_TREE_MODEL( store ));

	sdata = get_itvfilterable_data( instance );
	sdata->store = store;

	setup_filter_model( instance, sdata );
}

/**
 * ofa_itvfilterable_set_treeview:
 * @instance: this #ofaIStorable instance.
 * @treeview: the #GtkTreeView widget.
 *
 * Setup the treeview widget.
 */
void
ofa_itvfilterable_set_treeview( ofaITVFilterable *instance, GtkTreeView *treeview )
{
	sITVFilterable *sdata;

	g_return_if_fail( instance && OFA_IS_ITVFILTERABLE( instance ));
	g_return_if_fail( treeview && GTK_IS_TREE_VIEW( treeview ));

	sdata = get_itvfilterable_data( instance );
	sdata->treeview = treeview;

	setup_filter_model( instance, sdata );
}

static void
setup_filter_model( ofaITVFilterable *instance, sITVFilterable *sdata )
{
}

/*
static gboolean
on_filter_model( GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, ofaITVFilterable *instance )
{
	static const gchar *thisfn = "ofa_itvfilterable_on_sort_model";
	sITVFilterable *sdata;

	if( OFA_ITVFILTERABLE_GET_INTERFACE( instance )->filter_model ){
		sdata = get_itvfilterable_data( instance );
		return( OFA_ITVFILTERABLE_GET_INTERFACE( instance )->filter_model( instance, tmodel, a, b, sdata->sort_column_id ));
	}

	g_info( "%s: ofaITVFilterable's %s implementation does not provide 'sort_model()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( 0 );
}
*/

static sITVFilterable *
get_itvfilterable_data( ofaITVFilterable *instance )
{
	sITVFilterable *sdata;

	sdata = ( sITVFilterable * ) g_object_get_data( G_OBJECT( instance ), ITVFILTERABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sITVFilterable, 1 );
		g_object_set_data( G_OBJECT( instance ), ITVFILTERABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_instance_finalized, sdata );
	}

	return( sdata );
}

/*
 * ofaITVFilterable instance finalization
 */
static void
on_instance_finalized( sITVFilterable *sdata, GObject *finalized_instance )
{
	static const gchar *thisfn = "ofa_itvfilterable_on_instance_finalized";

	g_debug( "%s: sdata=%p, finalized_instance=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_instance );

	g_free( sdata );
}
