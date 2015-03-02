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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "api/ofa-idataset.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"

/* this structure is held by the dossier in a GList
 */
typedef struct {
	GType    type;
	GList   *dataset;
	gboolean send_signal_new;
}
	sIDataset;

#define IDATASET_LAST_VERSION           1
#define IDATASET_DATA                   "ofa-idataset-data"

static guint st_initializations = 0;	/* interface initialization count */

static GType      register_type( void );
static void       interface_base_init( ofaIDatasetInterface *klass );
static void       interface_base_finalize( ofaIDatasetInterface *klass );
static sIDataset *get_idataset_from_type( ofaIDataset *instance, GType type );

/**
 * ofa_idataset_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_idataset_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_idataset_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_idataset_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIDatasetInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIDataset", &info, 0 );

	g_type_interface_add_prerequisite( type, OFO_TYPE_BASE );

	return( type );
}

static void
interface_base_init( ofaIDatasetInterface *klass )
{
	static const gchar *thisfn = "ofa_idataset_interface_base_init";

	if( !st_initializations ){

		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));
	}

	st_initializations += 1;

	if( st_initializations == 1 ){

		/* declare here the default implementations */
	}
}

static void
interface_base_finalize( ofaIDatasetInterface *klass )
{
	static const gchar *thisfn = "ofa_idataset_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_idataset_get_interface_last_version:
 * @instance: this #ofaIDataset instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_idataset_get_interface_last_version( void )
{
	return( IDATASET_LAST_VERSION );
}

/**
 * ofa_idataset_get_dataset:
 * @dossier:
 * @type:
 *
 * Returns: the currently set dataset, or %NULL.
 *
 * The returned list is owned by the #ofoDossier dossier, and should
 * not be freed by the caller.
 */
GList *
ofa_idataset_get_dataset( ofoDossier *dossier, GType type )
{
	sIDataset *sdata;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	g_return_val_if_fail( OFA_IS_IDATASET( dossier ), NULL );

	sdata = get_idataset_from_type( OFA_IDATASET( dossier ), type );
	return( sdata->dataset );
}

/**
 * ofa_idataset_free_dataset:
 * @dossier:
 * @type:
 *
 * Free the previously loaded dataset.
 *
 * Note that this only frees the dataset itself, and not the structure
 * which holds the pointer to it, and is maintains by the dossier.
 */
void
ofa_idataset_free_dataset( ofoDossier *dossier, GType type )
{
	sIDataset *sdata;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( OFA_IS_IDATASET( dossier ));

	sdata = get_idataset_from_type( OFA_IDATASET( dossier ), type );
	g_list_free_full( sdata->dataset, ( GDestroyNotify ) g_object_unref );
	sdata->dataset = NULL;
}

/**
 * ofa_idataset_set_dataset:
 * @dossier:
 * @type:
 * @dataset:
 *
 * Records the @dataset dataset of records as part of this #ofoDossier
 * @dossier dossier, taking care of freeing a previously set dataset.
 */
void
ofa_idataset_set_dataset( ofoDossier *dossier, GType type, GList *dataset )
{
	sIDataset *sdata;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( OFA_IS_IDATASET( dossier ));

	sdata = get_idataset_from_type( OFA_IDATASET( dossier ), type );
	sdata->dataset = dataset;
}

static sIDataset *
get_idataset_from_type( ofaIDataset *instance, GType type )
{
	GList *datasets, *it;
	sIDataset *sdata;

	datasets = NULL;
	if( OFA_IDATASET_GET_INTERFACE( instance )->get_datasets ){
		datasets = OFA_IDATASET_GET_INTERFACE( instance )->get_datasets( instance );
	}

	for( it=datasets ; it ; it=it->next ){
		sdata = ( sIDataset * ) it->data;
		if( sdata->type == type ){
			return( sdata );
		}
	}

	sdata = g_new0( sIDataset, 1 );
	sdata->type = type;
	sdata->send_signal_new = TRUE;
	sdata->dataset = NULL;

	datasets = g_list_prepend( datasets, sdata );

	if( OFA_IDATASET_GET_INTERFACE( instance )->set_datasets ){
		OFA_IDATASET_GET_INTERFACE( instance )->set_datasets( instance, datasets );
	}

	return( sdata );
}

/**
 * ofa_idataset_is_signal_new_allowed:
 * @dossier:
 * @type:
 */
gboolean
ofa_idataset_is_signal_new_allowed( ofoDossier *dossier, GType type )
{
	sIDataset *sdata;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), TRUE );
	g_return_val_if_fail( OFA_IS_IDATASET( dossier ), TRUE );

	sdata = get_idataset_from_type( OFA_IDATASET( dossier ), type );
	return( sdata->send_signal_new );
}

/**
 * ofa_idataset_set_signal_new_allowed:
 * @dossier:
 * @type:
 */
void
ofa_idataset_set_signal_new_allowed( ofoDossier *dossier, GType type, gboolean allowed )
{
	sIDataset *sdata;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( OFA_IS_IDATASET( dossier ));

	sdata = get_idataset_from_type( OFA_IDATASET( dossier ), type );
	sdata->send_signal_new = allowed;
}

/**
 * ofa_idataset_free_full:
 *
 * This is called by the #ofoDossier dossier at dispose time in order
 * to release all loaded objects and, free the associated structures.
 */
void
ofa_idataset_free_full( void *data )
{
	sIDataset *sdata;

	sdata = ( sIDataset * ) data;
	g_list_free_full( sdata->dataset, ( GDestroyNotify ) g_object_unref );
	g_free( sdata );
}
