/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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
	GtkTreeModel *filter_model;
}
	sITVFilterable;

#define ITVFILTERABLE_LAST_VERSION        1
#define ITVFILTERABLE_DATA               "ofa-itvfilterable-data"

static guint st_initializations         = 0;	/* interface initialization count */

static GType           register_type( void );
static void            interface_base_init( ofaITVFilterableInterface *klass );
static void            interface_base_finalize( ofaITVFilterableInterface *klass );
static gboolean        on_filter_model( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaITVFilterable *instance );
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
 * ofa_itvfilterable_set_child_model:
 * @instance: this #ofaITVFilterable instance.
 * @model: the child model.
 *
 * Setup the underlying child model.
 *
 * #ofaITVFilterable @instance takes its own reference on the child @model,
 * which will be release on @instance finalization.
 *
 * Returns: the filter model.
 *
 * The returned reference is owned by the #ofaITVFilterable @instance, and
 * should not be released by the caller.
 */
GtkTreeModel *
ofa_itvfilterable_set_child_model( ofaITVFilterable *instance, GtkTreeModel *model )
{
	sITVFilterable *sdata;

	g_return_val_if_fail( instance && OFA_IS_ITVFILTERABLE( instance ), NULL );
	g_return_val_if_fail( model && GTK_IS_TREE_MODEL( model ), NULL );

	sdata = get_itvfilterable_data( instance );
	sdata->filter_model = gtk_tree_model_filter_new( model, NULL );

	gtk_tree_model_filter_set_visible_func(
			GTK_TREE_MODEL_FILTER( sdata->filter_model ),
			( GtkTreeModelFilterVisibleFunc ) on_filter_model, instance, NULL );

	return( sdata->filter_model );
}

static gboolean
on_filter_model( GtkTreeModel *tmodel, GtkTreeIter *iter, ofaITVFilterable *instance )
{
	if( OFA_ITVFILTERABLE_GET_INTERFACE( instance )->filter_model ){
		return( OFA_ITVFILTERABLE_GET_INTERFACE( instance )->filter_model( instance, tmodel, iter ));
	}

	/* do not display any message if the implementation does not provide
	 * any method; on non-filterable models, this would display too much
	 * messages */

	return( TRUE );
}

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

	g_clear_object( &sdata->filter_model );

	g_free( sdata );
}
