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

#include "api/my-idialog.h"
#include "api/my-iwindow.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-ope-template-select.h"
#include "ui/ofa-ope-template-frame-bin.h"

/* private instance data
 */
struct _ofaOpeTemplateSelectPrivate {
	gboolean                dispose_has_run;

	/* initialization
	 */
	ofaHub                 *hub;

	/* UI
	 */
	ofaOpeTemplateFrameBin *ope_templates_frame;
	GtkWidget              *ok_btn;

	/* returned value
	 */
	gchar                  *ope_mnemo;
};

static const gchar          *st_resource_ui = "/org/trychlos/openbook/ui/ofa-ope-template-select.ui";
static ofaOpeTemplateSelect *st_this        = NULL;

static void      iwindow_iface_init( myIWindowInterface *iface );
static void      idialog_iface_init( myIDialogInterface *iface );
static void      idialog_init( myIDialog *instance );
static void      on_ope_template_changed( ofaOpeTemplateFrameBin *piece, const gchar *mnemo, ofaOpeTemplateSelect *self );
static void      on_ope_template_activated( ofaOpeTemplateFrameBin *piece, const gchar *mnemo, ofaOpeTemplateSelect *self );
static void      check_for_enable_dlg( ofaOpeTemplateSelect *self );
static gboolean  idialog_quit_on_ok( myIDialog *instance );
static gboolean  do_select( ofaOpeTemplateSelect *self );
static void      on_hub_finalized( gpointer is_null, gpointer finalized_hub );

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

/**
 * ofa_ope_template_select_run:
 *
 * Returns: the selected operation template mnemo, as a newly allocated
 * string that must be g_free() by the caller
 */
gchar *
ofa_ope_template_select_run( ofaMainWindow *main_window, const gchar *asked_mnemo )
{
	static const gchar *thisfn = "ofa_ope_template_select_run";
	ofaOpeTemplateSelectPrivate *priv;
	gchar *selected_mnemo;

	g_debug( "%s: main_window=%p, asked_mnemo=%s",
			thisfn, ( void * ) main_window, asked_mnemo );

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	if( !st_this ){
		st_this = g_object_new( OFA_TYPE_OPE_TEMPLATE_SELECT, NULL );
		my_iwindow_set_main_window( MY_IWINDOW( st_this ), GTK_APPLICATION_WINDOW( main_window ));

		priv = ofa_ope_template_select_get_instance_private( st_this );

		priv->hub = ofa_main_window_get_hub( OFA_MAIN_WINDOW( main_window ));
		g_return_val_if_fail( priv->hub && OFA_IS_HUB( priv->hub ), NULL );

		my_iwindow_init( MY_IWINDOW( st_this ));
		my_iwindow_set_hide_on_close( MY_IWINDOW( st_this ), TRUE );

		/* setup a weak reference on the hub to auto-unref */
		g_object_weak_ref( G_OBJECT( priv->hub ), ( GWeakNotify ) on_hub_finalized, NULL );
	}

	priv = ofa_ope_template_select_get_instance_private( st_this );

	g_free( priv->ope_mnemo );
	priv->ope_mnemo = NULL;

	selected_mnemo = NULL;
	ofa_ope_template_frame_bin_set_selected( priv->ope_templates_frame, asked_mnemo );
	check_for_enable_dlg( st_this );

	if( my_idialog_run( MY_IDIALOG( st_this )) == GTK_RESPONSE_OK ){
		selected_mnemo = g_strdup( priv->ope_mnemo );
		my_iwindow_close( MY_IWINDOW( st_this ));
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
	GtkApplicationWindow *main_window;
	GtkWidget *parent;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_ope_template_select_get_instance_private( OFA_OPE_TEMPLATE_SELECT( instance ));

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	main_window = my_iwindow_get_main_window( MY_IWINDOW( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ope-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->ope_templates_frame = ofa_ope_template_frame_bin_new( OFA_MAIN_WINDOW( main_window ));
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->ope_templates_frame ));

	g_signal_connect( priv->ope_templates_frame, "ofa-changed", G_CALLBACK( on_ope_template_changed ), instance );
	g_signal_connect( priv->ope_templates_frame, "ofa-activated", G_CALLBACK( on_ope_template_activated ), instance );

	ofa_ope_template_frame_bin_add_button( priv->ope_templates_frame, TEMPLATE_BTN_NEW, TRUE );
	ofa_ope_template_frame_bin_add_button( priv->ope_templates_frame, TEMPLATE_BTN_PROPERTIES, TRUE );
	ofa_ope_template_frame_bin_add_button( priv->ope_templates_frame, TEMPLATE_BTN_DUPLICATE, TRUE );
	ofa_ope_template_frame_bin_add_button( priv->ope_templates_frame, TEMPLATE_BTN_DELETE, TRUE );

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
	gchar *mnemo;
	gboolean ok;

	priv = ofa_ope_template_select_get_instance_private( self );

	mnemo = ofa_ope_template_frame_bin_get_selected( priv->ope_templates_frame );
	ok = my_strlen( mnemo );
	g_free( mnemo );

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
	gchar *mnemo;

	priv = ofa_ope_template_select_get_instance_private( self );

	mnemo = ofa_ope_template_frame_bin_get_selected( priv->ope_templates_frame );
	if( my_strlen( mnemo )){
		g_free( priv->ope_mnemo );
		priv->ope_mnemo = g_strdup( mnemo );
	}
	g_free( mnemo );

	return( TRUE );
}

static void
on_hub_finalized( gpointer is_null, gpointer finalized_hub )
{
	static const gchar *thisfn = "ofa_ope_template_select_on_hub_finalized";

	g_debug( "%s: empty=%p, finalized_hub=%p",
			thisfn, ( void * ) is_null, ( void * ) finalized_hub );

	g_return_if_fail( st_this && OFA_IS_OPE_TEMPLATE_SELECT( st_this ));

	//g_clear_object( &st_this );
	gtk_widget_destroy( GTK_WIDGET( st_this ));
	st_this = NULL;
}
