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

#include <glib/gi18n.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"

#include "core/my-window-prot.h"

#include "ui/ofa-guided-common.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaGuidedInputPrivate {

	/* internals
	 */
	const ofoOpeTemplate  *model;
	ofaGuidedCommon       *common;
};

static const gchar  *st_ui_xml    = PKGUIDIR "/ofa-guided-input.ui";
static const gchar  *st_ui_id     = "GuidedInputDlg";

G_DEFINE_TYPE( ofaGuidedInput, ofa_guided_input, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_ok_clicked( GtkButton *button, ofaGuidedInput *self );
static void      on_cancel_clicked( GtkButton *button, ofaGuidedInput *self );

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

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_GUIDED_INPUT( instance )->priv;
		if( priv->common ){
			g_clear_object( &priv->common );
		}
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

	g_type_class_add_private( klass, sizeof( ofaGuidedInputPrivate ));
}

/**
 * ofa_guided_input_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
void
ofa_guided_input_run( ofaMainWindow *main_window, const ofoOpeTemplate *model )
{
	static const gchar *thisfn = "ofa_guided_input_run";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, model=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new(
					OFA_TYPE_GUIDED_INPUT,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->model = model;

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaGuidedInputPrivate *priv;
	GtkWidget *button;
	GtkContainer *container;

	priv = OFA_GUIDED_INPUT( dialog )->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	priv->common = ofa_guided_common_new(
							MY_WINDOW( dialog )->prot->main_window,
							container );

	ofa_guided_common_set_model( priv->common, priv->model );

	button = my_utils_container_get_child_by_name( container, "box-ok" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_ok_clicked ), dialog );

	button = my_utils_container_get_child_by_name( container, "box-cancel" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_cancel_clicked ), dialog );
}

static void
on_ok_clicked( GtkButton *button, ofaGuidedInput *self )
{
	if( ofa_guided_common_validate( self->priv->common )){
		gtk_dialog_response(
				GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
				GTK_RESPONSE_OK );
	}
}

static void
on_cancel_clicked( GtkButton *button, ofaGuidedInput *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_CANCEL );
}
