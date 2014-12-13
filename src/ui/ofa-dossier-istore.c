/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ui/ofa-dossier-misc.h"
#include "ui/ofa-dossier-istore.h"

/* data associated to each implementor object
 */
typedef struct {

	/* static data
	 * to be set at initialization time
	 */
	ofaDossierColumns columns;

	/* runtime data
	 */
	GtkListStore      *store;
}
	sIStore;

/* columns ordering in the store
 */
enum {
	COL_DNAME = 0,
	N_COLUMNS
};

#define DOSSIER_ISTORE_LAST_VERSION    1
#define DOSSIER_ISTORE_DATA            "ofa-dossier-istore-data"

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static guint st_initializations         = 0;	/* interface initialization count */

static GType    register_type( void );
static void     interface_base_init( ofaDossierIStoreInterface *klass );
static void     interface_base_finalize( ofaDossierIStoreInterface *klass );
static void     load_dataset( ofaDossierIStore *instance, sIStore *sdata );
static void     insert_row( ofaDossierIStore *instance, sIStore *sdata, const gchar *dname );
static void     set_row( ofaDossierIStore *instance, sIStore *sdata, GtkTreeIter *iter, const gchar *dname );
static void     on_parent_finalized( ofaDossierIStore *instance, gpointer finalized_parent );
static void     on_object_finalized( sIStore *sdata, gpointer finalized_object );
static sIStore *get_istore_data( ofaDossierIStore *instance );

/**
 * ofa_dossier_istore_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_dossier_istore_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_dossier_istore_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_dossier_istore_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaDossierIStoreInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaDossierIStore", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaDossierIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_dossier_istore_interface_base_init";
	GType interface_type = G_TYPE_FROM_INTERFACE( klass );

	g_debug( "%s: klass=%p (%s), st_initializations=%d",
			thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ), st_initializations );

	if( !st_initializations ){
		/* declare interface default methods here */

		/**
		 * ofaDossierIStore::changed:
		 *
		 * This signal is sent by the views when the selection is
		 * changed.
		 *
		 * Arguments is the selected dossier ISO 3A code.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaDossierIStore *store,
		 * 						const gchar     *code,
		 * 						gpointer         user_data );
		 */
		st_signals[ CHANGED ] = g_signal_new_class_handler(
					"changed",
					interface_type,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );

		/**
		 * ofaDossierIStore::activated:
		 *
		 * This signal is sent by the views when the selection is
		 * activated.
		 *
		 * Arguments is the selected dossier ISO 3A code.
		 *
		 * Handler is of type:
		 * void ( *handler )( ofaDossierIStore *store,
		 * 						const gchar     *code,
		 * 						gpointer         user_data );
		 */
		st_signals[ ACTIVATED ] = g_signal_new_class_handler(
					"activated",
					interface_type,
					G_SIGNAL_RUN_LAST,
					NULL,
					NULL,								/* accumulator */
					NULL,								/* accumulator data */
					NULL,
					G_TYPE_NONE,
					1,
					G_TYPE_STRING );
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaDossierIStoreInterface *klass )
{
	static const gchar *thisfn = "ofa_dossier_istore_interface_base_finalize";

	st_initializations -= 1;

	if( !st_initializations ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_dossier_istore_get_interface_last_version:
 * @instance: this #ofaDossierIStore instance.
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_dossier_istore_get_interface_last_version( const ofaDossierIStore *instance )
{
	return( DOSSIER_ISTORE_LAST_VERSION );
}

/**
 * ofa_dossier_istore_attach_to:
 * @instance: this #ofaDossierIStore instance.
 * @parent: the #GtkContainer to which the widget should be attached.
 *
 * Attach the widget to its parent.
 *
 * A weak notify reference is put on the parent, so that the GObject
 * will be unreffed when the parent will be destroyed.
 */
void
ofa_dossier_istore_attach_to( ofaDossierIStore *instance, GtkContainer *parent )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_DOSSIER_ISTORE( instance ));
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, instance );

	if( OFA_DOSSIER_ISTORE_GET_INTERFACE( instance )->attach_to ){
		OFA_DOSSIER_ISTORE_GET_INTERFACE( instance )->attach_to( instance, parent );
	}

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

/**
 * ofa_dossier_istore_set_columns:
 * @instance: this #ofaDossierIStore instance.
 * @columns: the columns to be displayed from the #GtkListStore.
 */
void
ofa_dossier_istore_set_columns( ofaDossierIStore *instance, ofaDossierColumns columns )
{
	sIStore *sdata;

	g_return_if_fail( instance && G_IS_OBJECT( instance ) && OFA_IS_DOSSIER_ISTORE( instance ));

	sdata = get_istore_data( instance );
	g_return_if_fail( sdata );

	sdata->columns = columns;

	sdata->store = gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING );								/* name */

	if( OFA_DOSSIER_ISTORE_GET_INTERFACE( instance )->set_columns ){
		OFA_DOSSIER_ISTORE_GET_INTERFACE( instance )->set_columns( instance, sdata->store, columns );
	}

	g_object_unref( sdata->store );

	load_dataset( instance, sdata );
}

static void
load_dataset( ofaDossierIStore *instance, sIStore *sdata )
{
	GSList *dataset, *it;

	dataset = ofa_dossier_misc_get_dossiers();

	for( it=dataset ; it ; it=it->next ){
		insert_row( instance, sdata, ( const gchar * ) it->data );
	}

	ofa_dossier_misc_free_dossiers( dataset );
}

static void
insert_row( ofaDossierIStore *instance, sIStore *sdata, const gchar *dname )
{
	GtkTreeIter iter;

	gtk_list_store_insert( sdata->store, &iter, -1 );
	set_row( instance, sdata, &iter, dname );
}

static void
set_row( ofaDossierIStore *instance, sIStore *sdata, GtkTreeIter *iter, const gchar *dname )
{
	gtk_list_store_set(
			sdata->store,
			iter,
			COL_DNAME, dname,
			-1 );
}

/**
 * ofa_dossier_istore_get_column_number:
 * @instance: this #ofaDossierIStore instance.
 * @column: the #ofaDossierColumns identifier.
 *
 * Returns: the number of the column in the store, counted from zero,
 * or -1 if the column is unknown.
 */
gint
ofa_dossier_istore_get_column_number( const ofaDossierIStore *instance, ofaDossierColumns column )
{
	static const gchar *thisfn = "ofa_dossier_istore_get_column_number";

	switch( column ){
		case DOSSIER_COL_DNAME:
			return( COL_DNAME );
			break;
	}

	g_warning( "%s: unknown column:%d", thisfn, column );
	return( -1 );
}

static void
on_parent_finalized( ofaDossierIStore *instance, gpointer finalized_parent )
{
	static const gchar *thisfn = "ofa_dossier_istore_on_parent_finalized";

	g_debug( "%s: instance=%p, finalized_parent=%p",
			thisfn, ( void * ) instance, ( void * ) finalized_parent );

	g_return_if_fail( instance );

	g_object_unref( G_OBJECT( instance ));
}

static void
on_object_finalized( sIStore *sdata, gpointer finalized_object )
{
	static const gchar *thisfn = "ofa_dossier_istore_on_object_finalized";

	g_debug( "%s: sdata=%p, finalized_object=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_object );

	g_return_if_fail( sdata );

	g_free( sdata );
}

static sIStore *
get_istore_data( ofaDossierIStore *instance )
{
	sIStore *sdata;

	sdata = ( sIStore * ) g_object_get_data( G_OBJECT( instance ), DOSSIER_ISTORE_DATA );

	if( !sdata ){
		sdata = g_new0( sIStore, 1 );
		g_object_set_data( G_OBJECT( instance ), DOSSIER_ISTORE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_object_finalized, sdata );
	}

	return( sdata );
}
