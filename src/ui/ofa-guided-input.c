/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "ui/ofa-guided-input.h"
#include "ui/ofa-guided-input-bin.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaGuidedInputPrivate {

	/* runtime data
	 */
	const ofaMainWindow  *main_window;
	const ofoOpeTemplate *model;

	/* UI
	 */
	ofaGuidedInputBin    *input_bin;
	GtkWidget            *ok_btn;
};

static const gchar  *st_ui_xml          = PKGUIDIR "/ofa-guided-input.ui";
static const gchar  *st_ui_id           = "GuidedInputDialog";

G_DEFINE_TYPE( ofaGuidedInput, ofa_guided_input, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_input_bin_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedInput *self );
static void      check_for_enable_dlg( ofaGuidedInput *self );
static gboolean  v_quit_on_ok( myDialog *dialog );

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
	g_return_if_fail( instance && OFA_IS_GUIDED_INPUT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_guided_input_parent_class )->dispose( instance );
}

static void
ofa_guided_input_init( ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_GUIDED_INPUT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_GUIDED_INPUT, ofaGuidedInputPrivate );
}

static void
ofa_guided_input_class_init( ofaGuidedInputClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = guided_input_dispose;
	G_OBJECT_CLASS( klass )->finalize = guided_input_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaGuidedInputPrivate ));
}

/**
 * ofa_guided_input_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @model: the #ofoOpeTemplate instance to be used as a template for
 *  the guided input.
 *
 * Let the user enter a new operation based on the @model template.
 */
void
ofa_guided_input_run( const ofaMainWindow *main_window, const ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_input_run";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, model=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new(
					OFA_TYPE_GUIDED_INPUT,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->main_window = main_window;
	self->priv->model = model;

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaGuidedInputPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *parent;

	priv = OFA_GUIDED_INPUT( dialog )->priv;

	toplevel = my_window_get_toplevel( MY_WINDOW( dialog ));
	g_return_if_fail( toplevel && GTK_IS_WINDOW( toplevel ));

	parent = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "piece-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->input_bin = ofa_guided_input_bin_new( priv->main_window );
	gtk_container_add( GTK_CONTAINER( parent ), GTK_WIDGET( priv->input_bin ));
	ofa_guided_input_bin_set_ope_template( priv->input_bin, priv->model );

	g_signal_connect( priv->input_bin, "ofa-changed", G_CALLBACK( on_input_bin_changed ), dialog );

	priv->ok_btn = my_utils_container_get_child_by_name( GTK_CONTAINER( toplevel ), "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	check_for_enable_dlg( OFA_GUIDED_INPUT( dialog ));
}

static void
on_input_bin_changed( ofaGuidedInputBin *bin, gboolean ok, ofaGuidedInput *self )
{
	static const gchar *thisfn = "ofa_guided_input_on_input_bin_changed";
	ofaGuidedInputPrivate *priv;

	priv = self->priv;

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

	priv = self->priv;

	ok = ofa_guided_input_bin_is_valid( priv->input_bin );
	on_input_bin_changed( priv->input_bin, ok, self );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	gboolean ok;

	ok = ofa_guided_input_bin_apply( OFA_GUIDED_INPUT( dialog )->priv->input_bin );

	return( ok );
}
