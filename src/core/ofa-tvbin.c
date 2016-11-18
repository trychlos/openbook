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

#include "my/my-date-renderer.h"
#include "my/my-double-renderer.h"
#include "my/my-utils.h"

#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-itvfilterable.h"
#include "api/ofa-itvsortable.h"
#include "api/ofa-preferences.h"
#include "api/ofa-tvbin.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* properties
	 */
	gboolean         headers_visible;
	gboolean         hexpand;
	GtkPolicyType    hpolicy;
	gchar           *name;
	GtkSelectionMode selection_mode;
	GtkShadowType    shadow;
	gboolean         vexpand;
	gboolean         write_settings;

	/* UI
	 */
	GtkWidget       *frame;
	GtkWidget       *scrolled;
	GtkWidget       *treeview;
}
	ofaTVBinPrivate;

/* class properties
 */
enum {
	PROP_HEADERS_ID = 1,
	PROP_HEXPAND_ID,
	PROP_HPOLICY_ID,
	PROP_NAME_ID,
	PROP_SELMODE_ID,
	PROP_SHADOW_ID,
	PROP_VEXPAND_ID,
	PROP_WRITESETTINGS_ID,
};

/* the function which is called when initializing an editable cell renderer
 */
typedef void (*ofaTVBinInitEditableFn )( gint, GtkCellRenderer *, void * );

/* signals defined here
 */
enum {
	CHANGED = 0,
	ACTIVATED,
	INSERT,
	DELETE,
	N_SIGNALS
};

static guint        st_signals[ N_SIGNALS ] = { 0 };

static void         init_top_widget( ofaTVBin *self );
static void         tview_on_row_selected( GtkTreeSelection *selection, ofaTVBin *self );
static void         tview_on_row_activated( GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVBin *self );
static gboolean     tview_on_key_pressed( GtkWidget *treeview, GdkEventKey *event, ofaTVBin *self );
static void         add_column( ofaTVBin *bin, GtkTreeViewColumn *column, gint column_id, const gchar *menu_label );
static void         iactionable_iface_init( ofaIActionableInterface *iface );
static guint        iactionable_get_interface_version( void );
static void         icontext_iface_init( ofaIContextInterface *iface );
static guint        icontext_get_interface_version( void );
static GtkWidget   *icontext_get_focused_widget( ofaIContext *instance );
static void         itvcolumnable_iface_init( ofaITVColumnableInterface *iface );
static guint        itvcolumnable_get_interface_version( void );
static void         itvfilterable_iface_init( ofaITVFilterableInterface *iface );
static guint        itvfilterable_get_interface_version( void );
static gboolean     itvfilterable_filter_model( const ofaITVFilterable *instance, GtkTreeModel *model, GtkTreeIter *iter );
static void         itvsortable_iface_init( ofaITVSortableInterface *iface );
static guint        itvsortable_get_interface_version( void );
static gint         itvsortable_get_column_id( ofaITVSortable *instance, GtkTreeViewColumn *column );
static gboolean     itvsortable_has_sort_model( const ofaITVSortable *instance );
static gint         itvsortable_sort_model( const ofaITVSortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id );

G_DEFINE_TYPE_EXTENDED( ofaTVBin, ofa_tvbin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaTVBin )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICONTEXT, icontext_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITVCOLUMNABLE, itvcolumnable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITVFILTERABLE, itvfilterable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ITVSORTABLE, itvsortable_iface_init ))

static void
tvbin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tvbin_finalize";
	ofaTVBinPrivate *priv;

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVBIN( instance ));

	/* free data members here */
	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	g_free( priv->name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tvbin_parent_class )->finalize( instance );
}

static void
tvbin_dispose( GObject *instance )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVBIN( instance ));

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		if( priv->write_settings ){
			ofa_itvcolumnable_write_columns_settings( OFA_ITVCOLUMNABLE( instance ));
		}

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tvbin_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
tvbin_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( OFA_IS_TVBIN( instance ));

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_HEADERS_ID:
				g_value_set_boolean( value, priv->headers_visible );
				break;

			case PROP_HEXPAND_ID:
				g_value_set_boolean( value, priv->hexpand );
				break;

			case PROP_HPOLICY_ID:
				g_value_set_int( value, priv->hpolicy );
				break;

			case PROP_NAME_ID:
				g_value_set_string( value, priv->name );
				break;

			case PROP_SELMODE_ID:
				g_value_set_int( value, priv->selection_mode );
				break;

			case PROP_SHADOW_ID:
				g_value_set_int( value, priv->shadow );
				break;

			case PROP_VEXPAND_ID:
				g_value_set_boolean( value, priv->vexpand );
				break;

			case PROP_WRITESETTINGS_ID:
				g_value_set_boolean( value, priv->write_settings );
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
tvbin_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( OFA_IS_TVBIN( instance ));

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_HEADERS_ID:
				priv->headers_visible = g_value_get_boolean( value );
				break;

			case PROP_HEXPAND_ID:
				priv->hexpand  = g_value_get_boolean( value );
				break;

			case PROP_HPOLICY_ID:
				priv->hpolicy = g_value_get_int( value );
				break;

			case PROP_NAME_ID:
				g_free( priv->name );
				priv->name = g_strdup( g_value_get_string( value ));
				break;

			case PROP_SELMODE_ID:
				priv->selection_mode = g_value_get_int( value );
				break;

			case PROP_SHADOW_ID:
				priv->shadow = g_value_get_int( value );
				break;

			case PROP_VEXPAND_ID:
				priv->vexpand = g_value_get_boolean( value );
				break;

			case PROP_WRITESETTINGS_ID:
				priv->write_settings = g_value_get_boolean( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * Called at the end of the instance initialization, after properties
 * have been set.
 * At this time, the instance is rightly typed
 */
static void
tvbin_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_tvbin_constructed";
	ofaTVBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVBIN( instance ));

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( ofa_tvbin_parent_class )->constructed ){
		G_OBJECT_CLASS( ofa_tvbin_parent_class )->constructed( instance );
	}

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	if( !my_strlen( priv->name )){
		priv->name = g_strdup( G_OBJECT_TYPE_NAME( instance ));
		g_debug( "%s: settings defaut settings key to '%s'", thisfn, priv->name );
	}

	init_top_widget( OFA_TVBIN( instance ));
}

static void
init_top_widget( ofaTVBin *self )
{
	ofaTVBinPrivate *priv;
	GtkTreeSelection *selection;

	priv = ofa_tvbin_get_instance_private( self );

	priv->frame = gtk_frame_new( NULL );
	ofa_tvbin_set_shadow( self, priv->shadow );
	gtk_container_add( GTK_CONTAINER( self ), priv->frame );

	priv->scrolled = gtk_scrolled_window_new( NULL, NULL );
	ofa_tvbin_set_hpolicy( self, priv->hpolicy );
	gtk_container_add( GTK_CONTAINER( priv->frame ), priv->scrolled );

	priv->treeview = gtk_tree_view_new();
	ofa_tvbin_set_hexpand( self, priv->hexpand );
	ofa_tvbin_set_vexpand( self, priv->vexpand );
	ofa_tvbin_set_headers( self, priv->headers_visible );
	gtk_container_add( GTK_CONTAINER( priv->scrolled ), priv->treeview );

	g_signal_connect( priv->treeview, "row-activated", G_CALLBACK( tview_on_row_activated ), self );
	g_signal_connect( priv->treeview, "key-press-event", G_CALLBACK( tview_on_key_pressed ), self );

	ofa_tvbin_set_selection_mode( self, priv->selection_mode );
	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	g_signal_connect( selection, "changed", G_CALLBACK( tview_on_row_selected ), self );
}

static void
ofa_tvbin_init( ofaTVBin *self )
{
	static const gchar *thisfn = "ofa_tvbin_init";
	ofaTVBinPrivate *priv;

	g_return_if_fail( OFA_IS_TVBIN( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tvbin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_tvbin_class_init( ofaTVBinClass *klass )
{
	static const gchar *thisfn = "ofa_tvbin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = tvbin_get_property;
	G_OBJECT_CLASS( klass )->set_property = tvbin_set_property;
	G_OBJECT_CLASS( klass )->constructed = tvbin_constructed;
	G_OBJECT_CLASS( klass )->dispose = tvbin_dispose;
	G_OBJECT_CLASS( klass )->finalize = tvbin_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_HEADERS_ID,
			g_param_spec_boolean(
					"ofa-tvbin-headers",
					"Headers visible",
					"Whether the columns headers are visible",
					TRUE,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_HEXPAND_ID,
			g_param_spec_boolean(
					"ofa-tvbin-hexpand",
					"Expand horizontally",
					"Whether the treeview expand horizontally",
					TRUE,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_HPOLICY_ID,
			g_param_spec_int(
					"ofa-tvbin-hpolicy",
					"Horizontal scrollbar policy",
					"Horizontal scrollbar policy",
					0, 99, GTK_POLICY_AUTOMATIC,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_NAME_ID,
			g_param_spec_string(
					"ofa-tvbin-name",
					"Name",
					"Both the action group and the settings key",
					NULL,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_SELMODE_ID,
			g_param_spec_int(
					"ofa-tvbin-selmode",
					"Selection mode",
					"Selection mode in the treeview",
					0, 99, GTK_SELECTION_BROWSE,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_SHADOW_ID,
			g_param_spec_int(
					"ofa-tvbin-shadow",
					"Shadow type",
					"Shadow type of surrounding frame",
					0, 99, GTK_SHADOW_NONE,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_HEXPAND_ID,
			g_param_spec_boolean(
					"ofa-tvbin-vexpand",
					"Expand vertically",
					"Whether the treeview expand vertically",
					TRUE,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WRITESETTINGS_ID,
			g_param_spec_boolean(
					"ofa-tvbin-writesettings",
					"Column settings",
					"Whether to write the columns settings for this view",
					TRUE,
					G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS ));

	/**
	 * ofaTVBin::ofa-selchanged:
	 *
	 * This signal is sent on the #ofaTVBin when the selection is
	 * changed.
	 *
	 * Argument is the current #GtkTreeSelection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin           *bin,
	 * 						GtkTreeSelection *selection,
	 * 						gpointer          user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"ofa-selchanged",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVBin::ofa-selactivated:
	 *
	 * This signal is sent on the #ofaTVBin when the selection is
	 * activated.
	 *
	 * Argument is the current #GtkTreeSelection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin           *bin,
	 * 						GtkTreeSelection *selection,
	 * 						gpointer          user_data );
	 */
	st_signals[ ACTIVATED ] = g_signal_new_class_handler(
				"ofa-selactivated",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaTVBin::ofa-insert:
	 *
	 * This signal is sent on the #ofaTVBin when the Insert key is
	 * pressed.
	 *
	 * No arg.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin  *bin,
	 * 						gpointer user_data );
	 */
	st_signals[ INSERT ] = g_signal_new_class_handler(
				"ofa-insert",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0,
				G_TYPE_NONE );

	/**
	 * ofaTVBin::ofa-seldelete:
	 *
	 * This signal is sent on the #ofaTVBin when the Delete key is
	 * pressed.
	 *
	 * Argument is the current #GtkTreeSelection.
	 * The signal is not sent if there is no current selection.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaTVBin           *bin,
	 * 						GtkTreeSelection *selection,
	 * 						gpointer          user_data );
	 */
	st_signals[ DELETE ] = g_signal_new_class_handler(
				"ofa-seldelete",
				OFA_TYPE_TVBIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );
}

static void
tview_on_row_selected( GtkTreeSelection *selection, ofaTVBin *self )
{
	g_signal_emit_by_name( self, "ofa-selchanged", selection );
}

static void
tview_on_row_activated( GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, ofaTVBin *self )
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection( treeview );
	g_return_if_fail( gtk_tree_selection_count_selected_rows( selection ) > 0 );

	g_signal_emit_by_name( self, "ofa-selactivated", selection );
}

/*
 * Handles here Insert and Delete keys.
 *
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
tview_on_key_pressed( GtkWidget *treeview, GdkEventKey *event, ofaTVBin *self )
{
	gboolean stop;
	GtkTreeSelection *selection;

	stop = FALSE;

	if( event->state == 0 ){
		if( event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete ){
			selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( treeview ));
			if( gtk_tree_selection_count_selected_rows( selection ) > 0 ){
				g_signal_emit_by_name( self, "ofa-seldelete", selection );
			}

		} else if( event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert ){
			g_signal_emit_by_name( self, "ofa-insert" );
		}
	}

	return( stop );
}

/**
 * ofa_tvbin_get_headers:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the visibility of the treeview headers.
 */
gboolean
ofa_tvbin_get_headers( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), FALSE );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->headers_visible );
}

/**
 * ofa_tvbin_set_headers:
 * @bin: this #ofaTVBin instance.
 * @visible: whether the headers of the #GtkTreeView should be visible.
 *
 * Setup the visibility of the header.
 *
 * May also be set via the "ofa-tvbin-headers" construction property.
 *
 * Columns headers default to be visible.
 */
void
ofa_tvbin_set_headers( ofaTVBin *bin, gboolean visible )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ));

	priv->headers_visible = visible;
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( priv->treeview ), visible );
}

/**
 * ofa_tvbin_get_hexpand:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the horizontal expandable status of the #GtkTreeView.
 */
gboolean
ofa_tvbin_get_hexpand( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), FALSE );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->hexpand );
}

/**
 * ofa_tvbin_set_hexpand:
 * @bin: this #ofaTVBin instance.
 * @expand: whether the #GtkTreeView should horizontally expand.
 *
 * Setup the horizontal expandable status.
 *
 * May also be set via the "ofa-tvbin-hexpand" construction property.
 *
 * The treeview defaults to horizontally expand.
 */
void
ofa_tvbin_set_hexpand( ofaTVBin *bin, gboolean expand )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ));

	priv->hexpand = expand;
	gtk_widget_set_hexpand( priv->treeview, expand );
}

/**
 * ofa_tvbin_get_hpolicy:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the horizontal scrollbar policy for the #GtkScrolledWindow.
 */
GtkPolicyType
ofa_tvbin_get_hpolicy( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), GTK_POLICY_AUTOMATIC );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, GTK_POLICY_AUTOMATIC );

	return( priv->hpolicy );
}

/**
 * ofa_tvbin_set_hpolicy:
 * @bin: this #ofaTVBin instance.
 * @policy: the horizontal scrollbar policy.
 *
 * Setup the horizontal scrollbar policy for the #GtkScrolledWindow.
 *
 * May also be set via the "ofa-tvbin-hpolicy" construction property.
 *
 * The horizontal scrollbar policy defaults to GTK_POLICY_AUTOMATIC.
 */
void
ofa_tvbin_set_hpolicy( ofaTVBin *bin, GtkPolicyType policy )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->scrolled && GTK_IS_SCROLLED_WINDOW( priv->scrolled ));

	priv->hpolicy = policy;
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW( priv->scrolled ), priv->hpolicy, GTK_POLICY_AUTOMATIC );
}

/**
 * ofa_tvbin_get_name:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the identifier name.
 */
const gchar *
ofa_tvbin_get_name( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->name );
}

/**
 * ofa_tvbin_set_name:
 * @bin: this #ofaTVBin instance.
 * @name: [allow-none]: the identifier name.
 *
 * Setup the desired identifier name.
 * Reset the identifier name to its default if %NULL.
 *
 * The identifier name is used both as the actions group name of the
 * #ofaITVcolumnable interface, and as the settings key where size and
 * position of the treeview columns will be written.
 *
 * May also be set via the "ofa-tvbin-name" construction property.
 *
 * The identifier name defaults to the class name of the @bin instance
 * (e.g. 'ofaBatTreeview').
 */
void
ofa_tvbin_set_name( ofaTVBin *bin, const gchar *name )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( bin )) == 0 );

	g_free( priv->name );
	priv->name = g_strdup( my_strlen( name ) ? name : G_OBJECT_TYPE_NAME( bin ));
}

/**
 * ofa_tvbin_get_selection_mode:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the selection mode.
 */
GtkSelectionMode
ofa_tvbin_get_selection_mode( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), GTK_SELECTION_BROWSE );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, GTK_SELECTION_BROWSE );

	return( priv->selection_mode );
}

/**
 * ofa_tvbin_set_selection_mode:
 * @bin: this #ofaTVBin instance.
 * @mode: the selection mode to be set.
 *
 * Setup the desired selection mode.
 *
 * May also be set via the "ofa-tvbin-selmode" construction property.
 *
 * The selection mode defaults to GTK_SELECTION_BROWSE.
 */
void
ofa_tvbin_set_selection_mode( ofaTVBin *bin, GtkSelectionMode mode )
{
	ofaTVBinPrivate *priv;
	GtkTreeSelection *selection;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ));

	priv->selection_mode = mode;
	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	gtk_tree_selection_set_mode( selection, mode );
}

/**
 * ofa_tvbin_get_shadow:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the shadow type of the #GtkFrame.
 */
GtkShadowType
ofa_tvbin_get_shadow( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), GTK_SHADOW_NONE );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, GTK_SHADOW_NONE );

	return( priv->shadow );
}

/**
 * ofa_tvbin_set_shadow:
 * @bin: this #ofaTVBin instance.
 * @shadow: the shadow type of the frame.
 *
 * Setup the desired shadow type of the #GtkFrame.
 *
 * May also be set via the "ofa-tvbin-shadow" construction property.
 *
 * The shadow type defaults to GTK_SHADOW_NONE.
 */
void
ofa_tvbin_set_shadow( ofaTVBin *bin, GtkShadowType shadow )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->frame && GTK_IS_FRAME( priv->frame ));

	priv->shadow = shadow;
	gtk_frame_set_shadow_type( GTK_FRAME( priv->frame ), shadow );
}

/**
 * ofa_tvbin_get_vexpand:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the vertical expandable status of the #GtkTreeView.
 */
gboolean
ofa_tvbin_get_vexpand( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), FALSE );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->vexpand );
}

/**
 * ofa_tvbin_set_vexpand:
 * @bin: this #ofaTVBin instance.
 * @expand: whether the #GtkTreeView should verticaly expand.
 *
 * Setup the vertical expandable status.
 *
 * May also be set via the "ofa-tvbin-vexpand" construction property.
 *
 * The treeview defaults to vertically expand.
 */
void
ofa_tvbin_set_vexpand( ofaTVBin *bin, gboolean expand )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ));

	priv->vexpand = expand;
	gtk_widget_set_vexpand( priv->treeview, expand );
}

/*
 * ofa_tvbin_get_write_settings:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the @write indicator.
 */
gboolean
ofa_tvbin_get_write_settings( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), TRUE );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, TRUE );

	return( priv->write_settings );
}

/*
 * ofa_tvbin_set_write_settings:
 * @bin: this #ofaTVBin instance.
 * @write: whether to write the column settings.
 *
 * Set the @write indicator.
 *
 * When the @write indicator is set, the view will write the columns
 * settings (size and position) at dispose() time. This is the default.
 *
 * May also be set via the "ofa-tvbin-writesettings" construction property.
 */
void
ofa_tvbin_set_write_settings( ofaTVBin *bin, gboolean write )
{
	ofaTVBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->write_settings = write;
}

/*
 * ofa_tvbin_add_column_amount:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends an amount column to the treeview, defaulting to non visible.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_amount( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );

	my_double_renderer_init( cell,
			g_utf8_get_char( ofa_prefs_amount_thousand_sep()), g_utf8_get_char( ofa_prefs_amount_decimal_sep()),
			ofa_prefs_amount_accept_dot(), ofa_prefs_amount_accept_comma(), -1 );

	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Amount" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_date:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a date column to the treeview, defaulting to non visible.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_date( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	my_date_renderer_init( cell );

	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Date" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_int:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends an integer column to the treeview, defaulting to non visible.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_int( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1.0, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Int" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_pixbuf:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a pixbuf column to the treeview, defaulting to non visible.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_pixbuf( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Pixbuf" ), cell, "pixbuf", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_stamp:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a timestamp column to the treeview, defaulting to non visible.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_stamp( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Timestamp" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_text( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text_c:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 * The text is centered in the column.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_text_c( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 0.5, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text_lx:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 * The text ellipsizes on the left (only the right part may be visible).
 * This column is marked expandable.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_text_lx( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( cell ), "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_START, NULL );

	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_expand( column, TRUE );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text_rx:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 * The text ellipsizes on the right (only the left part may be visible).
 * This column is marked expandable.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_text_rx( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();
	g_object_set( G_OBJECT( cell ), "ellipsize-set", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL );

	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_expand( column, TRUE );

	add_column( bin, column, column_id, menu );
}

/*
 * ofa_tvbin_add_column_text_x:
 * @bin: this #ofaTVBin instance.
 * @column_id: the source column id,
 *  is also used as sortable column identifier.
 * @header: [allow-none]: the header of the column,
 *  if NULL, then takes default.
 * @menu: [allow-none]: the label of the popup menu,
 *  if NULL, then takes the column header.
 *
 * Appends a text column to the treeview, defaulting to non visible.
 * This column is marked expandable.
 *
 * It is expected that the identifier name be set before the first
 * column is defined, as this name also defines the action group
 * namespace the column's action will take place in.
 */
void
ofa_tvbin_add_column_text_x( ofaTVBin *bin, gint column_id, const gchar *header, const gchar *menu )
{
	ofaTVBinPrivate *priv;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	cell = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes(
					header ? header : _( "Text" ), cell, "text", column_id, NULL );
	gtk_tree_view_column_set_expand( column, TRUE );

	add_column( bin, column, column_id, menu );
}

static void
add_column( ofaTVBin *bin, GtkTreeViewColumn *column, gint column_id, const gchar *menu_label )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( bin );

	if( ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( bin )) == 0 ){
		ofa_itvcolumnable_set_name( OFA_ITVCOLUMNABLE( bin ), priv->name );
		ofa_itvcolumnable_set_treeview( OFA_ITVCOLUMNABLE( bin ), GTK_TREE_VIEW( priv->treeview ));
	}

	ofa_itvcolumnable_add_column( OFA_ITVCOLUMNABLE( bin ), column, column_id, menu_label );
}

/*
 * ofa_tvbin_get_selection:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the current #GtkTreeSelection.
 */
GtkTreeSelection *
ofa_tvbin_get_selection( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ), NULL );

	return( gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview )));
}

/*
 * ofa_tvbin_get_tree_view:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the current #GtkTreeView.
 */
GtkWidget *
ofa_tvbin_get_tree_view( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ), NULL );

	return( priv->treeview );
}

/*
 * ofa_tvbin_get_tree_model:
 * @bin: this #ofaTVBin instance.
 *
 * Returns: the #GtkTreeModel attached to the treeview.
 */
GtkTreeModel *
ofa_tvbin_get_tree_model( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_TVBIN( bin ), NULL );

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );
	g_return_val_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ), NULL );

	return( gtk_tree_view_get_model( GTK_TREE_VIEW( priv->treeview )));
}

/*
 * ofa_tvbin_select_first_row:
 * @bin: this #ofaTVBin instance.
 *
 * Select the first row of the treeview (if any).
 *
 * This is needed whe GTK_SELECTION_MULTIPLE is set: getting the focus
 * is not enough for the treeview selects something.
 */
void
ofa_tvbin_select_first_row( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_debug( "ofa_tvbin_select_first_row" );

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	model = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->treeview ));

	if( gtk_tree_model_get_iter_first( model, &iter )){
		ofa_tvbin_select_row( bin, &iter );
	}
}

/*
 * ofa_tvbin_select_row:
 * @bin: this #ofaTVBin instance.
 * @iter: a #GtkTreeIter on the #GtkTreeView's model.
 *
 * Select the specified row.
 */
void
ofa_tvbin_select_row( ofaTVBin *bin, GtkTreeIter *treeview_iter )
{
	ofaTVBinPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;

	g_debug( "ofa_tvbin_select_row" );

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	/* select the specified row */
	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->treeview ));
	gtk_tree_selection_select_iter( selection, treeview_iter );

	/* move the cursor so that it is visible */
	model = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->treeview ));
	path = gtk_tree_model_get_path( model, treeview_iter );
	gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( priv->treeview ), path, NULL, FALSE, 0, 0 );
	gtk_tree_view_set_cursor( GTK_TREE_VIEW( priv->treeview ), path, NULL, FALSE );
	gtk_tree_path_free( path );
}

/**
 * ofa_tvbin_set_cell_data_func:
 * @bin: this #ofaTVBin instance.
 * @fn_cell: the function.
 * @fn_data: user data.
 *
 * Setup the fonction to be used to draw the GtkCellRenderer's.
 *
 * It is expected that all columns have been defined prior a cell data
 * func be set, as this same cell data func has to be proxyied to each
 * and every column.
 */
void
ofa_tvbin_set_cell_data_func( ofaTVBin *bin, GtkTreeCellDataFunc fn_cell, void *fn_data )
{
	static const gchar *thisfn = "ofa_tvbin_set_cell_data_func";
	ofaTVBinPrivate *priv;
	GList *columns, *itco, *cells, *itce;

	g_debug( "%s: bin=%p, fn_cell=%p, fn_data=%p",
			thisfn, ( void * ) bin, ( void * ) fn_cell, ( void * ) fn_data );

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ));

	columns = gtk_tree_view_get_columns( GTK_TREE_VIEW( priv->treeview ));
	for( itco=columns ; itco ; itco=itco->next ){
		cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( itco->data ));
		for( itce=cells ; itce ; itce=itce->next ){
			gtk_tree_view_column_set_cell_data_func(
					GTK_TREE_VIEW_COLUMN( itco->data ), GTK_CELL_RENDERER( itce->data ), fn_cell, fn_data, NULL );
		}
		g_list_free( cells );
	}
	g_list_free( columns );
}

/**
 * ofa_tvbin_set_cell_edited_func:
 * @bin: this #ofaTVBin instance.
 * @fn_cell: the function.
 * @fn_data: user data.
 *
 * Setup the fonction to be used on GtkCellRendererText's edition.
 *
 * It is expected that all columns have been defined prior a cell data
 * func be set, as this same cell data func has to be propagated to each
 * and every column.
 */
void
ofa_tvbin_set_cell_edited_func( ofaTVBin *bin, GCallback fn_cell, void *fn_data )
{
	static const gchar *thisfn = "ofa_tvbin_set_cell_edited_func";
	ofaTVBinPrivate *priv;
	GList *columns, *itco, *cells, *itce;

	g_debug( "%s: bin=%p, fn_cell=%p, fn_data=%p",
			thisfn, ( void * ) bin, ( void * ) fn_cell, ( void * ) fn_data );

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( priv->treeview && GTK_IS_TREE_VIEW( priv->treeview ));

	columns = gtk_tree_view_get_columns( GTK_TREE_VIEW( priv->treeview ));
	for( itco=columns ; itco ; itco=itco->next ){
		cells = gtk_cell_layout_get_cells( GTK_CELL_LAYOUT( itco->data ));
		for( itce=cells ; itce ; itce=itce->next ){
			if( GTK_IS_CELL_RENDERER_TEXT( itce->data )){
				g_signal_connect( itce->data, "edited", fn_cell, fn_data );
			}
		}
		g_list_free( cells );
	}
	g_list_free( columns );
}

/*
 * ofa_tvbin_set_store:)
 * @bin: this #ofaTVBin instance.
 * @store: the underlying store.
 *
 * Records the store model.
 *
 * It is expected that all columns have been defined prior this store
 * be set, as this is now that the sort model will define the sort
 * function for each column.
 *
 * The @bin instance takes its own reference on the @store. This later
 * may so be released by the caller.
 *
 * Note that Gtk+ silently expects that the GtkTreeSortable model be
 * those associated with the GtkTreeView. In other words, we *must* have:
 *   GtkTreeView -> GtkTreeModelSort -> GtkTreeModelFilter -> store
 * (cf. https://github.com/GNOME/gtk/blob/master/gtk/gtktreeviewcolumn.c::
 *      gtk_tree_view_column_sort() method).
 */
void
ofa_tvbin_set_store( ofaTVBin *bin, GtkTreeModel *store )
{
	static const gchar *thisfn = "ofa_tvbin_set_store";
	ofaTVBinPrivate *priv;
	GtkTreeModel *sort_model, *filter_model;

	g_debug( "%s: bin=%p, store=%p", thisfn, ( void * ) bin, ( void * ) store );

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));
	g_return_if_fail( store && GTK_IS_TREE_MODEL( store ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );
	g_return_if_fail( ofa_itvcolumnable_get_columns_count( OFA_ITVCOLUMNABLE( bin )) > 0 );

	filter_model = ofa_itvfilterable_set_child_model( OFA_ITVFILTERABLE( bin ), store );

	sort_model = ofa_itvsortable_set_child_model( OFA_ITVSORTABLE( bin ), filter_model );
	ofa_itvsortable_set_name( OFA_ITVSORTABLE( bin ), priv->name );
	ofa_itvsortable_set_treeview( OFA_ITVSORTABLE( bin ), GTK_TREE_VIEW( priv->treeview ));

	gtk_tree_view_set_model( GTK_TREE_VIEW( priv->treeview ), sort_model );

	ofa_itvcolumnable_show_columns( OFA_ITVCOLUMNABLE( bin ));
	ofa_itvsortable_show_sort_indicator( OFA_ITVSORTABLE( bin ));
}

/*
 * ofa_tvbin_refilter:
 * @bin: this #ofaTVBin instance.
 *
 * Ask the underlying tree model to refilter itself.
 */
void
ofa_tvbin_refilter( ofaTVBin *bin )
{
	ofaTVBinPrivate *priv;
	GtkTreeModel *sort_model, *filter_model;

	g_return_if_fail( bin && OFA_IS_TVBIN( bin ));

	priv = ofa_tvbin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	sort_model = gtk_tree_view_get_model( GTK_TREE_VIEW( priv->treeview ));
	if( sort_model && GTK_IS_TREE_MODEL_SORT( sort_model )){
		filter_model = gtk_tree_model_sort_get_model( GTK_TREE_MODEL_SORT( sort_model ));
		if( filter_model && GTK_IS_TREE_MODEL_FILTER( filter_model )){
			gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( filter_model ));
		}
	}
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaIContext interface management
 */
static void
icontext_iface_init( ofaIContextInterface *iface )
{
	static const gchar *thisfn = "ofa_bat_treeview_icontext_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icontext_get_interface_version;
	iface->get_focused_widget = icontext_get_focused_widget;
}

static guint
icontext_get_interface_version( void )
{
	return( 1 );
}

static GtkWidget *
icontext_get_focused_widget( ofaIContext *instance )
{
	ofaTVBinPrivate *priv;

	priv = ofa_tvbin_get_instance_private( OFA_TVBIN( instance ));

	return( priv->treeview );
}

/*
 * ofaITVColumnable interface management
 */
static void
itvcolumnable_iface_init( ofaITVColumnableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_itvcolumnable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = itvcolumnable_get_interface_version;
}

static guint
itvcolumnable_get_interface_version( void )
{
	return( 1 );
}

/*
 * ofaITVFilterable interface management
 */
static void
itvfilterable_iface_init( ofaITVFilterableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_itvfilterable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = itvfilterable_get_interface_version;
	iface->filter_model = itvfilterable_filter_model;
}

static guint
itvfilterable_get_interface_version( void )
{
	return( 1 );
}

static gboolean
itvfilterable_filter_model( const ofaITVFilterable *instance, GtkTreeModel *model, GtkTreeIter *iter )
{
	gboolean visible = TRUE;

	if( OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->filter ){
		visible = OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->filter( OFA_TVBIN( instance ), model, iter );
	}

	return( visible );
}

/*
 * ofaITVSortable interface management
 */
static void
itvsortable_iface_init( ofaITVSortableInterface *iface )
{
	static const gchar *thisfn = "ofa_tvbin_itvsortable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = itvsortable_get_interface_version;
	iface->get_column_id = itvsortable_get_column_id;
	iface->has_sort_model = itvsortable_has_sort_model;
	iface->sort_model = itvsortable_sort_model;
}

static guint
itvsortable_get_interface_version( void )
{
	return( 1 );
}

static gint
itvsortable_get_column_id( ofaITVSortable *instance, GtkTreeViewColumn *column )
{
	return( ofa_itvcolumnable_get_column_id( OFA_ITVCOLUMNABLE( instance ), column ));
}

static gboolean
itvsortable_has_sort_model( const ofaITVSortable *instance )
{
	return( OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->sort != NULL );
}

static gint
itvsortable_sort_model( const ofaITVSortable *instance, GtkTreeModel *tmodel, GtkTreeIter *a, GtkTreeIter *b, gint column_id )
{
	gint cmp = 0;

	if( OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->sort ){
		cmp = OFA_TVBIN_GET_CLASS( OFA_TVBIN( instance ))->sort( OFA_TVBIN( instance ), tmodel, a, b, column_id );
	}

	return( cmp );
}
