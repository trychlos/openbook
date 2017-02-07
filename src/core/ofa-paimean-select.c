/*
 * Open Firm Paimeaning
 * A double-entry paimeaning application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Paimeaning is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Paimeaning is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Paimeaning; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofo-paimean.h"
#include "api/ofo-dossier.h"

#include "core/ofa-paimean-frame-bin.h"
#include "core/ofa-paimean-select.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* initialization
	 */
	ofaIGetter         *getter;
	GtkWindow          *parent;

	/* runtime
	 */
	gchar              *settings_prefix;

	/* UI
	 */
	ofaPaimeanFrameBin *fbin;
	GtkWidget          *ok_btn;

	/* returned value
	 */
	gchar              *paimean_code;
}
	ofaPaimeanSelectPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-paimean-select.ui";

static ofaPaimeanSelect *paimean_select_new( ofaIGetter *getter, GtkWindow *parent );
static void              iwindow_iface_init( myIWindowInterface *iface );
static void              iwindow_init( myIWindow *instance );
static void              idialog_iface_init( myIDialogInterface *iface );
static void              idialog_init( myIDialog *instance );
static void              on_selection_changed( ofaPaimeanFrameBin *bin, ofoPaimean *paimean, ofaPaimeanSelect *self );
static void              on_selection_activated( ofaPaimeanFrameBin *bin, ofoPaimean *paimean, ofaPaimeanSelect *self );
static void              check_for_enable_dlg( ofaPaimeanSelect *self );
static void              check_for_enable_dlg_with_paimean( ofaPaimeanSelect *self, ofoPaimean *paimean );
static gboolean          is_selection_valid( ofaPaimeanSelect *self, ofoPaimean *paimean );
static gboolean          idialog_quit_on_ok( myIDialog *instance );
static gboolean          do_select( ofaPaimeanSelect *self );

G_DEFINE_TYPE_EXTENDED( ofaPaimeanSelect, ofa_paimean_select, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaPaimeanSelect )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
paimean_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_paimean_select_finalize";
	ofaPaimeanSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAIMEAN_SELECT( instance ));

	/* free data members here */
	priv = ofa_paimean_select_get_instance_private( OFA_PAIMEAN_SELECT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->paimean_code );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_select_parent_class )->finalize( instance );
}

static void
paimean_select_dispose( GObject *instance )
{
	ofaPaimeanSelectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAIMEAN_SELECT( instance ));

	priv = ofa_paimean_select_get_instance_private( OFA_PAIMEAN_SELECT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_select_parent_class )->dispose( instance );
}

static void
ofa_paimean_select_init( ofaPaimeanSelect *self )
{
	static const gchar *thisfn = "ofa_paimean_select_init";
	ofaPaimeanSelectPrivate *priv;

	g_return_if_fail( self && OFA_IS_PAIMEAN_SELECT( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_paimean_select_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_paimean_select_class_init( ofaPaimeanSelectClass *klass )
{
	static const gchar *thisfn = "ofa_paimean_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_select_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/*
 * Returns: the unique #ofaPaimeanSelect instance.
 */
static ofaPaimeanSelect *
paimean_select_new( ofaIGetter *getter, GtkWindow *parent )
{
	ofaPaimeanSelect *dialog;
	ofaPaimeanSelectPrivate *priv;
	myICollector *collector;

	collector = ofa_igetter_get_collector( getter );

	dialog = ( ofaPaimeanSelect * ) my_icollector_single_get_object( collector, OFA_TYPE_PAIMEAN_SELECT );

	if( !dialog ){
		dialog = g_object_new( OFA_TYPE_PAIMEAN_SELECT, NULL );

		priv = ofa_paimean_select_get_instance_private( dialog );

		priv->getter = getter;
		priv->parent = parent;

		my_iwindow_init( MY_IWINDOW( dialog ));
		my_icollector_single_set_object( collector, dialog );
	}

	return( dialog );
}

/**
 * ofa_paimean_select_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @asked_code: [allow-none]: the initially selected identifier.
 *
 * Returns: the selected identifier, as a newly allocated string
 * that must be g_free() by the caller.
 */
gchar *
ofa_paimean_select_run( ofaIGetter *getter, GtkWindow *parent, const gchar *asked_code )
{
	static const gchar *thisfn = "ofa_paimean_select_run";
	ofaPaimeanSelect *dialog;
	ofaPaimeanSelectPrivate *priv;
	gchar *selected_id;

	g_debug( "%s: getter=%p, parent=%p, asked_code=%s",
			thisfn, ( void * ) getter, ( void * ) parent, asked_code );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), NULL );

	dialog = paimean_select_new( getter, parent );
	priv = ofa_paimean_select_get_instance_private( dialog );

	g_free( priv->paimean_code );
	priv->paimean_code = NULL;

	selected_id = NULL;
	ofa_paimean_frame_bin_set_selected( priv->fbin, asked_code );
	check_for_enable_dlg( dialog );

	if( my_idialog_run( MY_IDIALOG( dialog )) == GTK_RESPONSE_OK ){
		selected_id = g_strdup( priv->paimean_code );
		gtk_widget_hide( GTK_WIDGET( dialog ));
	}

	return( selected_id );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_paimean_select_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_paimean_select_iwindow_init";
	ofaPaimeanSelectPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_paimean_select_get_instance_private( OFA_PAIMEAN_SELECT( instance ));

	my_iwindow_set_parent( instance, priv->parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_paimean_select_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_paimean_select_idialog_init";
	ofaPaimeanSelectPrivate *priv;
	GtkWidget *parent;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_paimean_select_get_instance_private( OFA_PAIMEAN_SELECT( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "bin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->fbin = ofa_paimean_frame_bin_new( priv->getter, priv->settings_prefix );
	my_utils_widget_set_margins( GTK_WIDGET( priv->fbin ), 0, 4, 0, 0 );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->fbin ));

	g_signal_connect( priv->fbin, "ofa-changed", G_CALLBACK( on_selection_changed ), instance );
	g_signal_connect( priv->fbin, "ofa-activated", G_CALLBACK( on_selection_activated ), instance );
}

static void
on_selection_changed( ofaPaimeanFrameBin *bin, ofoPaimean *paimean, ofaPaimeanSelect *self )
{
	check_for_enable_dlg_with_paimean( self, paimean );
}

static void
on_selection_activated( ofaPaimeanFrameBin *bin, ofoPaimean *paimean, ofaPaimeanSelect *self )
{
	gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaPaimeanSelect *self )
{
	ofaPaimeanSelectPrivate *priv;
	ofoPaimean *paimean;

	priv = ofa_paimean_select_get_instance_private( self );

	paimean = ofa_paimean_frame_bin_get_selected( priv->fbin );
	check_for_enable_dlg_with_paimean( self, paimean );
}

static void
check_for_enable_dlg_with_paimean( ofaPaimeanSelect *self, ofoPaimean *paimean )
{
	ofaPaimeanSelectPrivate *priv;
	gboolean ok;

	priv = ofa_paimean_select_get_instance_private( self );

	ok = is_selection_valid( self, paimean );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
is_selection_valid( ofaPaimeanSelect *self, ofoPaimean *paimean )
{
	gboolean ok;

	/* paimean may be %NULL */
	ok = ( paimean != NULL );

	return( ok );
}

static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	return( do_select( OFA_PAIMEAN_SELECT( instance )));
}

static gboolean
do_select( ofaPaimeanSelect *self )
{
	ofaPaimeanSelectPrivate *priv;
	ofoPaimean *paimean;
	gboolean ok;

	priv = ofa_paimean_select_get_instance_private( self );

	paimean = ofa_paimean_frame_bin_get_selected( priv->fbin );
	ok = is_selection_valid( self, paimean );
	if( ok ){
		priv->paimean_code = g_strdup( ofo_paimean_get_code( paimean ));
	}

	return( ok );
}
