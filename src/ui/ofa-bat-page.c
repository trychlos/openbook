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

#include <glib/gi18n.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-file-format.h"
#include "api/ofa-iimportable.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofa-settings.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "ui/ofa-bat-page.h"
#include "ui/ofa-bat-properties.h"
#include "ui/ofa-bat-treeview.h"
#include "ui/ofa-bat-utils.h"

/* private instance data
 */
struct _ofaBatPagePrivate {

	/* runtime data
	 */
	ofoDossier     *dossier;
	gboolean        is_current;			/* whether the dossier is current */

	/* UI
	 */
	ofaBatTreeview *tview;				/* the main treeview of the page */
	GtkWidget      *update_btn;
	GtkWidget      *delete_btn;
	GtkWidget      *import_btn;
};

G_DEFINE_TYPE( ofaBatPage, ofa_bat_page, OFA_TYPE_PAGE )

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *page );
static void       on_row_selected( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *page );
static void       on_update_clicked( GtkButton *button, ofaBatPage *page );
static void       on_delete_clicked( GtkButton *button, ofaBatPage *page );
static void       on_import_clicked( GtkButton *button, ofaBatPage *page );

static void
bats_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_page_parent_class )->finalize( instance );
}

static void
bats_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_BAT_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_page_parent_class )->dispose( instance );
}

static void
ofa_bat_page_init( ofaBatPage *self )
{
	static const gchar *thisfn = "ofa_bat_page_init";

	g_return_if_fail( OFA_IS_BAT_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_PAGE, ofaBatPagePrivate );
}

static void
ofa_bat_page_class_init( ofaBatPageClass *klass )
{
	static const gchar *thisfn = "ofa_bat_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bats_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = bats_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaBatPagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_bat_page_v_setup_view";
	ofaBatPagePrivate *priv;
	ofoDossier *dossier;
	static ofaBatColumns st_columns [] = {
			BAT_DISP_ID, BAT_DISP_BEGIN, BAT_DISP_END, BAT_DISP_COUNT,
			BAT_DISP_FORMAT, BAT_DISP_RIB,
			BAT_DISP_BEGIN_SOLDE, BAT_DISP_END_SOLDE, BAT_DISP_CURRENCY,
			0 };

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_BAT_PAGE( page )->priv;

	dossier = ofa_page_get_dossier( page );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	priv->dossier = dossier;
	priv->is_current = ofo_dossier_is_current( dossier );

	priv->tview = ofa_bat_treeview_new();
	ofa_bat_treeview_set_columns( priv->tview, st_columns );
	ofa_bat_treeview_set_main_window( priv->tview, ofa_page_get_main_window( page ));

	g_signal_connect( priv->tview, "changed", G_CALLBACK( on_row_selected ), page );
	g_signal_connect( priv->tview, "activated", G_CALLBACK( on_row_activated ), page );

	return( GTK_WIDGET( priv->tview ));
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	ofaBatPagePrivate *priv;
	ofaButtonsBox *buttons_box;

	priv = OFA_BAT_PAGE( page )->priv;

	buttons_box = ofa_buttons_box_new();

	ofa_buttons_box_add_spacer( buttons_box );

	/* always disabled */
	ofa_buttons_box_add_button_with_mnemonic(
				buttons_box, BUTTON_NEW, NULL, NULL );

	priv->update_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_PROPERTIES, G_CALLBACK( on_update_clicked ), page );

	priv->delete_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, BUTTON_DELETE, G_CALLBACK( on_delete_clicked ), page );

	ofa_buttons_box_add_spacer( buttons_box );

	priv->import_btn =
			ofa_buttons_box_add_button_with_mnemonic(
					buttons_box, _( "_Import..." ), G_CALLBACK( on_import_clicked ), page );
	gtk_widget_set_sensitive( priv->import_btn, priv->is_current );

	return( GTK_WIDGET( buttons_box ));
}

static void
on_row_activated( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *page )
{
	on_update_clicked( NULL, OFA_BAT_PAGE( page ));
}

static void
on_row_selected( ofaBatTreeview *tview, ofoBat *bat, ofaBatPage *page )
{
	ofaBatPagePrivate *priv;
	gboolean is_bat;

	priv = page->priv;
	is_bat = bat && OFO_IS_BAT( bat );

	gtk_widget_set_sensitive( priv->update_btn,
			is_bat );

	gtk_widget_set_sensitive( priv->delete_btn,
			priv->is_current && is_bat && ofo_bat_is_deletable( bat, priv->dossier ));
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaBatPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_BAT_PAGE( page ), NULL );

	priv = OFA_BAT_PAGE( page )->priv;

	return( ofa_bat_treeview_get_treeview( priv->tview ));
}

/*
 * only notes can be updated
 */
static void
on_update_clicked( GtkButton *button, ofaBatPage *page )
{
	ofaBatPagePrivate *priv;
	ofoBat *bat;

	priv = page->priv;
	bat = ofa_bat_treeview_get_selected( priv->tview );
	if( bat ){
		ofa_bat_properties_run( ofa_page_get_main_window( OFA_PAGE( page )), bat );
	}

	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

static void
on_delete_clicked( GtkButton *button, ofaBatPage *page )
{
	ofaBatPagePrivate *priv;
	ofoBat *bat;

	priv = page->priv;
	bat = ofa_bat_treeview_get_selected( priv->tview );
	if( bat ){
		g_return_if_fail( ofo_bat_is_deletable( bat, ofa_page_get_dossier( OFA_PAGE( page ))));
		ofa_bat_treeview_delete_bat( priv->tview, bat );
	}
	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}

/*
 * open GtkFileChooser dialog to let the user select the file to be
 * imported and import it
 */
static void
on_import_clicked( GtkButton *button, ofaBatPage *page )
{
	ofa_bat_utils_import( ofa_page_get_main_window( OFA_PAGE( page )));
	gtk_widget_grab_focus( v_get_top_focusable_widget( OFA_PAGE( page )));
}
