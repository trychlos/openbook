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

#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"
#include "ui/ofa-settlement.h"

/* priv instance data
 */
struct _ofaSettlementPrivate {

	/* internals
	 */
	ofoDossier        *dossier;			/* dossier */
};

/*static const gchar           *st_ui_xml                   = PKGUIDIR "/ofa-settlement.piece.ui";
static const gchar           *st_ui_id                    = "SettlementWindow";*/

G_DEFINE_TYPE( ofaSettlement, ofa_settlement, OFA_TYPE_PAGE )

static GtkWidget     *v_setup_view( ofaPage *page );
static GtkWidget     *v_setup_buttons( ofaPage *page );
static void           v_init_view( ofaPage *page );
static GtkWidget     *v_get_top_focusable_widget( const ofaPage *page );

static void
settlement_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_settlement_finalize";
	/*ofaSettlementPrivate *priv;*/

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_SETTLEMENT( instance ));

	/* free data members here */
	/*priv = OFA_SETTLEMENT( instance )->priv;*/

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_parent_class )->finalize( instance );
}

static void
settlement_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_SETTLEMENT( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_settlement_parent_class )->dispose( instance );
}

static void
ofa_settlement_init( ofaSettlement *self )
{
	static const gchar *thisfn = "ofa_settlement_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_SETTLEMENT( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_SETTLEMENT, ofaSettlementPrivate );
}

static void
ofa_settlement_class_init( ofaSettlementClass *klass )
{
	static const gchar *thisfn = "ofa_settlement_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = settlement_dispose;
	G_OBJECT_CLASS( klass )->finalize = settlement_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->init_view = v_init_view;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaSettlementPrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static void
v_init_view( ofaPage *page )
{
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_SETTLEMENT( page ), NULL );

	return( NULL );
}
