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

#include "my/my-icollector.h"
#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-ope-template-select.h"
#include "core/ofa-ope-template-frame-bin.h"

/* private instance data
 */
typedef struct {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaIGetter             *getter;
	gchar                  *settings_prefix;

	/* UI
	 */
	ofaOpeTemplateFrameBin *template_bin;
	GtkWidget              *ok_btn;

	/* returned value
	 */
	gchar                  *ope_mnemo;
}
	ofaOpeTemplateSelectPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-ope-template-select.ui";

static ofaOpeTemplateSelect *ope_template_select_new( ofaIGetter *getter, GtkWindow *parent );
static void                  iwindow_iface_init( myIWindowInterface *iface );
static gboolean              iwindow_is_destroy_allowed( const myIWindow *instance );
static void                  idialog_iface_init( myIDialogInterface *iface );
static void                  idialog_init( myIDialog *instance );
static void                  on_ope_template_changed( ofaOpeTemplateFrameBin *piece, const gchar *mnemo, ofaOpeTemplateSelect *self );
static void                  on_ope_template_activated( ofaOpeTemplateFrameBin *piece, const gchar *mnemo, ofaOpeTemplateSelect *self );
static void                  check_for_enable_dlg( ofaOpeTemplateSelect *self );
static gboolean              idialog_quit_on_ok( myIDialog *instance );
static gboolean              do_select( ofaOpeTemplateSelect *self );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateSelect, ofa_ope_template_select, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaOpeTemplateSelect )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
ope_template_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_select_finalize";
	ofaOpeTemplateSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_SELECT( instance ));

	/* free data members here */
	priv = ofa_ope_template_select_get_instance_private( OFA_OPE_TEMPLATE_SELECT( instance ));

	g_free( priv->settings_prefix );
	g_free( priv->ope_mnemo );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_select_parent_class )->finalize( instance );
}

static void
ope_template_select_dispose( GObject *instance )
{
	ofaOpeTemplateSelectPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_SELECT( instance ));

	priv = ofa_ope_template_select_get_instance_private( OFA_OPE_TEMPLATE_SELECT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_select_parent_class )->dispose( instance );
}

static void
ofa_ope_template_select_init( ofaOpeTemplateSelect *self )
{
	static const gchar *thisfn = "ofa_ope_template_select_init";
	ofaOpeTemplateSelectPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_SELECT( self ));

	priv = ofa_ope_template_select_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_ope_template_select_class_init( ofaOpeTemplateSelectClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_select_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/*
 * Returns: the unique #ofaOpeTemplateSelect instance.
 */
static ofaOpeTemplateSelect *
ope_template_select_new( ofaIGetter *getter, GtkWindow *parent )
{
	ofaOpeTemplateSelect *dialog;
	ofaOpeTemplateSelectPrivate *priv;
	ofaHub *hub;
	myICollector *collector;

	hub = ofa_igetter_get_hub( getter );
	collector = ofa_hub_get_collector( hub );

	dialog = ( ofaOpeTemplateSelect * ) my_icollector_single_get_object( collector, OFA_TYPE_OPE_TEMPLATE_SELECT );

	if( !dialog ){
		dialog = g_object_new( OFA_TYPE_OPE_TEMPLATE_SELECT, NULL );
		my_iwindow_set_parent( MY_IWINDOW( dialog ), parent );
		my_iwindow_set_settings( MY_IWINDOW( dialog ), ofa_settings_get_settings( SETTINGS_TARGET_USER ));

		/* setup a permanent getter before initialization */
		priv = ofa_ope_template_select_get_instance_private( dialog );
		priv->getter = ofa_igetter_get_permanent_getter( getter );
		my_iwindow_init( MY_IWINDOW( dialog ));

		/* and record this unique object */
		my_icollector_single_set_object( collector, dialog );
	}

	return( dialog );
}

/**
 * ofa_ope_template_select_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the parent window.
 * @asked_mnemo: [allow-none]: the initially selected operation template identifier.
 *
 * Returns: the selected operation template mnemo, as a newly allocated
 * string that must be g_free() by the caller
 */
gchar *
ofa_ope_template_select_run( ofaIGetter *getter, GtkWindow *parent, const gchar *asked_mnemo )
{
	static const gchar *thisfn = "ofa_ope_template_select_run";
	ofaOpeTemplateSelect *dialog;
	ofaOpeTemplateSelectPrivate *priv;
	gchar *selected_mnemo;

	g_debug( "%s: getter=%p, parent=%p, asked_mnemo=%s",
			thisfn, ( void * ) getter, ( void * ) parent, asked_mnemo );

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );
	g_return_val_if_fail( !parent || GTK_IS_WINDOW( parent ), NULL );

	dialog = ope_template_select_new( getter, parent );
	priv = ofa_ope_template_select_get_instance_private( dialog );

	g_free( priv->ope_mnemo );
	priv->ope_mnemo = NULL;

	selected_mnemo = NULL;
	ofa_ope_template_frame_bin_set_selected( priv->template_bin, asked_mnemo );
	check_for_enable_dlg( dialog );

	if( my_idialog_run( MY_IDIALOG( dialog )) == GTK_RESPONSE_OK ){
		selected_mnemo = g_strdup( priv->ope_mnemo );
		my_iwindow_close( MY_IWINDOW( dialog ));
	}

	return( selected_mnemo );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_select_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->is_destroy_allowed = iwindow_is_destroy_allowed;
}

/*
 * ofaOpeTemplateSelect is not destroyed at end, but only hidden.
 */
static gboolean
iwindow_is_destroy_allowed( const myIWindow *instance )
{
	ofaOpeTemplateSelectPrivate *priv;
	GtkWidget *page;

	priv = ofa_ope_template_select_get_instance_private( OFA_OPE_TEMPLATE_SELECT( instance ));

	/* save the settings before hiding */
	page = ofa_ope_template_frame_bin_get_current_page( priv->template_bin );
	ofa_itvcolumnable_write_columns_settings( OFA_ITVCOLUMNABLE( page ));

	return( FALSE );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_select_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
	iface->quit_on_ok = idialog_quit_on_ok;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_ope_template_select_idialog_init";
	ofaOpeTemplateSelectPrivate *priv;
	GtkWidget *parent;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ope_template_select_get_instance_private( OFA_OPE_TEMPLATE_SELECT( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "bin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->template_bin = ofa_ope_template_frame_bin_new();
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->template_bin ));
	ofa_ope_template_frame_bin_set_settings_key( priv->template_bin, priv->settings_prefix );

	g_signal_connect( priv->template_bin, "ofa-changed", G_CALLBACK( on_ope_template_changed ), instance );
	g_signal_connect( priv->template_bin, "ofa-activated", G_CALLBACK( on_ope_template_activated ), instance );

	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_NEW );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_PROPERTIES );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_DUPLICATE );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_DELETE );

	ofa_ope_template_frame_bin_set_getter( priv->template_bin, priv->getter );

	gtk_widget_show_all( GTK_WIDGET( instance ));
}

static void
on_ope_template_changed( ofaOpeTemplateFrameBin *piece, const gchar *mnemo, ofaOpeTemplateSelect *self )
{
	check_for_enable_dlg( self );
}

static void
on_ope_template_activated( ofaOpeTemplateFrameBin *piece, const gchar *mnemo, ofaOpeTemplateSelect *self )
{
	gtk_dialog_response( GTK_DIALOG( self ), GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaOpeTemplateSelect *self )
{
	ofaOpeTemplateSelectPrivate *priv;
	ofoOpeTemplate *template;
	gboolean ok;

	priv = ofa_ope_template_select_get_instance_private( self );

	template = ofa_ope_template_frame_bin_get_selected( priv->template_bin );
	ok = template && OFO_IS_OPE_TEMPLATE( template );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
idialog_quit_on_ok( myIDialog *instance )
{
	return( do_select( OFA_OPE_TEMPLATE_SELECT( instance )));
}

static gboolean
do_select( ofaOpeTemplateSelect *self )
{
	ofaOpeTemplateSelectPrivate *priv;
	ofoOpeTemplate *template;

	priv = ofa_ope_template_select_get_instance_private( self );

	template = ofa_ope_template_frame_bin_get_selected( priv->template_bin );
	if( template ){
		g_free( priv->ope_mnemo );
		priv->ope_mnemo = g_strdup( ofo_ope_template_get_mnemo( template ));
	}

	return( TRUE );
}
