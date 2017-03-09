/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-guided-input.h"
#include "core/ofa-guided-input-bin.h"

/* private instance data
 */
typedef struct {
	gboolean           dispose_has_run;

	/* initialization
	 */
	ofaIGetter        *getter;
	GtkWindow         *parent;
	ofoOpeTemplate    *model;

	/* runtime
	 */

	/* UI
	 */
	ofaGuidedInputBin *input_bin;
	GtkWidget         *ok_btn;
}
	ofaGuidedInputPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-guided-input.ui";

static void   iwindow_iface_init( myIWindowInterface *iface );
static void   iwindow_init( myIWindow *instance );
static void   idialog_iface_init( myIDialogInterface *iface );
static void   idialog_init( myIDialog *instance );
static void   on_input_bin_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedInput *self );
static void   check_for_enable_dlg( ofaGuidedInput *self );
static void   on_ok_clicked( ofaGuidedInput *self );

G_DEFINE_TYPE_EXTENDED( ofaGuidedInput, ofa_guided_input, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaGuidedInput )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
guided_input_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_guided_input_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_parent_class )->finalize( instance );
}

static void
guided_input_dispose( GObject *instance )
{
	ofaGuidedInputPrivate *priv;

	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT( instance ));

	priv = ofa_guided_input_get_instance_private( OFA_GUIDED_INPUT( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_parent_class )->dispose( instance );
}

static void
ofa_guided_input_init( ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_init";
	ofaGuidedInputPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_GUIDED_INPUT( self ));

	priv = ofa_guided_input_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_guided_input_class_init( ofaGuidedInputClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_input_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_input_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_guided_input_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent of this dialog.
 * @model: the #ofoOpeTemplate instance to be used as a template for
 *  the guided input.
 *
 * Let the user enter a new operation based on the @model template.
 */
void
ofa_guided_input_run( ofaIGetter *getter, GtkWindow *parent, ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_input_run";
	ofaGuidedInput *self;
	ofaGuidedInputPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, model=%p",
			thisfn, ( void * ) getter, ( void * ) parent, ( void * ) model );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_GUIDED_INPUT, NULL );

	priv = ofa_guided_input_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->model = model;

	/* after this call, @self may be invalid */
	my_iwindow_present( MY_IWINDOW( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_guided_input_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_guided_input_iwindow_init";
	ofaGuidedInputPrivate *priv;
	gchar *id;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_guided_input_get_instance_private( OFA_GUIDED_INPUT( instance ));

	my_iwindow_set_parent( instance, priv->parent );
	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));

	id = g_strdup_printf( "%s-%s",
				G_OBJECT_TYPE_NAME( instance ), ofo_ope_template_get_mnemo( priv->model ));
	my_iwindow_set_identifier( instance, id );
	g_free( id );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_guided_input_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_guided_input_idialog_init";
	ofaGuidedInputPrivate *priv;
	GtkWidget *btn, *parent;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_guided_input_get_instance_private( OFA_GUIDED_INPUT( instance ));

	/* validate and record the operation on OK + always terminates */
	btn = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "ok-btn" );
	g_return_if_fail( btn && GTK_IS_BUTTON( btn ));
	g_signal_connect_swapped( btn, "clicked", G_CALLBACK( on_ok_clicked ), instance );
	priv->ok_btn = btn;

	my_utils_container_dump( GTK_CONTAINER( instance ));
	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "bin-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->input_bin = ofa_guided_input_bin_new( priv->getter );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->input_bin ));
	ofa_guided_input_bin_set_ope_template( priv->input_bin, priv->model );

	g_signal_connect( priv->input_bin, "ofa-changed", G_CALLBACK( on_input_bin_changed ), instance );

	check_for_enable_dlg( OFA_GUIDED_INPUT( instance ));
}

static void
on_input_bin_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_on_input_bin_changed";
	ofaGuidedInputPrivate *priv;

	priv = ofa_guided_input_get_instance_private( self );

	g_debug( "%s: ok=%s", thisfn, ok ? "True":"False" );

	if( priv->ok_btn ){
		gtk_widget_set_sensitive( priv->ok_btn, ok );
	}
}

static void
check_for_enable_dlg( ofaGuidedInput *self )
{
	ofaGuidedInputPrivate *priv;
	gboolean ok;

	priv = ofa_guided_input_get_instance_private( self );

	ok = ofa_guided_input_bin_is_valid( priv->input_bin );
	on_input_bin_changed( priv->input_bin, ok, self );
}

static void
on_ok_clicked( ofaGuidedInput *self )
{
	ofaGuidedInputPrivate *priv;
	gboolean ok;

	priv = ofa_guided_input_get_instance_private( self );

	ok = ofa_guided_input_bin_apply( priv->input_bin );

	if( !ok ){
		my_utils_msg_dialog( GTK_WINDOW( self ), GTK_MESSAGE_WARNING, _( "Unable to create the required entries" ));
	}

	my_iwindow_close( MY_IWINDOW( self ));
}
