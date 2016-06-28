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
#include "my/my-utils.h"

#include "api/ofa-hub.h"

#include "tva/ofa-tva-style.h"

/* private instance data
 */
typedef struct {
	gboolean    dispose_has_run;

	/*
	 */
}
	ofaTVAStylePrivate;

static const gchar *st_style_css        = "/org/trychlos/openbook/tva/ofa-tva.css";

G_DEFINE_TYPE_EXTENDED( ofaTVAStyle, ofa_tva_style, GTK_TYPE_CSS_PROVIDER, 0,
		G_ADD_PRIVATE( ofaTVAStyle ))

static void
tva_style_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_style_finalize";

	g_debug( "%s: application=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_STYLE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_style_parent_class )->finalize( instance );
}

static void
tva_style_dispose( GObject *instance )
{
	ofaTVAStylePrivate *priv;

	g_return_if_fail( instance && OFA_IS_TVA_STYLE( instance ));

	priv = ofa_tva_style_get_instance_private( OFA_TVA_STYLE( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_style_parent_class )->dispose( instance );
}

static void
ofa_tva_style_init( ofaTVAStyle *self )
{
	static const gchar *thisfn = "ofa_tva_style_init";
	ofaTVAStylePrivate *priv;

	g_return_if_fail( OFA_IS_TVA_STYLE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofa_tva_style_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_tva_style_class_init( ofaTVAStyleClass *klass )
{
	static const gchar *thisfn = "ofa_tva_style_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_style_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_style_finalize;
}

/**
 * ofa_tva_style_new:
 * @hub: the #ofaHub object of the application.
 *
 * Instanciates a new #ofaTVAStyle and attached it to the @hub
 * if not already done. Else get the already allocated #ofaTVAStyle
 * from the @hub.
 */
ofaTVAStyle *
ofa_tva_style_new( ofaHub *hub )
{
	ofaTVAStyle *provider;
	myICollector *collector;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	collector = ofa_hub_get_collector( hub );
	provider = ( ofaTVAStyle * ) my_icollector_single_get_object( collector, OFA_TYPE_TVA_STYLE );

	if( provider ){
		g_return_val_if_fail( OFA_IS_TVA_STYLE( provider ), NULL );

	} else {
		provider = g_object_new( OFA_TYPE_TVA_STYLE, NULL );

		gtk_css_provider_load_from_resource( GTK_CSS_PROVIDER( provider ), st_style_css );

		my_icollector_single_set_object( collector, provider );
	}

	return( provider );
}

/**
 * ofa_tva_style_set_style:
 * @provider: this #ofaTVAStyle instance.
 * @widget: the #GtkWidget.
 * @style: the style to be set.
 *
 * Set the @style on the @widget.
 */
void
ofa_tva_style_set_style( ofaTVAStyle *provider, GtkWidget *widget, const gchar *style )
{
	static const gchar *thisfn = "ofa_tva_style_set_style";
	ofaTVAStylePrivate *priv;
	GtkStyleContext *context;
	const GtkWidgetPath *path;
	gchar *path_str;

	g_debug( "%s: provider=%p, widget=%p, style=%s",
			thisfn, ( void * ) provider, ( void * ) widget, style );

	g_return_if_fail( provider && OFA_IS_TVA_STYLE( provider ));
	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));
	g_return_if_fail( my_strlen( style ));

	priv = ofa_tva_style_get_instance_private( provider );

	g_return_if_fail( !priv->dispose_has_run );

	context = gtk_widget_get_style_context( widget );

	gtk_style_context_add_provider( context,
			GTK_STYLE_PROVIDER( provider ),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

	gtk_style_context_add_class( context, style );

	if( 0 ){
		path = gtk_style_context_get_path( context );
		path_str = gtk_widget_path_to_string( path );
		g_debug( "%s: path=%s", thisfn, path_str );
		//path=ofaTVARecordProperties:dir-ltr.background GtkBox:dir-ltr.vertical.dialog-vbox GtkGrid:dir-ltr[1/2].horizontal GtkNotebook:dir-ltr.notebook GtkScrolledWindow:dir-ltr GtkViewport:dir-ltr GtkGrid:dir-ltr.horizontal GtkLabel:dir-ltr.label.vat-label3
	}
}
