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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-icontext.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-class.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-class-store.h"
#include "ui/ofa-class-treeview.h"

/* private instance data
 */
typedef struct {
	gboolean       dispose_has_run;

	/* UI
	 */
	ofaClassStore *store;
}
	ofaClassTreeviewPrivate;

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	DELETE,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void      setup_columns( ofaClassTreeview *self );
static void      on_selection_changed( ofaClassTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_activated( ofaClassTreeview *self, GtkTreeSelection *selection, void *empty );
static void      on_selection_delete( ofaClassTreeview *self, GtkTreeSelection *selection, void *empty );
static void      get_and_send( ofaClassTreeview *self, GtkTreeSelection *selection, const gchar *signal );
static ofoClass *get_selected_with_selection( ofaClassTreeview *self, GtkTreeSelection *selection );
static gint      v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaClassTreeview, ofa_class_treeview, OFA_TYPE_TVBIN, 0,
		G_ADD_PRIVATE( ofaClassTreeview ))

static void
class_treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_class_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_CLASS_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_treeview_parent_class )->finalize( instance );
}

static void
class_treeview_dispose( GObject *instance )
{
	ofaClassTreeviewPrivate *priv;

	g_return_if_fail( instance && OFA_IS_CLASS_TREEVIEW( instance ));

	priv = ofa_class_treeview_get_instance_private( OFA_CLASS_TREEVIEW( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_class_treeview_parent_class )->dispose( instance );
}

static void
ofa_class_treeview_init( ofaClassTreeview *self )
{
	static const gchar *thisfn = "ofa_class_treeview_init";
	ofaClassTreeviewPrivate *priv;

	g_return_if_fail( self && OFA_IS_CLASS_TREEVIEW( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_class_treeview_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->store = NULL;
}

static void
ofa_class_treeview_class_init( ofaClassTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_class_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = class_treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = class_treeview_finalize;

	OFA_TVBIN_CLASS( klass )->sort = v_sort;

	/**
	 * ofaClassTreeview::ofa-classchanged:
	 *
	 * #ofaTVBin sends a 'ofa-selchanged' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaClassTreeview proxyes it with this 'ofa-classchanged' signal,
	 * providing the #ofoClass selected object.
	 *
	 * Argument is the current #ofoClass object, may be %NULL.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaClassTreeview *view,
	 * 						ofoClass       *object,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-classchanged",
				OFA_TYPE_CLASS_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaClassTreeview::ofa-classactivated:
	 *
	 * #ofaTVBin sends a 'ofa-selactivated' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaClassTreeview proxyes it with this 'ofa-classactivated' signal,
	 * providing the #ofoClass selected object.
	 *
	 * Argument is the current #ofoClass object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaClassTreeview *view,
	 * 						ofoClass       *object,
	 * 						gpointer        user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-classactivated",
				OFA_TYPE_CLASS_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaClassTreeview::ofa-classdelete:
	 *
	 * #ofaTVBin sends a 'ofa-seldelete' signal, with the current
	 * #GtkTreeSelection as an argument.
	 * #ofaClassTreeview proxyes it with this 'ofa-classdelete' signal,
	 * providing the #ofoClass selected object.
	 *
	 * Argument is the current #ofoClass object.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaClassTreeview *view,
	 * 						ofoClass       *object,
	 * 						gpointer        user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-classdelete",
				OFA_TYPE_CLASS_TREEVIEW,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

/**
 * ofa_class_treeview_new:
 */
ofaClassTreeview *
ofa_class_treeview_new( void )
{
	ofaClassTreeview *view;

	view = g_object_new( OFA_TYPE_CLASS_TREEVIEW,
					"ofa-tvbin-hpolicy", GTK_POLICY_NEVER,
					"ofa-tvbin-shadow", GTK_SHADOW_IN,
					NULL );

	/* signals sent by ofaTVBin base class are intercepted to provide
	 * a #ofoClass object instead of just the raw GtkTreeSelection
	 */
	g_signal_connect( view, "ofa-selchanged", G_CALLBACK( on_selection_changed ), NULL );
	g_signal_connect( view, "ofa-selactivated", G_CALLBACK( on_selection_activated ), NULL );

	/* the 'ofa-seldelete' signal is sent in response to the Delete key press.
	 * There may be no current selection.
	 * in this case, the signal is just ignored (not proxied).
	 */
	g_signal_connect( view, "ofa-seldelete", G_CALLBACK( on_selection_delete ), NULL );

	setup_columns( view );

	return( view );
}

/*
 * Defines the treeview columns
 */
static void
setup_columns( ofaClassTreeview *self )
{
	static const gchar *thisfn = "ofa_class_treeview_setup_columns";

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	ofa_tvbin_add_column_int    ( OFA_TVBIN( self ), CLASS_COL_NUMBER,    _( "Number" ), _( "Class number" ));
	ofa_tvbin_add_column_text_x ( OFA_TVBIN( self ), CLASS_COL_LABEL,     _( "Label" ),      NULL );
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), CLASS_COL_NOTES,     _( "Notes" ),      NULL );
	ofa_tvbin_add_column_pixbuf ( OFA_TVBIN( self ), CLASS_COL_NOTES_PNG,    "",         _( "Notes indicator" ));
	ofa_tvbin_add_column_text   ( OFA_TVBIN( self ), CLASS_COL_UPD_USER,  _( "User" ),   _( "Last update user" ));
	ofa_tvbin_add_column_stamp  ( OFA_TVBIN( self ), CLASS_COL_UPD_STAMP,     NULL,      _( "Last update timestamp" ));

	ofa_itvcolumnable_set_default_column( OFA_ITVCOLUMNABLE( self ), CLASS_COL_LABEL );
}

/**
 * ofa_class_treeview_set_settings_key:
 * @view: this #ofaClassTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_class_treeview_set_settings_key( ofaClassTreeview *view, const gchar *key )
{
	static const gchar *thisfn = "ofa_class_treeview_set_settings_key";
	ofaClassTreeviewPrivate *priv;

	g_debug( "%s: view=%p, key=%s", thisfn, ( void * ) view, key );

	g_return_if_fail( view && OFA_IS_CLASS_TREEVIEW( view ));

	priv = ofa_class_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to the
	 * base class */
	ofa_tvbin_set_settings_key( OFA_TVBIN( view ), key );
}

/**
 * ofa_class_treeview_set_hub:
 * @view: this #ofaClassTreeview instance.
 * @hub: the current #ofaHub object.
 *
 * Initialize the underlying store.
 * Read the settings and show the columns accordingly.
 */
void
ofa_class_treeview_set_hub( ofaClassTreeview *view, ofaHub *hub )
{
	ofaClassTreeviewPrivate *priv;

	g_return_if_fail( view && OFA_IS_CLASS_TREEVIEW( view ));
	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = ofa_class_treeview_get_instance_private( view );

	g_return_if_fail( !priv->dispose_has_run );

	priv->store = ofa_class_store_new( hub );
	ofa_tvbin_set_store( OFA_TVBIN( view ), GTK_TREE_MODEL( priv->store ));
	g_object_unref( priv->store );

	ofa_itvsortable_set_default_sort( OFA_ITVSORTABLE( view ), CLASS_COL_NUMBER, GTK_SORT_ASCENDING );
}

static void
on_selection_changed( ofaClassTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-classchanged" );
}

static void
on_selection_activated( ofaClassTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-classactivated" );
}

/*
 * Delete key pressed
 * ofaTVBin base class makes sure the selection is not empty.
 */
static void
on_selection_delete( ofaClassTreeview *self, GtkTreeSelection *selection, void *empty )
{
	get_and_send( self, selection, "ofa-classdelete" );
}

/*
 * BAT may be %NULL when selection is empty (on 'ofa-batchanged' signal)
 */
static void
get_and_send( ofaClassTreeview *self, GtkTreeSelection *selection, const gchar *signal )
{
	ofoClass *class;

	class = get_selected_with_selection( self, selection );
	g_return_if_fail( !class || OFO_IS_CLASS( class ));

	g_signal_emit_by_name( self, signal, class );
}

/**
 * ofa_class_treeview_get_selected:
 * @view: this #ofaClassTreeview instance.
 *
 * Return: the currently selected class, or %NULL.
 */
ofoClass *
ofa_class_treeview_get_selected( ofaClassTreeview *view )
{
	static const gchar *thisfn = "ofa_class_treeview_get_selected";
	ofaClassTreeviewPrivate *priv;
	GtkTreeSelection *selection;
	ofoClass *class;

	g_debug( "%s: view=%p", thisfn, ( void * ) view );

	g_return_val_if_fail( view && OFA_IS_CLASS_TREEVIEW( view ), NULL );

	priv = ofa_class_treeview_get_instance_private( view );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	selection = ofa_tvbin_get_selection( OFA_TVBIN( view ));
	class = get_selected_with_selection( view, selection );

	return( class );
}

/*
 * get_selected_with_selection:
 * @view: this #ofaClassTreeview instance.
 * @selection:
 *
 * Return: the currently selected class, or %NULL.
 */
static ofoClass *
get_selected_with_selection( ofaClassTreeview *self, GtkTreeSelection *selection )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	ofoClass *class;

	class = NULL;
	if( gtk_tree_selection_get_selected( selection, &tmodel, &iter )){
		gtk_tree_model_get( tmodel, &iter, CLASS_COL_OBJECT, &class, -1 );
		g_return_val_if_fail( class && OFO_IS_CLASS( class ), NULL );
		g_object_unref( class );
	}

	return( class );
}

static gint
v_sort( const ofaTVBin *bin, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	static const gchar *thisfn = "ofa_class_treeview_v_sort";
	gint cmp, ida, idb;
	gchar *numa, *labela, *notesa, *updusera, *updstampa;
	gchar *numb, *labelb, *notesb, *upduserb, *updstampb;
	GdkPixbuf *pnga, *pngb;

	gtk_tree_model_get( tmodel, a,
			CLASS_COL_ID,        &ida,
			CLASS_COL_NUMBER,    &numa,
			CLASS_COL_LABEL,     &labela,
			CLASS_COL_NOTES,     &notesa,
			CLASS_COL_NOTES_PNG, &pnga,
			CLASS_COL_UPD_USER,  &updusera,
			CLASS_COL_UPD_STAMP, &updstampa,
			-1 );

	gtk_tree_model_get( tmodel, b,
			CLASS_COL_ID,        &idb,
			CLASS_COL_NUMBER,    &numb,
			CLASS_COL_LABEL,     &labelb,
			CLASS_COL_NOTES,     &notesb,
			CLASS_COL_NOTES_PNG, &pngb,
			CLASS_COL_UPD_USER,  &upduserb,
			CLASS_COL_UPD_STAMP, &updstampb,
			-1 );

	cmp = 0;

	switch( column_id ){
		case CLASS_COL_ID:
			cmp = ( ida < idb ? -1 : ( ida > idb ? 1 : 0 ));
			break;
		case CLASS_COL_NUMBER:
			cmp = ofa_itvsortable_sort_str_int( numa, numb );
			break;
		case CLASS_COL_LABEL:
			cmp = my_collate( labela, labelb );
			break;
		case CLASS_COL_NOTES:
			cmp = my_collate( notesa, notesb );
			break;
		case CLASS_COL_NOTES_PNG:
			cmp = ofa_itvsortable_sort_png( pnga, pngb );
			break;
		case CLASS_COL_UPD_USER:
			cmp = my_collate( updusera, upduserb );
			break;
		case CLASS_COL_UPD_STAMP:
			cmp = my_collate( updstampa, updstampb );
			break;
		default:
			g_warning( "%s: unhandled column: %d", thisfn, column_id );
			break;
	}

	g_free( numa );
	g_free( labela );
	g_free( notesa );
	g_free( updusera );
	g_free( updstampa );
	g_clear_object( &pnga );

	g_free( numb );
	g_free( labelb );
	g_free( notesb );
	g_free( upduserb );
	g_free( updstampb );
	g_clear_object( &pngb );

	return( cmp );
}
