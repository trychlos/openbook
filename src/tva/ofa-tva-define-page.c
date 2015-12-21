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

#include "api/my-utils.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-define-page.h"

/* priv instance data
 */
struct _ofaTVADefinePagePrivate {

	/* internals
	 */
	GtkContainer *top_box;
};

static const gchar *st_ui_xml           = PLUGINUIDIR "/ofa-tva-define-page.ui";
static const gchar *st_ui_id            = "TVADefinePage";

static GtkWidget      *v_setup_view( ofaPage *page );
static void            reparent_from_dialog( ofaTVADefinePage *self, GtkContainer *parent );
static GtkWidget      *v_get_top_focusable_widget( const ofaPage *page );

G_DEFINE_TYPE( ofaTVADefinePage, ofa_tva_define_page, OFA_TYPE_PAGE )

static void
tva_define_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_define_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_DEFINE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_define_page_parent_class )->finalize( instance );
}

static void
tva_define_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_TVA_DEFINE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_define_page_parent_class )->dispose( instance );
}

static void
ofa_tva_define_page_init( ofaTVADefinePage *self )
{
	static const gchar *thisfn = "ofa_tva_define_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_DEFINE_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_DEFINE_PAGE, ofaTVADefinePagePrivate );
}

static void
ofa_tva_define_page_class_init( ofaTVADefinePageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_define_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_define_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_define_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaTVADefinePagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_define_page_v_setup_view";
	GtkWidget *frame;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	frame = gtk_frame_new( NULL );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_NONE );
	reparent_from_dialog( OFA_TVA_DEFINE_PAGE( page ), GTK_CONTAINER( frame ));

	return( frame );
}

static void
reparent_from_dialog( ofaTVADefinePage *self, GtkContainer *parent )
{
	ofaTVADefinePagePrivate *priv;
	GtkWidget *box;

	priv = self->priv;

	box = my_utils_container_attach_from_ui( GTK_CONTAINER( parent ), st_ui_xml, st_ui_id, "px-box" );
	g_return_if_fail( box && GTK_IS_CONTAINER( box ));

	priv->top_box = GTK_CONTAINER( box );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_TVA_DEFINE_PAGE( page ), NULL );

	return( NULL );
}
