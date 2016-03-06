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

#include "core/ofa-main-window.h"

#include "ui/ofa-ope-template-help.h"

/* private instance data
 *
 * The instance is make unique because of:
 * 1/ non-modal
 * 2/ managed through myIWindow interface
 * 3/ does not provide any other identifier than standard type name.
 */
struct _ofaOpeTemplateHelpPrivate {
	gboolean dispose_has_run;

	/* initialization
	 */

	/* runtime
	 */
	GList   *parents;
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-ope-template-help.ui";

static void     iwindow_iface_init( myIWindowInterface *iface );
static void     iwindow_init( myIWindow *instance );
static void     idialog_iface_init( myIDialogInterface *iface );
static void     add_parent( ofaOpeTemplateHelp *self, GtkWindow *parent );
static void     on_parent_finalized( ofaOpeTemplateHelp *self, GObject *finalized_parent );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplateHelp, ofa_ope_template_help, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaOpeTemplateHelp )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
ope_template_help_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_help_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_HELP( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_help_parent_class )->finalize( instance );
}

static void
ope_template_help_dispose( GObject *instance )
{
	ofaOpeTemplateHelpPrivate *priv;
	GList *it;

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_HELP( instance ));

	priv = ofa_ope_template_help_get_instance_private( OFA_OPE_TEMPLATE_HELP( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */

		/* remove the weak reference callback on the still-alive parents */
		for( it=priv->parents ; it ; it=it->next ){
			g_object_weak_unref( G_OBJECT( it->data ), ( GWeakNotify ) on_parent_finalized, instance );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_help_parent_class )->dispose( instance );
}

static void
ofa_ope_template_help_init( ofaOpeTemplateHelp *self )
{
	static const gchar *thisfn = "ofa_ope_template_help_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_HELP( self ));

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_ope_template_help_class_init( ofaOpeTemplateHelpClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_help_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_help_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_help_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_ope_template_help_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @parent: the #GtkWindow (usually a #ofaOpeTemplateProperties) parent.
 *
 * Creates if needed and presents this OpeTemplate help dialog.
 * If not explicitely closed by the user, it will automatically auto-
 * close itself on last parent finalization.
 */
void
ofa_ope_template_help_run( const ofaMainWindow *main_window, GtkWindow *parent )
{
	ofaOpeTemplateHelp *self;
	myIWindow *shown;

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( parent && GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_OPE_TEMPLATE_HELP, NULL );
	my_iwindow_set_main_window( MY_IWINDOW( self ), GTK_APPLICATION_WINDOW( main_window ));

	/* after this call, @self may be invalid */
	shown = my_iwindow_present( MY_IWINDOW( self ));

	add_parent( OFA_OPE_TEMPLATE_HELP( shown ), parent );
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_help_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	GtkWidget *button;

	my_idialog_init_dialog( MY_IDIALOG( instance ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "close-btn" );
	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	my_idialog_widget_click_to_close( MY_IDIALOG( instance ), button );
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_ope_template_help_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );
}

/*
 * records the parent
 * this dialog will auto-close on last parent finalization
 */
static void
add_parent( ofaOpeTemplateHelp *self, GtkWindow *parent )
{
	ofaOpeTemplateHelpPrivate *priv;
	GList *find;

	priv = ofa_ope_template_help_get_instance_private( self );

	find = g_list_find( priv->parents, parent );
	if( !find ){
		priv->parents = g_list_prepend( priv->parents, parent );
		g_object_weak_ref( G_OBJECT( parent ), ( GWeakNotify ) on_parent_finalized, self );
	}
}

static void
on_parent_finalized( ofaOpeTemplateHelp *self, GObject *finalized_parent )
{
	static const gchar *thisfn = "ofa_ope_template_help_on_parent_finalized";
	ofaOpeTemplateHelpPrivate *priv;

	g_debug( "%s: self=%p, finalized_parent=%p",
			thisfn, ( void * ) self, ( void * ) finalized_parent );

	priv = ofa_ope_template_help_get_instance_private( self );

	priv->parents = g_list_remove( priv->parents, finalized_parent );

	if( !priv->parents || g_list_length( priv->parents ) == 0 ){
		my_iwindow_close( MY_IWINDOW( self ));
	}
}
