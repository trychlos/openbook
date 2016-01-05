/*
 * Open Freelance Accounting * A double-entry accounting application for freelances.
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

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

/* private instance data
 */
struct _ofoBasePrivate {
	void *empty;						/* so that gcc -pedantic is empty */
};

G_DEFINE_TYPE( ofoBase, ofo_base, G_TYPE_OBJECT )

static void
ofo_base_finalize( GObject *instance )
{
	ofoBaseProtected *prot;

	/* free data members here */
	prot = OFO_BASE( instance )->prot;

	/* only free ofaBox fields list here so that it is left available
	 * in finalize method of the child classes */
	ofa_box_free_fields_list( prot->fields );
	prot->fields = NULL;

	g_free( prot );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->finalize( instance );
}

static void
ofo_base_dispose( GObject *instance )
{
	ofoBase *self;
	ofoBaseProtected *prot;

	self = OFO_BASE( instance );
	prot = self->prot;

	if( !prot->dispose_has_run ){

		prot->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->dispose( instance );
}

static void
ofo_base_init( ofoBase *self )
{
	self->prot = g_new0( ofoBaseProtected, 1 );
	self->prot->dispose_has_run = FALSE;
	self->prot->fields = NULL;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_BASE, ofoBasePrivate );
}

static void
ofo_base_class_init( ofoBaseClass *klass )
{
	static const gchar *thisfn = "ofo_base_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ofo_base_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_base_finalize;

	g_type_class_add_private( klass, sizeof( ofoBasePrivate ));
}

/**
 * ofo_base_init_fields_list:
 * @defs: the #ofsBoxDefs list of field definitions for this object
 *
 * Returns: the list of data fields described in the definitions.
 */
GList *
ofo_base_init_fields_list( const ofsBoxDef *defs )
{
	g_return_val_if_fail( defs, NULL );

	return( ofa_box_init_fields_list( defs ));
}

/**
 * ofo_base_load_dataset:
 * @defs: the #ofsBoxDefs list of field definitions for this object
 * @from: the 'from' part of the query
 * @type: the #GType of the #ofoBase -derived object to be allocated
 * @hub: the #ofaHub object.
 *
 * Load the full dataset for the specified @type class.
 *
 * Returns: the ordered list of loaded objects.
 */
GList *
ofo_base_load_dataset( const ofsBoxDef *defs, const gchar *from, GType type, ofaHub *hub )
{
	const ofaIDBConnect *connect;
	ofoBase *object;
	GList *rows, *dataset, *it;

	g_return_val_if_fail( defs, NULL );
	g_return_val_if_fail( type, NULL );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	dataset = NULL;
	connect = ofa_hub_get_connect( hub );
	rows = ofo_base_load_rows( defs, connect, from );

	for( it=rows ; it ; it=it->next ){
		object = g_object_new( type, NULL );
		object->prot->hub = hub;
		object->prot->fields = it->data;
		dataset = g_list_prepend( dataset, object );
	}
	g_list_free( rows );

	return( g_list_reverse( dataset ));
}

GList *
ofo_base_load_dataset_from_dossier( const ofsBoxDef *defs, const ofaIDBConnect *connect, const gchar *from, GType type )
{
	ofoBase *object;
	GList *rows, *dataset, *it;

	g_return_val_if_fail( defs, NULL );
	g_return_val_if_fail( type, NULL );

	dataset = NULL;
	rows = ofo_base_load_rows( defs, connect, from );

	for( it=rows ; it ; it=it->next ){
		object = g_object_new( type, NULL );
		object->prot->fields = it->data;
		dataset = g_list_prepend( dataset, object );
	}
	g_list_free( rows );

	return( g_list_reverse( dataset ));
}

/**
 * ofo_base_load_rows:
 * @defs: the #ofsBoxDefs list of field definitions for this object
 * @dossier: the currently opened dossier
 * @cnx: the #ofaIDBConnect connection object
 * @from: the 'from' part of the query
 *
 * This function loads the specified rows.
 *
 * Returns: the list of rows, each element being itself a list of fields.
 */
GList *
ofo_base_load_rows( const ofsBoxDef *defs, const ofaIDBConnect *cnx, const gchar *from )
{
	gchar *columns, *query;
	GSList *result, *irow;
	GList *rows;

	g_return_val_if_fail( defs, NULL );
	g_return_val_if_fail( cnx && OFA_IS_IDBCONNECT( cnx ), NULL );

	rows = NULL;
	columns = ofa_box_get_dbms_columns( defs );
	query = g_strdup_printf( "SELECT %s FROM %s", columns, from );
	g_free( columns );

	if( ofa_idbconnect_query_ex( cnx, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			rows = g_list_prepend( rows, ofa_box_parse_dbms_result( defs, irow ));
		}
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	return( g_list_reverse( rows ));
}

/**
 * ofo_base_get_hub:
 * @base: this #ofoBase object.
 *
 * Returns: the current #ofaHub object attach to @base.
 */
ofaHub *
ofo_base_get_hub( const ofoBase *base )
{
	g_return_val_if_fail( base && OFO_IS_BASE( base ), NULL );

	if( base->prot->dispose_has_run ){
		g_return_val_if_reached( NULL );
	}

	return( base->prot->hub );
}
