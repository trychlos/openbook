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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-igetter.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-dossier.h"
#include "api/ofo-paimean.h"

#include "core/ofa-paimean-frame-bin.h"
#include "core/ofa-paimean-properties.h"

#include "ui/ofa-paimean-page.h"

/* private instance data
 */
typedef struct {

	/* internals
	 */
	gchar              *settings_prefix;

	/* UI
	 */
	ofaPaimeanFrameBin *fbin;
}
	ofaPaimeanPagePrivate;

static GtkWidget *page_v_get_top_focusable_widget( const ofaPage *page );
static GtkWidget *action_page_v_setup_view( ofaActionPage *page );
static void       on_row_activated( ofaPaimeanFrameBin *bin, ofoPaimean *paimean, ofaPaimeanPage *self );

G_DEFINE_TYPE_EXTENDED( ofaPaimeanPage, ofa_paimean_page, OFA_TYPE_ACTION_PAGE, 0,
		G_ADD_PRIVATE( ofaPaimeanPage ))

static void
paimean_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_paimean_page_finalize";
	ofaPaimeanPagePrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAIMEAN_PAGE( instance ));

	/* free data members here */
	priv = ofa_paimean_page_get_instance_private( OFA_PAIMEAN_PAGE( instance ));

	g_free( priv->settings_prefix );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_page_parent_class )->finalize( instance );
}

static void
paimean_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_PAIMEAN_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paimean_page_parent_class )->dispose( instance );
}

static void
ofa_paimean_page_init( ofaPaimeanPage *self )
{
	static const gchar *thisfn = "ofa_paimean_page_init";
	ofaPaimeanPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PAIMEAN_PAGE( self ));

	priv = ofa_paimean_page_get_instance_private( self );

	priv->settings_prefix = g_strdup( G_OBJECT_TYPE_NAME( self ));
}

static void
ofa_paimean_page_class_init( ofaPaimeanPageClass *klass )
{
	static const gchar *thisfn = "ofa_paimean_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = paimean_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = paimean_page_finalize;

	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = page_v_get_top_focusable_widget;

	OFA_ACTION_PAGE_CLASS( klass )->setup_view = action_page_v_setup_view;
}

static GtkWidget *
page_v_get_top_focusable_widget( const ofaPage *page )
{
	ofaPaimeanPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_PAIMEAN_PAGE( page ), NULL );

	priv = ofa_paimean_page_get_instance_private( OFA_PAIMEAN_PAGE( page ));

	return( priv->fbin ? ofa_paimean_frame_bin_get_tree_view( priv->fbin ) : NULL );
}

static GtkWidget *
action_page_v_setup_view( ofaActionPage *page )
{
	static const gchar *thisfn = "ofa_paimean_page_v_setup_view";
	ofaPaimeanPagePrivate *priv;
	ofaIGetter *getter;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = ofa_paimean_page_get_instance_private( OFA_PAIMEAN_PAGE( page ));

	getter = ofa_page_get_getter( OFA_PAGE( page ));
	priv->fbin = ofa_paimean_frame_bin_new( getter, priv->settings_prefix, TRUE );
	g_signal_connect( priv->fbin, "ofa-activated", G_CALLBACK( on_row_activated ), page );

	return( GTK_WIDGET( priv->fbin ));
}

static void
on_row_activated( ofaPaimeanFrameBin *bin, ofoPaimean *paimean, ofaPaimeanPage *self )
{
	ofaIGetter *getter;
	GtkWindow *toplevel;

	g_return_if_fail( paimean && OFO_IS_PAIMEAN( paimean ));

	getter = ofa_page_get_getter( OFA_PAGE( self ));
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( self ));

	ofa_paimean_properties_run( getter, toplevel, paimean );
}
