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

#include "api/my-utils.h"
#include "api/ofa-buttons-box.h"
#include "api/ofa-date-filter-hv-bin.h"
#include "api/ofa-idate-filter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-declare-page.h"
#include "tva/ofa-tva-form-store.h"
#include "tva/ofo-tva-form.h"

/* priv instance data
 */
struct _ofaTVADeclarePagePrivate {

	/* internals
	 */
	ofoDossier *dossier;
	gboolean    editable;

	/* UI
	 */
	GtkWidget  *main_grid;				/* inside of the top frame */
	GtkWidget  *selection_combo;
	GtkWidget  *selection_label;
	GtkWidget  *booleans_viewport;
	GtkWidget  *taxes_viewport;

	/* runtime
	 */
	ofoTVAForm *form;
};

static const gchar *st_pref_effect      = "TVADeclareEffectDates";

static GtkWidget *v_setup_view( ofaPage *page );
static GtkWidget *setup_top_grid( ofaTVADeclarePage *self );
static GtkWidget *setup_form_selection( ofaTVADeclarePage *self );
static GtkWidget *setup_dates_filter( ofaTVADeclarePage *self );
static GtkWidget *setup_declare_frame( ofaTVADeclarePage *self );
static GtkWidget *setup_book_booleans_page( ofaTVADeclarePage *page );
static GtkWidget *setup_book_taxes_page( ofaTVADeclarePage *page );
static GtkWidget *setup_book_notes_page( ofaTVADeclarePage *page );
static GtkWidget *v_setup_buttons( ofaPage *page );
static GtkWidget *v_get_top_focusable_widget( const ofaPage *page );
static void       on_form_selected( GtkComboBox *combo, ofaTVADeclarePage *page );
static void       clear_declare_frame( ofaTVADeclarePage *page );
static void       init_declare_frame_with_form( ofaTVADeclarePage *page, ofoTVAForm *form );
static GtkWidget *init_declare_grid_booleans( ofaTVADeclarePage *page, ofoTVAForm *form );
static GtkWidget *init_declare_grid_bases_and_taxes( ofaTVADeclarePage *page, ofoTVAForm *form );
static void       on_effect_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaTVADeclarePage *page );

G_DEFINE_TYPE( ofaTVADeclarePage, ofa_tva_declare_page, OFA_TYPE_PAGE )

static void
tva_declare_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_declare_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TVA_DECLARE_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_declare_page_parent_class )->finalize( instance );
}

static void
tva_declare_page_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_TVA_DECLARE_PAGE( instance ));

	if( !OFA_PAGE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_declare_page_parent_class )->dispose( instance );
}

static void
ofa_tva_declare_page_init( ofaTVADeclarePage *self )
{
	static const gchar *thisfn = "ofa_tva_declare_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TVA_DECLARE_PAGE( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_DECLARE_PAGE, ofaTVADeclarePagePrivate );
}

static void
ofa_tva_declare_page_class_init( ofaTVADeclarePageClass *klass )
{
	static const gchar *thisfn = "ofa_tva_declare_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_declare_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_declare_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_view = v_setup_view;
	OFA_PAGE_CLASS( klass )->setup_buttons = v_setup_buttons;
	OFA_PAGE_CLASS( klass )->get_top_focusable_widget = v_get_top_focusable_widget;

	g_type_class_add_private( klass, sizeof( ofaTVADeclarePagePrivate ));
}

static GtkWidget *
v_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_tva_declare_page_v_setup_view";
	ofaTVADeclarePagePrivate *priv;
	ofoDossier *dossier;
	GtkWidget *frame, *widget;

	g_debug( "%s: page=%p", thisfn, ( void * ) page );

	priv = OFA_TVA_DECLARE_PAGE( page )->priv;
	dossier = ofa_page_get_dossier( page );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );
	priv->dossier = dossier;
	priv->editable = ofo_dossier_is_current( dossier );

	/* actually a grid */
	frame = setup_top_grid( OFA_TVA_DECLARE_PAGE( page ));

	widget = setup_form_selection( OFA_TVA_DECLARE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( priv->main_grid ), widget, 0, 0, 1, 1 );

	widget = setup_dates_filter( OFA_TVA_DECLARE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( priv->main_grid ), widget, 1, 0, 1, 1 );

	widget = setup_declare_frame( OFA_TVA_DECLARE_PAGE( page ));
	gtk_grid_attach( GTK_GRID( priv->main_grid ), widget, 0, 1, 2, 1 );

	return( frame );
}

static GtkWidget *
setup_top_grid( ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;

	priv = self->priv;

	priv->main_grid = gtk_grid_new();

	return( priv->main_grid );
}

static GtkWidget *
setup_form_selection( ofaTVADeclarePage *self )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *frame, *grid, *combo, *label;
	ofaTVAFormStore *store;
	GtkCellRenderer *cell;

	priv = self->priv;

	frame = gtk_frame_new( _( " Form selection " ));
	my_utils_widget_set_margin( frame, 0, 4, 4, 4 );
	gtk_frame_set_shadow_type( GTK_FRAME( frame ), GTK_SHADOW_IN );

	grid = gtk_grid_new();
	my_utils_widget_set_margin( grid, 4, 4, 12, 4 );
	gtk_container_add( GTK_CONTAINER( frame ), grid );

	combo = gtk_combo_box_new();
	gtk_grid_attach( GTK_GRID( grid ), combo, 0, 0, 1, 1 );
	priv->selection_combo = combo;

	label = gtk_label_new( NULL );
	gtk_widget_set_sensitive( label, FALSE );
	gtk_grid_attach( GTK_GRID( grid ), label, 0, 1, 1, 1 );
	priv->selection_label = label;

	g_signal_connect( combo, "changed", G_CALLBACK( on_form_selected ), self );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), cell, "text", TVA_COL_MNEMO );

	gtk_combo_box_set_id_column ( GTK_COMBO_BOX( combo ), TVA_COL_MNEMO );

	store = ofa_tva_form_store_new( priv->dossier );
	gtk_combo_box_set_model( GTK_COMBO_BOX( combo ), GTK_TREE_MODEL( store ));

	return( frame );
}

static GtkWidget *
setup_dates_filter( ofaTVADeclarePage *self )
{
	ofaDateFilterHVBin *filter;

	filter = ofa_date_filter_hv_bin_new();
	my_utils_widget_set_margin( GTK_WIDGET( filter ), 0, 4, 0, 4 );
	ofa_idate_filter_set_prefs( OFA_IDATE_FILTER( filter ), st_pref_effect );
	g_signal_connect( filter, "ofa-focus-out", G_CALLBACK( on_effect_filter_changed ), self );

	return( GTK_WIDGET( filter ));
}

static GtkWidget *
setup_declare_frame( ofaTVADeclarePage *self )
{
	GtkWidget *book, *label, *booleans, *taxes, *notes;

	book = gtk_notebook_new();
	my_utils_widget_set_margin( book, 0, 4, 4, 4 );
	gtk_widget_set_hexpand( book, TRUE );
	gtk_widget_set_vexpand( book, TRUE );

	label = gtk_label_new_with_mnemonic( _( "_Booleans" ));
	booleans = setup_book_booleans_page( self );
	gtk_notebook_append_page( GTK_NOTEBOOK( book ), booleans, label );

	label = gtk_label_new_with_mnemonic( _( "Bases and _Taxes" ));
	taxes = setup_book_taxes_page( self );
	gtk_notebook_append_page( GTK_NOTEBOOK( book ), taxes, label );

	label = gtk_label_new_with_mnemonic( _( "Notes" ));
	notes = setup_book_notes_page( self );
	gtk_notebook_append_page( GTK_NOTEBOOK( book ), notes, label );

	return( book );
}

static GtkWidget *
setup_book_booleans_page( ofaTVADeclarePage *page )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *scrolled, *viewport;

	priv = page->priv;

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	my_utils_widget_set_margin( scrolled, 4, 4, 4, 4 );

	viewport = gtk_viewport_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( scrolled ), viewport );

	priv->booleans_viewport = viewport;

	return( scrolled );
}

static GtkWidget *
setup_book_taxes_page( ofaTVADeclarePage *page )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *scrolled, *viewport;

	priv = page->priv;

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	my_utils_widget_set_margin( scrolled, 4, 4, 4, 4 );

	viewport = gtk_viewport_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( scrolled ), viewport );

	priv->taxes_viewport = viewport;

	return( scrolled );
}

static GtkWidget *
setup_book_notes_page( ofaTVADeclarePage *page )
{
	GtkWidget *scrolled, *textview;

	scrolled = gtk_scrolled_window_new( NULL, NULL );
	my_utils_widget_set_margin( scrolled, 4, 4, 4, 4 );

	textview = gtk_text_view_new();
	gtk_container_add( GTK_CONTAINER( scrolled ), textview );
	my_utils_container_notes_setup_ex( GTK_TEXT_VIEW( textview ), NULL, TRUE );

	return( scrolled );
}

static GtkWidget *
v_setup_buttons( ofaPage *page )
{
	return( NULL );
}

static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	ofaTVADeclarePagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_TVA_DECLARE_PAGE( page ), NULL );

	priv = OFA_TVA_DECLARE_PAGE( page )->priv;

	return( priv->selection_combo );
}

static void
on_form_selected( GtkComboBox *combo, ofaTVADeclarePage *page )
{
	ofaTVADeclarePagePrivate *priv;
	const gchar *cstr, *clabel;
	ofoTVAForm *form;

	priv = page->priv;

	/* first destroy the previous declaration grid */
	clear_declare_frame( page );
	priv->form = NULL;
	clabel = NULL;

	/* then setup the frame with the current selection */
	cstr = gtk_combo_box_get_active_id( combo );
	if( my_strlen( cstr )){
		form = ofo_tva_form_get_by_mnemo( priv->dossier, cstr );
		if( form ){
			clabel = ofo_tva_form_get_label( form );
			init_declare_frame_with_form( page, form );
			priv->form = form;
		}
	}

	gtk_label_set_text( GTK_LABEL( priv->selection_label ), my_strlen( clabel ) ? clabel : "" );
	gtk_widget_show_all( GTK_WIDGET( page ));
}

static void
clear_declare_frame( ofaTVADeclarePage *page )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *child;

	priv = page->priv;

	g_return_if_fail( priv->booleans_viewport && GTK_IS_VIEWPORT( priv->booleans_viewport ));
	child = gtk_bin_get_child( GTK_BIN( priv->booleans_viewport ));
	if( child ){
		gtk_widget_destroy( child );
	}

	g_return_if_fail( priv->taxes_viewport && GTK_IS_VIEWPORT( priv->taxes_viewport ));
	child = gtk_bin_get_child( GTK_BIN( priv->taxes_viewport ));
	if( child ){
		gtk_widget_destroy( child );
	}
}

static void
init_declare_frame_with_form( ofaTVADeclarePage *page, ofoTVAForm *form )
{
	ofaTVADeclarePagePrivate *priv;
	GtkWidget *widget;

	g_return_if_fail( form && OFO_IS_TVA_FORM( form ));

	priv = page->priv;

	widget = init_declare_grid_booleans( page, form );
	gtk_container_add( GTK_CONTAINER( priv->booleans_viewport ), widget );

	widget = init_declare_grid_bases_and_taxes( page, form );
	gtk_container_add( GTK_CONTAINER( priv->taxes_viewport ), widget );
}

static GtkWidget *
init_declare_grid_booleans( ofaTVADeclarePage *page, ofoTVAForm *form )
{
	GtkWidget *grid, *check;
	guint i, count;
	const gchar *cstr;

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 3 );
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );

	count = ofo_tva_form_boolean_get_count( form );
	for( i=0 ; i<count ; ++i ){
		cstr = ofo_tva_form_boolean_get_label( form, i );
		check = gtk_check_button_new_with_label( cstr );
		gtk_grid_attach( GTK_GRID( grid ), check, 0, i, 1, 1 );
	}

	return( grid );
}

static GtkWidget *
init_declare_grid_bases_and_taxes( ofaTVADeclarePage *page, ofoTVAForm *form )
{
	GtkWidget *grid, *label, *entry;
	guint i, count;
	const gchar *cstr;
	gchar *str;
	gboolean has_base, has_amount;

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing( GTK_GRID( grid ), 3 );
	gtk_grid_set_column_spacing( GTK_GRID( grid ), 4 );

	count = ofo_tva_form_detail_get_count( form );
	for( i=0 ; i<count ; ++i ){

		label = gtk_label_new( NULL );
		gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
		my_utils_widget_set_xalign( label, 1.0 );
		str = g_strdup_printf( "<i>%u</i>", i+1 );
		gtk_label_set_markup( GTK_LABEL( label ), str );
		g_free( str );
		gtk_label_set_width_chars( GTK_LABEL( label ), 3 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, i, 1, 1 );

		/* code */
		entry = gtk_entry_new();
		my_utils_widget_set_editable( entry, FALSE );
		gtk_entry_set_width_chars( GTK_ENTRY( entry ), 4 );
		gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 4 );
		gtk_grid_attach( GTK_GRID( grid ), entry, 1, i, 1, 1 );

		cstr = ofo_tva_form_detail_get_code( form, i );
		gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );

		/* label */
		entry = gtk_entry_new();
		my_utils_widget_set_editable( entry, FALSE );
		gtk_widget_set_hexpand( entry, TRUE );
		gtk_grid_attach( GTK_GRID( grid ), entry, 2, i, 1, 1 );

		cstr = ofo_tva_form_detail_get_label( form, i );
		gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );

		/* base */
		has_base = ofo_tva_form_detail_get_has_base( form, i );
		if( has_base ){
			entry = gtk_entry_new();
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, 3, i, 1, 1 );

			cstr = ofo_tva_form_detail_get_base( form, i );
			gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );
		}

		/* amount */
		has_amount = ofo_tva_form_detail_get_has_amount( form, i );
		if( has_amount ){
			entry = gtk_entry_new();
			gtk_entry_set_width_chars( GTK_ENTRY( entry ), 8 );
			gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), 10 );
			gtk_grid_attach( GTK_GRID( grid ), entry, 4, i, 1, 1 );

			cstr = ofo_tva_form_detail_get_amount( form, i );
			gtk_entry_set_text( GTK_ENTRY( entry ), my_strlen( cstr ) ? cstr : "" );
		}
	}

	return( grid );
}

static void
on_effect_filter_changed( ofaIDateFilter *filter, gint who, gboolean empty, const GDate *date, ofaTVADeclarePage *page )
{
}
