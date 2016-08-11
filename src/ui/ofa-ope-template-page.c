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

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-ledger.h"

#include "core/ofa-ope-template-properties.h"
#include "core/ofa-ope-template-frame-bin.h"

#include "ui/ofa-ope-template-page.h"

/* private instance data
 */
typedef struct {

	/* UI
	 */
	ofaOpeTemplateFrameBin *template_bin;
}
	ofaOpeTemplatePagePrivate;

static void       v_setup_page( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_activated( ofaOpeTemplateFrameBin *frame, const gchar *mnemo, ofaOpeTemplatePage *self );

G_DEFINE_TYPE_EXTENDED( ofaOpeTemplatePage, ofa_ope_template_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaOpeTemplatePage ))

static void
ope_template_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ope_template_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_page_parent_class )->finalize( instance );
}

static void
ope_template_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_OPE_TEMPLATE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ope_template_page_parent_class )->dispose( instance );
}

static void
ofa_ope_template_page_init( ofaOpeTemplatePage *self )
{
	static const gchar *thisfn = "ofa_ope_template_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPE_TEMPLATE_PAGE( self ));
}

static void
ofa_ope_template_page_class_init( ofaOpeTemplatePageClass *klass )
{
	static const gchar *thisfn = "ofa_ope_template_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ope_template_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = ope_template_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_page = v_setup_page;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;
}

static void
v_setup_page( ofaPage *page )
{
	static const gchar *thisfn = "ofa_ope_template_page_v_setup_page";
	ofaOpeTemplatePagePrivate *priv;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_ope_template_page_get_instance_private( OFA_OPE_TEMPLATE_PAGE( page ));

	priv->template_bin = ofa_ope_template_frame_bin_new();
	my_utils_widget_set_margins( GTK_WIDGET( priv->template_bin ), 2, 2, 2, 0 );
	gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( priv->template_bin ), 0, 0, 1, 1 );

	ofa_ope_template_frame_bin_set_settings_key( priv->template_bin, G_OBJECT_TYPE_NAME( page ));

	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_NEW );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_PROPERTIES );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_DUPLICATE );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_DELETE );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_SPACER );
	ofa_ope_template_frame_bin_add_action( priv->template_bin, TEMPLATE_ACTION_GUIDED_INPUT );

	g_signal_connect( priv->template_bin, "ofa-activated", G_CALLBACK( on_row_activated ), page );

	ofa_ope_template_frame_bin_set_getter( priv->template_bin, OFA_IGETTER( page ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaOpeTemplatePagePrivate *priv;
	GtkWidget *top_widget;

	g_return_val_if_fail( page && OFA_IS_OPE_TEMPLATE_PAGE( page ), NULL );

	priv = ofa_ope_template_page_get_instance_private( OFA_OPE_TEMPLATE_PAGE( page ));

	top_widget = ofa_ope_template_frame_bin_get_current_page( priv->template_bin );

	return( top_widget );
}

/*
 * double click on a row opens the ope_template properties
 */
static void
on_row_activated( ofaOpeTemplateFrameBin *frame, const gchar *mnemo, ofaOpeTemplatePage *self )
{
	ofoOpeTemplate *ope;
	ofaHub *hub;
	GtkWindow *toplevel;

	if( my_strlen( mnemo )){

		hub = ofa_igetter_get_hub( OFA_IGETTER( self ));
		g_return_if_fail( hub && OFA_IS_HUB( hub ));

		ope = ofo_ope_template_get_by_mnemo( hub, mnemo );
		g_return_if_fail( ope && OFO_IS_OPE_TEMPLATE( ope ));

		toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

		ofa_ope_template_properties_run( OFA_IGETTER( self ), toplevel, ope, NULL );
	}
}
