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
#include <stdlib.h>

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-periodicity.h"
#include "api/ofa-preferences.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ofa-recurrent-run-page.h"
#include "ofa-recurrent-run-treeview.h"
#include "ofo-recurrent-run.h"

/* priv instance data
 */
struct _ofaRecurrentRunPagePrivate {

	/* internals
	 */
	ofaHub                  *hub;
	gboolean                 is_current;

	/* UI
	 */
	GtkWidget               *top_paned;
	ofaRecurrentRunTreeview *tview;
	GtkWidget               *cancelled_toggle;
	GtkWidget               *waiting_toggle;
	GtkWidget               *validated_toggle;
	GtkWidget               *generated_toggle;
	GtkWidget               *generate_btn;

	/* filtering
	 */
	gboolean                 cancelled_visible;
	gboolean                 waiting_visible;
	gboolean                 validated_visible;
	gboolean                 generated_visible;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/recurrent/ofa-recurrent-run-page.ui";
static const gchar *st_page_settings    = "ofaRecurrentRunPage-settings";

static GtkWidget       *v_setup_view( ofaPage *page );
static void             setup_treeview( ofaRecurrentRunPage *self, GtkContainer *parent );
static void             setup_filters( ofaRecurrentRunPage *self, GtkContainer *parent );
static void             setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent );
static void             setup_datas( ofaRecurrentRunPage *self );
static GtkWidget       *v_get_top_focusable_widget( const ofaPage *page );
//static ofoRecurrentRun *treeview_get_selected( ofaRecurrentRunPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter );
static void             filter_on_cancelled_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void             filter_on_waiting_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void             filter_on_validated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void             filter_on_generated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self );
static void             action_on_generate_clicked( GtkButton *button, ofaRecurrentRunPage *self );
static void             get_settings( ofaRecurrentRunPage *self );
static void             set_settings( ofaRecurrentRunPage *self );

G_DEFINE_TYPE_EXTENDED( ofaRecurrentRunPage, ofa_recurrent_run_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaRecurrentRunPage ))

static void
recurrent_run_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_page_parent_class )->finalize( instance );
}

static void
recurrent_run_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_RECURRENT_RUN_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		set_settings( OFA_RECURRENT_RUN_PAGE( instance ));

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_recurrent_run_page_parent_class )->dispose( instance );
}

static void
ofa_recurrent_run_page_init( ofaRecurrentRunPage *self )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_RECURRENT_RUN_PAGE( self ));
}

static void
ofa_recurrent_run_page_class_init( ofaRecurrentRunPageClass *klass )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = recurrent_run_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = recurrent_run_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_recurrent_run_page_v_setup_view";
	ofaRecurrentRunPagePrivate *priv;
	ofoDossier *dossier;
	GtkWidget *page_widget, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	priv->hub = ofa_page_get_hub( page );
	g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

	dossier = ofa_hub_get_dossier( priv->hub );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	priv->is_current = ofo_dossier_is_current( dossier );

	page_widget = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_hexpand( page_widget, TRUE );
	widget = my_utils_container_attach_from_resource( GTK_CONTAINER( page_widget ), st_resource_ui, "RecurrentRunPageWindow", "top" );
	g_return_val_if_fail( widget && GTK_IS_PANED( widget ), NULL );
	priv->top_paned = widget;

	setup_treeview( OFA_RECURRENT_RUN_PAGE( page ), GTK_CONTAINER( priv->top_paned ));
	setup_filters( OFA_RECURRENT_RUN_PAGE( page ), GTK_CONTAINER( priv->top_paned ));
	setup_actions( OFA_RECURRENT_RUN_PAGE( page ), GTK_CONTAINER( priv->top_paned ));

	setup_datas( OFA_RECURRENT_RUN_PAGE( page ));
	get_settings( OFA_RECURRENT_RUN_PAGE( page ));

	return( page_widget );
}

/*
 * initialize the treeview
 */
static void
setup_treeview( ofaRecurrentRunPage *self, GtkContainer *parent )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *tview_parent;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	tview_parent = my_utils_container_get_child_by_name( parent, "tview-parent" );
	g_return_if_fail( tview_parent && GTK_IS_CONTAINER( tview_parent ));

	priv->tview = ofa_recurrent_run_treeview_new( priv->hub, TRUE );
	gtk_container_add( GTK_CONTAINER( tview_parent ), GTK_WIDGET( priv->tview ));

	gtk_widget_show_all( GTK_WIDGET( parent ));
}

/*
 * initialize the filter area
 */
static void
setup_filters( ofaRecurrentRunPage *self, GtkContainer *parent )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *btn;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( parent, "p3-cancelled-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->cancelled_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_cancelled_btn_toggled ), self );

	btn = my_utils_container_get_child_by_name( parent, "p3-waiting-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->waiting_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_waiting_btn_toggled ), self );

	btn = my_utils_container_get_child_by_name( parent, "p3-validated-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->validated_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_validated_btn_toggled ), self );

	btn = my_utils_container_get_child_by_name( parent, "p3-generated-btn" );
	g_return_if_fail( btn && GTK_IS_CHECK_BUTTON( btn ));
	priv->generated_toggle = btn;
	g_signal_connect( btn, "toggled", G_CALLBACK( filter_on_generated_btn_toggled ), self );
}

/*
 * initialize the actions area
 */
static void
setup_actions( ofaRecurrentRunPage *self, GtkContainer *parent )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkWidget *btn;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	btn = my_utils_container_get_child_by_name( parent, "p2-generate-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	priv->generate_btn = btn;
	g_signal_connect( btn, "clicked", G_CALLBACK( action_on_generate_clicked ), self );
}

/*
 * load the current datas
 */
static void
setup_datas( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	ofa_recurrent_run_treeview_set_from_db( priv->tview );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaRecurrentRunPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_RECURRENT_RUN_PAGE( page ), NULL );

	priv = ofa_recurrent_run_page_get_instance_private( OFA_RECURRENT_RUN_PAGE( page ));

	return( ofa_recurrent_run_treeview_get_treeview( priv->tview ));
}

#if 0
static ofoRecurrentRun  *
treeview_get_selected( ofaRecurrentRunPage *page, GtkTreeModel **tmodel, GtkTreeIter *iter )
{
	ofaRecurrentRunPagePrivate *priv;
	GtkTreeSelection *select;
	ofoRecurrentRun  *object;

	priv = ofa_recurrent_run_page_get_instance_private( page );

	object = NULL;

	select = gtk_tree_view_get_selection( GTK_TREE_VIEW( priv->record_treeview ));
	if( gtk_tree_selection_get_selected( select, tmodel, iter )){
		gtk_tree_model_get( *tmodel, iter, RECURRENT_RUN_COL_OBJECT, &object, -1 );
		g_return_val_if_fail( object && OFO_IS_RECURRENT_RUN( object ), NULL );
		g_object_unref( object );
	}

	return( object );
}
#endif

static void
filter_on_cancelled_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->cancelled_visible = gtk_toggle_button_get_active( button );

	ofa_recurrent_run_treeview_set_visible( priv->tview, REC_STATUS_CANCELLED, priv->cancelled_visible );

	set_settings( self );
}

static void
filter_on_waiting_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->waiting_visible = gtk_toggle_button_get_active( button );

	ofa_recurrent_run_treeview_set_visible( priv->tview, REC_STATUS_WAITING, priv->waiting_visible );

	set_settings( self );
}

static void
filter_on_validated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->validated_visible = gtk_toggle_button_get_active( button );

	ofa_recurrent_run_treeview_set_visible( priv->tview, REC_STATUS_VALIDATED, priv->validated_visible );

	set_settings( self );
}

static void
filter_on_generated_btn_toggled( GtkToggleButton *button, ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	priv->generated_visible = gtk_toggle_button_get_active( button );

	ofa_recurrent_run_treeview_set_visible( priv->tview, REC_STATUS_GENERATED, priv->generated_visible );

	set_settings( self );
}

static void
action_on_generate_clicked( GtkButton *button, ofaRecurrentRunPage *self )
{

}

/*
 * settings: sort_column_id;sort_sens;paned_position;cancelled_visible;waiting_visible;validated_visible;generated_visible;
 */
static void
get_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	GList *slist, *it;
	const gchar *cstr;
	gint sort_column_id, sort_sens, pos;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	slist = ofa_settings_user_get_string_list( st_page_settings );

	it = slist ? slist : NULL;
	cstr = it ? it->data : NULL;
	sort_column_id = cstr ? atoi( cstr ) : 0;

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	sort_sens = cstr ? atoi( cstr ) : 0;

	ofa_recurrent_run_treeview_set_sort_settings( priv->tview, sort_column_id, sort_sens );

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	pos = 0;
	if( my_strlen( cstr )){
		pos = atoi( cstr );
	}
	if( pos <= 10 ){
		pos = 150;
	}
	gtk_paned_set_position( GTK_PANED( priv->top_paned ), pos );

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->cancelled_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_cancelled_btn_toggled( GTK_TOGGLE_BUTTON( priv->cancelled_toggle ), self );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->waiting_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_waiting_btn_toggled( GTK_TOGGLE_BUTTON( priv->waiting_toggle ), self );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->validated_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_validated_btn_toggled( GTK_TOGGLE_BUTTON( priv->validated_toggle ), self );
	}

	it = it ? it->next : NULL;
	cstr = it ? it->data : NULL;
	if( my_strlen( cstr )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( priv->generated_toggle ), my_utils_boolean_from_str( cstr ));
		filter_on_generated_btn_toggled( GTK_TOGGLE_BUTTON( priv->generated_toggle ), self );
	}

	ofa_settings_free_string_list( slist );
}

static void
set_settings( ofaRecurrentRunPage *self )
{
	ofaRecurrentRunPagePrivate *priv;
	gchar *str;
	gint sort_column_id, sort_sens, pos;

	priv = ofa_recurrent_run_page_get_instance_private( self );

	ofa_recurrent_run_treeview_get_sort_settings( priv->tview, &sort_column_id, &sort_sens );
	pos = gtk_paned_get_position( GTK_PANED( priv->top_paned ));

	str = g_strdup_printf( "%d;%d;%d;%s;%s;%s;%s;",
			sort_column_id, sort_sens, pos,
			priv->cancelled_visible ? "True":"False",
			priv->waiting_visible ? "True":"False",
			priv->validated_visible ? "True":"False",
			priv->generated_visible ? "True":"False" );

	ofa_settings_user_set_string( st_page_settings, str );

	g_free( str );
}
