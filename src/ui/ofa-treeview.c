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

#include "ui/ofa-treeview.h"
#include "ui/ofa-treeview-prot.h"

/* private instance data
 */
struct _ofaTreeviewPrivate {

	/* properties
	 */
	gboolean     use_boxes;

	/* internals
	 */
	GtkTreeView *tview;
};

/* ofaTreeview class properties
 */
enum {
	PROP_USE_BOXES_ID = 1
};

/* signals defined by ofaTreeview class
 */
enum {
	ROW_SELECTED,
	ROW_ACTIVATED,
	KEY_PRESSED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofaTreeview, ofa_treeview, GTK_TYPE_SCROLLED_WINDOW )

static void          tview_build( ofaTreeview *self );
static GtkTreeModel *v_tree_model_new( ofaTreeview *self );
static gboolean      on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTreeview *self );
static void          on_key_pressed_signal_handler( ofaTreeview *self, guint keyval );
static void          on_row_selected( GtkTreeSelection *selection, ofaTreeview *self );
static void          on_row_selected_signal_handler( ofaTreeview *self, GList *selection );
static void          on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTreeview *self );
static void          on_row_activated_signal_handler( ofaTreeview *self, GList *selection );

static void
treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_treeview_parent_class )->finalize( instance );
}

static void
treeview_dispose( GObject *instance )
{
	ofaTreeviewProtected *prot;

	g_return_if_fail( instance && OFA_IS_TREEVIEW( instance ));

	prot = ( OFA_TREEVIEW( instance ))->prot;

	if( !prot->dispose_has_run ){

		prot->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_treeview_parent_class )->dispose( instance );
}

static void
treeview_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_treeview_constructed";

	g_return_if_fail( instance && OFA_IS_TREEVIEW( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* chain up to the parent class */
	if( G_OBJECT_CLASS( ofa_treeview_parent_class )->constructed ){
		G_OBJECT_CLASS( ofa_treeview_parent_class )->constructed( instance );
	}

	/* build the treeview */
	tview_build( OFA_TREEVIEW( instance ));
}

static void
tview_build( ofaTreeview *self )
{
	ofaTreeviewPrivate *priv;
	GtkTreeSelection *select;
	GtkTreeModel *tmodel;

	priv = self->priv;

	priv->tview = GTK_TREE_VIEW( gtk_tree_view_new());
	gtk_container_add( GTK_CONTAINER( self ), GTK_WIDGET( priv->tview ));

	/* tree view setup */
	gtk_tree_view_set_headers_visible( priv->tview, TRUE );

	/* tree model setup */
	tmodel = v_tree_model_new( self );
	g_return_if_fail( tmodel && GTK_IS_TREE_MODEL( tmodel ));
	gtk_tree_view_set_model( priv->tview, tmodel );

	/* define the columns */

	/* connect the signals */
	g_signal_connect(
			G_OBJECT( priv->tview ), "row-activated", G_CALLBACK( on_row_activated), self );
	g_signal_connect(
			G_OBJECT( priv->tview ), "key-press-event", G_CALLBACK( on_key_pressed ), self );

	/* setup the selection */
	select = gtk_tree_view_get_selection( priv->tview );
	g_signal_connect(
			G_OBJECT( select ), "changed", G_CALLBACK( on_row_selected ), self );
}

static GtkTreeModel *
v_tree_model_new( ofaTreeview *self )
{
	static const gchar *thisfn = "ofa_treeview_treemodel_new";

	if( OFA_TREEVIEW_GET_CLASS( self )->tree_model_new ){
		return( OFA_TREEVIEW_GET_CLASS( self )->tree_model_new( self ));
	}

	g_warning( "%s: self=%p (%s): treemodel_new not implemented by any derived class (but should)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	return( NULL );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
treeview_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaTreeviewProtected *prot;
	ofaTreeviewPrivate *priv;

	g_return_if_fail( OFA_IS_TREEVIEW( instance ));

	prot = OFA_TREEVIEW( instance )->prot;

	if( !prot->dispose_has_run ){

		priv = OFA_TREEVIEW( instance )->priv;

		switch( property_id ){
			case PROP_USE_BOXES_ID:
				g_value_set_boolean( value, priv->use_boxes );
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
treeview_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaTreeviewProtected *prot;
	ofaTreeviewPrivate *priv;

	g_return_if_fail( OFA_IS_TREEVIEW( instance ));

	prot = OFA_TREEVIEW( instance )->prot;

	if( !prot->dispose_has_run ){

		priv = OFA_TREEVIEW( instance )->priv;

		switch( property_id ){
			case PROP_USE_BOXES_ID:
				priv->use_boxes = g_value_get_boolean( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
ofa_treeview_init( ofaTreeview *self )
{
	static const gchar *thisfn = "ofa_treeview_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TREEVIEW( self ));

	self->prot = g_new0( ofaTreeviewProtected, 1 );
	self->prot->dispose_has_run = FALSE;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TREEVIEW, ofaTreeviewPrivate );
}

static void
ofa_treeview_class_init( ofaTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = treeview_constructed;
	G_OBJECT_CLASS( klass )->get_property = treeview_get_property;
	G_OBJECT_CLASS( klass )->set_property = treeview_set_property;
	G_OBJECT_CLASS( klass )->dispose = treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = treeview_finalize;

	g_type_class_add_private( klass, sizeof( ofaTreeviewPrivate ));

	/**
	 * ofaTreeview::ofa-treeview-signal-row-selected:
	 *
	 * This signal is sent when the selection changes in the treeview.
	 *
	 * The passed @selection may be %NULL if the selection is empty.
	 * Else, the GList contains references to selected #ofoBase objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTreeview *treeview,
	 * 						GList *selection,
	 * 						gpointer user_data );
	 */
	st_signals[ ROW_SELECTED ] = g_signal_new_class_handler(
				TREEVIEW_SIGNAL_ROW_SELECTED,
				OFA_TYPE_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_row_selected_signal_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaTreeview::ofa-treeview-signal-row-activated:
	 *
	 * This signal is sent when the selection in the treeview is
	 * activated.
	 *
	 * The GList contains references to selected #ofoBase objects.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTreeview *treeview,
	 * 						GList *selection,
	 * 						gpointer user_data );
	 */
	st_signals[ ROW_ACTIVATED ] = g_signal_new_class_handler(
				TREEVIEW_SIGNAL_ROW_ACTIVATED,
				OFA_TYPE_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_row_activated_signal_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );

	/**
	 * ofaTreeview::ofa-treeview-signal-key-pressed:
	 *
	 * This signal is sent when a key is pressed in the treeview.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTreeview *treeview,
	 * 						guint keyval,
	 * 						gpointer user_data );
	 */
	st_signals[ ROW_ACTIVATED ] = g_signal_new_class_handler(
				TREEVIEW_SIGNAL_ROW_ACTIVATED,
				OFA_TYPE_TREEVIEW,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_key_pressed_signal_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_UINT );

	/* install ofaTreeview class properties
	 */
	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_USE_BOXES_ID,
			g_param_spec_boolean(
					TREEVIEW_PROP_USE_BOXES,
					"Use boxes",
					"Whether the derived class makes use of ofaBoxes fields",
					FALSE,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_key_pressed( GtkWidget *widget, GdkEventKey *event, ofaTreeview *self )
{
	gboolean stop;

	stop = FALSE;

	if( event->state == 0 ){
		g_signal_emit_by_name( G_OBJECT( self ), TREEVIEW_SIGNAL_KEY_PRESSED, event->keyval );
		stop = TRUE;
	}

	return( stop );
}

static void
on_key_pressed_signal_handler( ofaTreeview *self, guint keyval )
{
	static const gchar *thisfn = "ofa_treeview_on_key_pressed_signal_handler";

	g_debug( "%s: self=%p, keyval=%u", thisfn, ( void * ) self, keyval );
}

static void
on_row_selected( GtkTreeSelection *selection, ofaTreeview *self )
{

}

static void
on_row_selected_signal_handler( ofaTreeview *self, GList *selection )
{
	static const gchar *thisfn = "ofa_treeview_on_row_selected_signal_handler";

	g_debug( "%s: self=%p, selection=%p", thisfn, ( void * ) self, ( void * ) selection );
}

static void
on_row_activated( GtkTreeView *tview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTreeview *self )
{

}

static void
on_row_activated_signal_handler( ofaTreeview *self, GList *selection )
{
	static const gchar *thisfn = "ofa_treeview_on_row_activated_signal_handler";

	g_debug( "%s: self=%p, selection=%p", thisfn, ( void * ) self, ( void * ) selection );
}
