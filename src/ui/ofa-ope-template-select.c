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

#include "api/my-utils.h"
#include "api/ofo-account.h"

#include "core/my-window-prot.h"

#include "ui/ofa-ope-template-select.h"
#include "ui/ofa-ope-templates-frame.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaOpeTemplateSelectPrivate {

	/* UI
	 */
	ofaOpeTemplatesFrame *ope_templates_frame;

	GtkWidget            *ok_btn;

	/* returned value
	 */
	gchar                *ope_mnemo;
};

static const gchar          *st_ui_xml   = PKGUIDIR "/ofa-ope-template-select.ui";
static const gchar          *st_ui_id    = "OpeTemplateSelectDialog";

static ofaOpeTemplateSelect *st_this     = NULL;
static GtkWindow            *st_toplevel = NULL;

G_DEFINE_TYPE( ofaOpeTemplateSelect, ofa_ope_template_select, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      on_ope_template_changed( ofaOpeTemplatesFrame *piece, const gchar *mnemo, ofaOpeTemplateSelect *self );
static void      on_ope_template_activated( ofaOpeTemplatesFrame *piece, const gchar *mnemo, ofaOpeTemplateSelect *self );
static void      check_for_enable_dlg( ofaOpeTemplateSelect *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_select( ofaOpeTemplateSelect *self );

static void
ope_template_select_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_select_finalize";
	ofaOpeTemplateSelectPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_SELECT( instance ));

	/* free data members here */
	priv = OFA_OPE_TEMPLATE_SELECT( instance )->priv;

	g_free( priv->ope_mnemo );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_select_parent_class )->finalize( instance );
}

static void
ope_template_select_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_SELECT( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_select_parent_class )->dispose( instance );
}

static void
ofa_ope_template_select_init( ofaOpeTemplateSelect *self )
{
	static const gchar *thisfn = "ofa_ope_template_select_init";

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_SELECT( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_OPE_TEMPLATE_SELECT, ofaOpeTemplateSelectPrivate );
}

static void
ofa_ope_template_select_class_init( ofaOpeTemplateSelectClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_select_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_select_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_select_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaOpeTemplateSelectPrivate ));
}

static void
on_dossier_finalized( gpointer is_null, gpointer finalized_dossier )
{
	static const gchar *thisfn = "ofa_ope_template_select_on_dossier_finalized";

	g_debug( "%s: empty=%p, finalized_dossier=%p",
			thisfn, ( void * ) is_null, ( void * ) finalized_dossier );

	g_return_if_fail( st_this && OFA_IS_OPE_TEMPLATE_SELECT( st_this ));

	g_clear_object( &st_this );
}

/**
 * ofa_ope_template_select_run:
 *
 * Returns the selected operation template mnemo, as a newly allocated
 * string that must be g_free() by the caller
 */
gchar *
ofa_ope_template_select_run( ofaMainWindow *main_window, const gchar *asked_mnemo )
{
	static const gchar *thisfn = "ofa_ope_template_select_run";
	ofaOpeTemplateSelectPrivate *priv;
	ofoDossier *dossier;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), NULL );

	g_debug( "%s: main_window=%p, asked_mnemo=%s",
			thisfn, ( void * ) main_window, asked_mnemo );

	if( !st_this ){
		dossier = ofa_main_window_get_dossier( main_window );
		st_this = g_object_new(
				OFA_TYPE_OPE_TEMPLATE_SELECT,
				MY_PROP_MAIN_WINDOW,   main_window,
				MY_PROP_DOSSIER,       dossier,
				MY_PROP_WINDOW_XML,    st_ui_xml,
				MY_PROP_WINDOW_NAME,   st_ui_id,
				MY_PROP_SIZE_POSITION, FALSE,
				NULL );

		st_toplevel = my_window_get_toplevel( MY_WINDOW( st_this ));
		my_utils_window_restore_position( st_toplevel, st_ui_id );
		my_dialog_init_dialog( MY_DIALOG( st_this ));

		/* setup a weak reference on the dossier to auto-unref */
		g_object_weak_ref( G_OBJECT( dossier ), ( GWeakNotify ) on_dossier_finalized, NULL );
	}

	priv = st_this->priv;

	g_free( priv->ope_mnemo );
	priv->ope_mnemo = NULL;

	ofa_ope_templates_frame_set_selected( priv->ope_templates_frame, asked_mnemo );
	check_for_enable_dlg( st_this );

	my_dialog_run_dialog( MY_DIALOG( st_this ));

	my_utils_window_save_position( st_toplevel, st_ui_id );
	gtk_widget_hide( GTK_WIDGET( st_toplevel ));

	return( g_strdup( priv->ope_mnemo ));
}

static void
v_init_dialog( myDialog *dialog )
{
	static const gchar *thisfn = "ofa_ope_template_select_v_init_dialog";
	ofaOpeTemplateSelectPrivate *priv;
	GtkContainer *container;
	GtkWidget *parent;

	g_debug( "%s: dialog=%p", thisfn, ( void * ) dialog );

	priv = OFA_OPE_TEMPLATE_SELECT( dialog )->priv;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( st_this )));

	priv->ok_btn = my_utils_container_get_child_by_name( container, "btn-ok" );

	parent = my_utils_container_get_child_by_name( container, "ope-parent" );
	g_return_if_fail( parent && GTK_IS_CONTAINER( parent ));

	priv->ope_templates_frame = ofa_ope_templates_frame_new();
	ofa_ope_templates_frame_attach_to( priv->ope_templates_frame, GTK_CONTAINER( parent ));
	ofa_ope_templates_frame_set_main_window( priv->ope_templates_frame, MY_WINDOW( dialog )->prot->main_window );
	ofa_ope_templates_frame_set_buttons( priv->ope_templates_frame, FALSE );

	g_signal_connect(
			G_OBJECT( priv->ope_templates_frame ), "changed", G_CALLBACK( on_ope_template_changed ), dialog );
	g_signal_connect(
			G_OBJECT( priv->ope_templates_frame ), "activated", G_CALLBACK( on_ope_template_activated ), dialog );
}

static void
on_ope_template_changed( ofaOpeTemplatesFrame *piece, const gchar *mnemo, ofaOpeTemplateSelect *self )
{
	check_for_enable_dlg( self );
}

static void
on_ope_template_activated( ofaOpeTemplatesFrame *piece, const gchar *mnemo, ofaOpeTemplateSelect *self )
{
	gtk_dialog_response(
			GTK_DIALOG( my_window_get_toplevel( MY_WINDOW( self ))),
			GTK_RESPONSE_OK );
}

static void
check_for_enable_dlg( ofaOpeTemplateSelect *self )
{
	ofaOpeTemplateSelectPrivate *priv;
	gchar *mnemo;
	gboolean ok;

	priv = self->priv;

	mnemo = ofa_ope_templates_frame_get_selected( priv->ope_templates_frame );
	ok = mnemo && g_utf8_strlen( mnemo, -1 );
	g_free( mnemo );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_select( OFA_OPE_TEMPLATE_SELECT( dialog )));
}

static gboolean
do_select( ofaOpeTemplateSelect *self )
{
	ofaOpeTemplateSelectPrivate *priv;
	gchar *mnemo;

	priv = self->priv;

	mnemo = ofa_ope_templates_frame_get_selected( priv->ope_templates_frame );
	if( mnemo && g_utf8_strlen( mnemo, -1 )){
		priv->ope_mnemo = g_strdup( mnemo );
	}
	g_free( mnemo );

	return( TRUE );
}
