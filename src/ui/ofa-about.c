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

#include "ui/ofa-about.h"
#include "ui/ofa-application.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaAboutPrivate {
	gboolean dispose_has_run;
};

static const gchar *st_icon_fname       = ICONFNAME;

G_DEFINE_TYPE( ofaAbout, ofa_about, GTK_TYPE_ABOUT_DIALOG )

static void on_close_clicked( GtkButton *button, ofaAbout *about );

static void
about_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_about_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ABOUT( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_about_parent_class )->finalize( instance );
}

static void
about_dispose( GObject *instance )
{
	ofaAboutPrivate *priv;

	g_return_if_fail( instance && OFA_IS_ABOUT( instance ));

	priv = ( OFA_ABOUT( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_about_parent_class )->dispose( instance );
}

static void
ofa_about_init( ofaAbout *self )
{
	static const gchar *thisfn = "ofa_about_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ABOUT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_ABOUT, ofaAboutPrivate );

	self->priv->dispose_has_run = FALSE;
}

static void
ofa_about_class_init( ofaAboutClass *klass )
{
	static const gchar *thisfn = "ofa_about_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = about_dispose;
	G_OBJECT_CLASS( klass )->finalize = about_finalize;

	g_type_class_add_private( klass, sizeof( ofaAboutPrivate ));
}

/**
 * ofa_about_run:
 * @main_window: the main window.
 *
 * Display the About... dialog box.
 */
void
ofa_about_run( ofaMainWindow *main_window )
{
	GtkApplication *application;
	ofaAbout *about;
	GdkPixbuf *pixbuf;
	GtkWidget *button;

	application = gtk_window_get_application( GTK_WINDOW( main_window ));
	g_return_if_fail( application && OFA_IS_APPLICATION( application ));

	static const gchar *authors[] = {
		"Pierre Wieser <pwieser@trychlos.org>",
		NULL
	};

	about = g_object_new(
			OFA_TYPE_ABOUT,
			"authors", authors,
			"comments", _( "A double-entry, multi-currencies, accounting software.\n"
							"Primarily designed with french rules in mind, adapted "
							"to several european countries." ),
			"copyright", ofa_application_get_copyright( OFA_APPLICATION( application )),
			"license-type", GTK_LICENSE_GPL_3_0,
			"version", PACKAGE_VERSION,
			"website", "http://trychlos.github.io/openbook/",
			NULL );

	/*
	gtk_about_dialog_set_artists( GTK_ABOUT_DIALOG( about ),  );
	gtk_about_dialog_set_documenters( GTK_ABOUT_DIALOG( about ),  );
	*/

	pixbuf = gdk_pixbuf_new_from_file( st_icon_fname, NULL );
	gtk_about_dialog_set_logo( GTK_ABOUT_DIALOG( about ), pixbuf );
	g_object_unref( pixbuf );

	button = gtk_dialog_get_widget_for_response( GTK_DIALOG( about ), GTK_RESPONSE_CANCEL );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_close_clicked ), about );

	gtk_dialog_run( GTK_DIALOG( about ));
}

static void
on_close_clicked( GtkButton *button, ofaAbout *about )
{
	gtk_widget_destroy( GTK_WIDGET( about ));
}
