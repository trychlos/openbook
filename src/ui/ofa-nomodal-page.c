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
#include "api/my-utils.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-nomodal-page.h"

/* private instance data
 */
struct _ofaNomodalPagePrivate {
	gboolean   dispose_has_run;

	/* initialization
	 */
	gchar     *identifier;
	GtkWidget *top_widget;
};

static void      idialog_iface_init( myIDialogInterface *iface );
static guint     idialog_get_interface_version( const myIDialog *instance );
static gchar    *idialog_get_identifier( const myIDialog *instance );
static void      idialog_init( myIDialog *instance );

G_DEFINE_TYPE_EXTENDED( ofaNomodalPage, ofa_nomodal_page, GTK_TYPE_WINDOW, 0, \
		G_ADD_PRIVATE( ofaNomodalPage ) \
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ));

static void
nomodal_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_nomodal_page_finalize";
	ofaNomodalPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_NOMODAL_PAGE( instance ));

	/* free data members here */
	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	g_free( priv->identifier );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_nomodal_page_parent_class )->finalize( instance );
}

static void
nomodal_page_dispose( GObject *instance )
{
	ofaNomodalPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_NOMODAL_PAGE( instance ));

	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_nomodal_page_parent_class )->dispose( instance );
}

static void
ofa_nomodal_page_init( ofaNomodalPage *self )
{
	static const gchar *thisfn = "ofa_nomodal_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_NOMODAL_PAGE( self ));
}

static void
ofa_nomodal_page_class_init( ofaNomodalPageClass *klass )
{
	static const gchar *thisfn = "ofa_nomodal_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = nomodal_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = nomodal_page_finalize;
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_nomodal_page_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = idialog_get_interface_version;
	iface->get_identifier = idialog_get_identifier;
	iface->init = idialog_init;
}

static guint
idialog_get_interface_version( const myIDialog *instance )
{
	return( 1 );
}

static gchar *
idialog_get_identifier( const myIDialog *instance )
{
	ofaNomodalPagePrivate *priv;

	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	return( g_strdup( priv->identifier ));
}

static void
idialog_init( myIDialog *instance )
{
	ofaNomodalPagePrivate *priv;

	priv = ofa_nomodal_page_get_instance_private( OFA_NOMODAL_PAGE( instance ));

	gtk_window_set_title( GTK_WINDOW( instance ), priv->identifier );
	gtk_window_set_resizable( GTK_WINDOW( instance ), TRUE );
	gtk_window_set_modal( GTK_WINDOW( instance ), FALSE );

	gtk_container_add( GTK_CONTAINER( instance ), priv->top_widget );
}

/**
 * ofa_nomodal_page_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @identifier: the identifier which makes this window unique.
 * @page: the #GtkWidget top widget.
 *
 * Creates or represents a #ofaNomodalPage non-modal window.
 */
void
ofa_nomodal_page_run(
		const ofaMainWindow *main_window, const gchar *identifier, GtkWidget *page )
{
	static const gchar *thisfn = "ofa_nomodal_page_run";
	ofaNomodalPage *self;
	ofaNomodalPagePrivate *priv;

	g_debug( "%s: main_window=%p, identifier=%s, page=%p",
			thisfn, ( void * ) main_window, identifier, ( void * ) page );

	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( my_strlen( identifier ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	self = g_object_new( OFA_TYPE_NOMODAL_PAGE, NULL );
	my_idialog_set_main_window( MY_IDIALOG( self ), GTK_APPLICATION_WINDOW( main_window ));

	priv = ofa_nomodal_page_get_instance_private( self );
	priv->identifier = g_strdup( identifier );
	priv->top_widget = page;

	/* after this call, @self may be invalid */
	my_idialog_present( MY_IDIALOG( self ));
}
