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

#include "ui/ofa-itreeview-column.h"

#define ITREEVIEW_COLUMN_LAST_VERSION  1

typedef struct {
	gint         id;
	const gchar *menu_label;
	gboolean     def_visible;
	const gchar *header_label;
}
	sColumn;

static const sColumn st_columns[] = {
	{ ITVC_ACC_ID,       N_( "Account identifier" ),        TRUE,  N_( "Account" )},
	{ ITVC_CONCIL_DATE,  N_( "Reconciliation date" ),       FALSE, N_( "Rec." )},
	{ ITVC_CONCIL_ID,    N_( "Reconciliation identifier" ), FALSE, N_( "Id." )},
	{ ITVC_CREDIT,       NULL,                              TRUE,  N_( "Credit" )},
	{ ITVC_CUR_ID,       N_( "Currency identifier" ),       TRUE,  N_( "Cur." )},
	{ ITVC_DEBIT,        NULL,                              TRUE,  N_( "Debit" )},
	{ ITVC_DEFFECT,      N_( "Effect date" ),               FALSE, N_( "Effect" )},
	{ ITVC_DOPE,         N_( "Operation date" ),            TRUE,  N_( "Operation" )},
	{ ITVC_ENT_ID,       N_( "Entry number" ),              FALSE, N_( "Number" )},
	{ ITVC_ENT_LABEL,    NULL,                              TRUE,  N_( "Label" )},
	{ ITVC_ENT_REF,      N_( "Piece reference" ),           FALSE, N_( "Ref." )},
	{ ITVC_ENT_STATUS,   N_( "Entry status" ),              FALSE, N_( "St." )},
	{ ITVC_LED_ID,       N_( "Ledger identifier" ),         FALSE, N_( "Ledger" )},
	{ ITVC_OPE_TEMPLATE, N_( "Operation template" ),        FALSE, N_( "Model" )},
	{ ITVC_STLMT_NUMBER, N_( "Settlement number" ),         FALSE, N_( "Stlmt." )},
	{ ITVC_TYPE,         N_( "Type" ),                      FALSE, N_( "Type" )},
	{ -1 }
};

static guint st_initializations         = 0;	/* interface initialization count */

static GType          register_type( void );
static void           interface_base_init( ofaITreeviewColumnInterface *klass );
static void           interface_base_finalize( ofaITreeviewColumnInterface *klass );
static guint          itreeview_column_get_interface_version( const ofaITreeviewColumn *instance );
static guint          storeid2interfaceid( guint id, const ofsTreeviewColumnId *sid );
static const sColumn *get_column_def( guint id );

/**
 * ofa_itreeview_column_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_itreeview_column_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_itreeview_column_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_itreeview_column_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaITreeviewColumnInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaITreeviewColumn", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaITreeviewColumnInterface *klass )
{
	static const gchar *thisfn = "ofa_itreeview_column_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		klass->get_interface_version = itreeview_column_get_interface_version;
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaITreeviewColumnInterface *klass )
{
	static const gchar *thisfn = "ofa_itreeview_column_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

static guint
itreeview_column_get_interface_version( const ofaITreeviewColumn *instance )
{
	return( 1 );
}

/**
 * ofa_itreeview_column_get_interface_last_version:
 * @instance: this #ofaITreeviewColumn instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_itreeview_column_get_interface_last_version( const ofaITreeviewColumn *instance )
{
	return( ITREEVIEW_COLUMN_LAST_VERSION );
}

/**
 * ofa_itreeview_column_get_menu_label:
 * @instance: this #ofaITreeviewColumn instance.
 * @id: the column id.
 * @sid: [allow-none]: an array of #ofsTreeviewColumnId structures.
 *
 * Returns: the localized label to be displayed in a menu for the @id
 * column. This may be %NULL. If not null, the returned string should
 * be g_free() by the caller.
 */
gchar *
ofa_itreeview_column_get_menu_label( const ofaITreeviewColumn *instance, guint id, const ofsTreeviewColumnId *sid )
{
	guint interface_id;
	const sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITREEVIEW_COLUMN( instance ), NULL );

	interface_id = storeid2interfaceid( id, sid );
	if( interface_id < 0 ){
		return( NULL );
	}
	scol = get_column_def( interface_id );
	return( g_strdup( gettext( scol->menu_label )));
}

/**
 * ofa_itreeview_column_get_def_visible:
 * @instance: this #ofaITreeviewColumn instance.
 * @id: the column id.
 * @sid: [allow-none]: an array of #ofsTreeviewColumnId structures.
 *
 * Returns: Whether the specified @id column defaults to be visible.
 * Returns: %TRUE if the column is not defined.
 */
gboolean
ofa_itreeview_column_get_def_visible( const ofaITreeviewColumn *instance, guint id, const ofsTreeviewColumnId *sid )
{
	guint interface_id;
	const sColumn *scol;

	g_return_val_if_fail( instance && OFA_IS_ITREEVIEW_COLUMN( instance ), FALSE );

	interface_id = storeid2interfaceid( id, sid );
	if( interface_id < 0 ){
		return( TRUE );
	}
	scol = get_column_def( interface_id );
	return( scol->def_visible );
}

static guint
storeid2interfaceid( guint id, const ofsTreeviewColumnId *sid )
{
	if( sid ){
		for( gint i=0 ; sid[i].id_store >= 0 ; ++i ){
			if( sid[i].id_store == id ){
				return( sid[i].id_interface );
			}
		}
		return( -1 );
	}
	return( id );
}

static const sColumn *
get_column_def( guint id )
{
	for( gint i=0 ; st_columns[i].id >= 0 ; ++i ){
		if( st_columns[i].id == id ){
			return( &st_columns[i] );
		}
	}
	return( NULL );
}
