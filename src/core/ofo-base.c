/*
 * Open Firm Accounting * A double-entry accounting application for professional services.
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

#include "api/ofa-box.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

/* private instance data
 */
typedef struct {

	/* initialization
	 */
	ofaIGetter *getter;
}
	ofoBasePrivate;

/* class properties
 */
enum {
	PROP_GETTER_ID = 1,
};

G_DEFINE_TYPE_EXTENDED( ofoBase, ofo_base, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofoBase ))

static void
base_finalize( GObject *instance )
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
base_dispose( GObject *instance )
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

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
base_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofoBasePrivate *priv;
	ofoBaseProtected *prot;

	g_return_if_fail( OFO_IS_BASE( instance ));

	priv = ofo_base_get_instance_private( OFO_BASE( instance ));
	prot = OFO_BASE( instance )->prot;

	if( !prot->dispose_has_run ){

		switch( property_id ){
			case PROP_GETTER_ID:
				g_value_set_pointer( value, priv->getter );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * the user asks to set a property and provides it into a GValue
 * read the content of the provided GValue and set our instance datas
 */
static void
base_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofoBasePrivate *priv;
	ofoBaseProtected *prot;

	g_return_if_fail( OFO_IS_BASE( instance ));

	priv = ofo_base_get_instance_private( OFO_BASE( instance ));
	prot = OFO_BASE( instance )->prot;

	if( !prot->dispose_has_run ){

		switch( property_id ){
			case PROP_GETTER_ID:
				priv->getter = g_value_get_pointer( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
ofo_base_init( ofoBase *self )
{
	self->prot = g_new0( ofoBaseProtected, 1 );
	self->prot->dispose_has_run = FALSE;
	self->prot->fields = NULL;
}

static void
ofo_base_class_init( ofoBaseClass *klass )
{
	static const gchar *thisfn = "ofo_base_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = base_get_property;
	G_OBJECT_CLASS( klass )->set_property = base_set_property;
	G_OBJECT_CLASS( klass )->dispose = base_dispose;
	G_OBJECT_CLASS( klass )->finalize = base_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_GETTER_ID,
			g_param_spec_pointer(
					"ofo-base-getter",
					"ofaIGetter instance",
					"ofaIGetter instance",
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));
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
 * @getter: a #ofaIGetter instance.
 *
 * Load the full dataset for the specified @type class.
 *
 * Returns: the ordered list of loaded objects.
 */
GList *
ofo_base_load_dataset( const ofsBoxDef *defs, const gchar *from, GType type, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofo_base_load_dataset";
	const ofaIDBConnect *connect;
	ofoBase *object;
	GList *rows, *dataset, *it;
	ofaHub *hub;

	g_return_val_if_fail( defs, NULL );
	g_return_val_if_fail( type, NULL );
	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	dataset = NULL;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	rows = ofo_base_load_rows( defs, connect, from );

	for( it=rows ; it ; it=it->next ){
		object = g_object_new( type, "ofo-base-getter", getter, NULL );
		object->prot->fields = it->data;
		dataset = g_list_prepend( dataset, object );
	}
	g_list_free( rows );

	g_debug( "%s: type=%s, count=%d", thisfn, g_type_name( type ), g_list_length( dataset ));

	return( g_list_reverse( dataset ));
}

/**
 * ofo_base_load_rows:
 * @defs: the #ofsBoxDefs list of field definitions for this object
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
	columns = ofa_box_dbms_get_columns_list( defs );
	query = g_strdup_printf( "SELECT %s FROM %s", columns, from );
	g_free( columns );

	if( ofa_idbconnect_query_ex( cnx, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			rows = g_list_prepend( rows, ofa_box_dbms_parse_result( defs, irow ));
		}
		ofa_idbconnect_free_results( result );
	}
	g_free( query );

	return( g_list_reverse( rows ));
}

/**
 * ofo_base_get_getter:
 * @base: this #ofoBase object.
 *
 * Returns: the current #ofaHub object attach to @base.
 *
 * The current #ofaHub is expected to be attached to the #ofoBase
 * object when this later is loaded from the database.
 */
ofaIGetter *
ofo_base_get_getter( ofoBase *base )
{
	ofoBasePrivate *priv;

	g_return_val_if_fail( base && OFO_IS_BASE( base ), NULL );
	g_return_val_if_fail( !base->prot->dispose_has_run, NULL );

	priv = ofo_base_get_instance_private( base );

	return( priv->getter );
}
