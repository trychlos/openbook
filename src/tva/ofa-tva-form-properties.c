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
#include <stdarg.h>

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/my-window-prot.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-dossier.h"

#include "core/ofa-main-window.h"

#include "tva/ofa-tva-form-properties.h"
#include "tva/ofo-tva-form.h"

/* private instance data
 */
struct _ofaTVAFormPropertiesPrivate {

	/* internals
	 */
	ofoDossier    *dossier;
	gboolean       is_current;
	ofoTVAForm    *tva_form;
	gboolean       is_new;
	gboolean       updated;

	/* UI
	 */
	GtkWidget     *grid;
	gint           count;				/* count of data rows added to the grid */
	 	 	 	 	 	 	 	 	 	/* - not including header */
										/* - not including last row with only '+' button */
	GtkWidget     *ok_btn;

	/* data
	 */
	gchar         *mnemo;
	gchar         *label;
};

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"
#define RANG_WIDTH                      3
#define DET_CODE_WIDTH                  4
#define DET_CODE_MAX_WIDTH              4
#define DET_CODE_MAX_LENGTH             10
#define DET_LABEL_MAX_WIDTH             80
#define DET_AMOUNT_WIDTH                8
#define DET_AMOUNT_MAX_WIDTH            10
#define DET_AMOUNT_MAX_LENGTH           80

/* the columns in the dynamic grid
 */
enum {
	COL_ADD = 0,
	COL_ROW = 0,
	COL_CODE,
	COL_LABEL,
	COL_HAS_AMOUNT,
	COL_AMOUNT,
	COL_UP,
	COL_DOWN,
	COL_REMOVE,
	N_COLUMNS
};

static const gchar  *st_ui_xml       = PLUGINUIDIR "/ofa-tva-form-properties.ui";
static const gchar  *st_ui_id        = "TVAFormPropertiesDlg";

G_DEFINE_TYPE( ofaTVAFormProperties, ofa_tva_form_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      insert_new_row( ofaTVAFormProperties *self, guint idx );
static void      add_empty_row( ofaTVAFormProperties *self );
static void      add_button( ofaTVAFormProperties *self, const gchar *stock_id, gint column, gint row, gint right_margin );
static void      update_detail_buttons( ofaTVAFormProperties *self );
static void      signal_row_added( ofaTVAFormProperties *self );
static void      signal_row_removed( ofaTVAFormProperties *self );
static void      on_mnemo_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void      on_det_code_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void      on_det_label_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void      on_det_has_amount_toggled( GtkToggleButton *button, ofaTVAFormProperties *self );
static void      on_det_amount_changed( GtkEntry *entry, ofaTVAFormProperties *self );
static void      on_button_clicked( GtkButton *button, ofaTVAFormProperties *self );
static void      exchange_rows( ofaTVAFormProperties *self, gint row_a, gint row_b );
static void      remove_row( ofaTVAFormProperties *self, gint row );
static void      check_for_enable_dlg( ofaTVAFormProperties *self );
static gboolean  is_dialog_validable( ofaTVAFormProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaTVAFormProperties *self );

static void
tva_form_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tva_form_properties_finalize";
	ofaTVAFormPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_TVA_FORM_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_TVA_FORM_PROPERTIES( instance )->priv;
	g_free( priv->mnemo );
	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_properties_parent_class )->finalize( instance );
}

static void
tva_form_properties_dispose( GObject *instance )
{
	g_return_if_fail( OFA_IS_TVA_FORM_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tva_form_properties_parent_class )->dispose( instance );
}

static void
ofa_tva_form_properties_init( ofaTVAFormProperties *self )
{
	static const gchar *thisfn = "ofa_tva_form_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_TVA_FORM_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TVA_FORM_PROPERTIES, ofaTVAFormPropertiesPrivate );

	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
	self->priv->count = 0;
}

static void
ofa_tva_form_properties_class_init( ofaTVAFormPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_tva_form_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tva_form_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = tva_form_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaTVAFormPropertiesPrivate ));
}

/**
 * ofa_tva_form_properties_run:
 * @main_window: the #ofaMainWindow main window of the application.
 * @form: the #ofoTVAForm to be displayed/updated.
 *
 * Update the properties of a tva_form.
 */
gboolean
ofa_tva_form_properties_run( const ofaMainWindow *main_window, ofoTVAForm *form )
{
	static const gchar *thisfn = "ofa_tva_form_properties_run";
	ofaTVAFormProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, form=%p",
			thisfn, ( void * ) main_window, ( void * ) form );

	self = g_object_new(
					OFA_TYPE_TVA_FORM_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->tva_form = form;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaTVAFormProperties *self;
	ofaTVAFormPropertiesPrivate *priv;
	GtkApplicationWindow *main_window;
	guint count, idx;
	gchar *title;
	const gchar *mnemo;
	GtkEntry *entry;
	GtkWidget *label;
	GtkContainer *container;

	self = OFA_TVA_FORM_PROPERTIES( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	main_window = my_window_get_main_window( MY_WINDOW( self ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	priv->dossier = ofa_main_window_get_dossier( OFA_MAIN_WINDOW( main_window ));
	g_return_if_fail( priv->dossier && OFO_IS_DOSSIER( priv->dossier ));

	priv->is_current = ofo_dossier_is_current( priv->dossier );

	mnemo = ofo_tva_form_get_mnemo( priv->tva_form );
	if( !mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new TVA form" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » TVA form" ), mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->ok_btn = my_utils_container_get_child_by_name( container, "btn-ok" );
	g_return_if_fail( priv->ok_btn && GTK_IS_BUTTON( priv->ok_btn ));

	/* mnemonic */
	priv->mnemo = g_strdup( mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-mnemo-entry" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p1-mnemo-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	priv->label = g_strdup( ofo_tva_form_get_label( priv->tva_form ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label-entry" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

	label = my_utils_container_get_child_by_name( container, "p1-label-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	gtk_label_set_mnemonic_widget( GTK_LABEL( label ), GTK_WIDGET( entry ));

	my_utils_container_notes_init( container, tva_form );
	my_utils_container_updstamp_init( container, tva_form );
	my_utils_container_set_editable( container, priv->is_current );

	/* set the detail rows after having set editability for current
	 * dossier (because my_utils_container_set_editable() set the
	 * sensitivity flag without considering the has_amount flag or
	 * the row number - which is ok in general but not here) */
	priv->grid = my_utils_container_get_child_by_name( container, "p2-grid" );
	add_button( self, "gtk-add", COL_ADD, 1, 4 );
	count = ofo_tva_form_get_detail_count( priv->tva_form );
	for( idx=0 ; idx<count ; ++idx ){
		insert_new_row( self, idx );
	}

	/* if not the current exercice, then only have a 'Close' button */
	if( !priv->is_current ){
		priv->ok_btn = my_dialog_set_readonly_buttons( dialog );
	}

	check_for_enable_dlg( self );
}

/*
 * idx is the index of the tva form detail to be added, and counted from zero
 * Due to the presence of headers, the 'idx' detail line will go to 'idx+1' row of the grid
 */
static void
insert_new_row( ofaTVAFormProperties *self, guint idx )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry, *check;
	const gchar *cstr;
	gint row;
	gboolean has_amount;

	add_empty_row( self );
	priv = self->priv;
	row = priv->count;

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_CODE, row );
	cstr = ofo_tva_form_get_detail_code( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_LABEL, row );
	cstr = ofo_tva_form_get_detail_label( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}

	check = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_HAS_AMOUNT, row );
	has_amount = ofo_tva_form_get_detail_has_amount( priv->tva_form, idx );
	//g_debug( "insert_new_row: row=%d, has_amount=%s", row, has_amount ? "True":"False" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), has_amount );
	/* handler is not triggered when has_amount is false */
	on_det_has_amount_toggled( GTK_TOGGLE_BUTTON( check ), self );

	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_AMOUNT, row );
	cstr = ofo_tva_form_get_detail_amount( priv->tva_form, idx );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( entry ), cstr );
	}
}

static void
add_empty_row( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *entry, *label, *check;
	gchar *str;
	guint row;

	priv = self->priv;
	row = priv->count+1;

	gtk_widget_destroy( gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_ADD, row ));

	str = g_strdup_printf( "%2u", row );
	label = gtk_label_new( str );
	g_object_set_data( G_OBJECT( label ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_free( str );
	gtk_widget_set_sensitive( GTK_WIDGET( label ), FALSE );
	my_utils_widget_set_margin( label, 0, 2, 0, 4 );
	my_utils_widget_set_xalign( label, 1.0 );
	gtk_label_set_width_chars( GTK_LABEL( label ), RANG_WIDTH );
	gtk_grid_attach( GTK_GRID( priv->grid ), GTK_WIDGET( label ), COL_ROW, row, 1, 1 );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_code_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), DET_CODE_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_CODE_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_CODE_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->grid ), entry, COL_CODE, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_label_changed ), self );
	gtk_widget_set_hexpand( entry, TRUE );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_LABEL_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_LABEL_MAX_WIDTH );
	gtk_grid_attach( GTK_GRID( priv->grid ), entry, COL_LABEL, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	check = gtk_check_button_new();
	g_object_set_data( G_OBJECT( check ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( check, "toggled", G_CALLBACK( on_det_has_amount_toggled ), self );
	gtk_grid_attach( GTK_GRID( priv->grid ), check, COL_HAS_AMOUNT, row, 1, 1 );
	gtk_widget_set_sensitive( check, priv->is_current );

	entry = gtk_entry_new();
	g_object_set_data( G_OBJECT( entry ), DATA_ROW, GUINT_TO_POINTER( row ));
	g_signal_connect( entry, "changed", G_CALLBACK( on_det_amount_changed ), self );
	gtk_entry_set_width_chars( GTK_ENTRY( entry ), DET_AMOUNT_WIDTH );
	gtk_entry_set_max_width_chars( GTK_ENTRY( entry ), DET_AMOUNT_MAX_WIDTH );
	gtk_entry_set_max_length( GTK_ENTRY( entry ), DET_AMOUNT_MAX_LENGTH );
	gtk_grid_attach( GTK_GRID( priv->grid ), entry, COL_AMOUNT, row, 1, 1 );
	gtk_widget_set_sensitive( entry, priv->is_current );

	add_button( self, "gtk-go-up", COL_UP, row, 0 );
	add_button( self, "gtk-go-down", COL_DOWN, row, 0 );
	add_button( self, "gtk-remove", COL_REMOVE, row, 0 );
	add_button( self, "gtk-add", COL_ADD, row+1, 4 );

	priv->count += 1;
	signal_row_added( self );
	gtk_widget_show_all( GTK_WIDGET( priv->grid ));
}


static void
add_button( ofaTVAFormProperties *self, const gchar *stock_id, gint column, gint row, gint right_margin )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *image, *button;

	priv = self->priv;
	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	button = gtk_button_new();
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_widget_set_halign( button, GTK_ALIGN_END );
	my_utils_widget_set_margin( GTK_WIDGET( button ), 0, 0, 0, right_margin );
	gtk_button_set_image( GTK_BUTTON( button ), image );
	g_signal_connect( button, "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( GTK_GRID( priv->grid ), button, column, row, 1, 1 );
	gtk_widget_set_sensitive( button, priv->is_current );
}

static void
update_detail_buttons( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	GtkWidget *up_btn, *down_btn;
	guint i;

	priv = self->priv;
	for( i=1 ; i<=priv->count ; ++i ){

		up_btn = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_UP, i );
		g_return_if_fail( up_btn && GTK_IS_WIDGET( up_btn ));

		down_btn = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_DOWN, i );
		g_return_if_fail( down_btn && GTK_IS_WIDGET( down_btn ));

		gtk_widget_set_sensitive( up_btn, priv->is_current );
		gtk_widget_set_sensitive( down_btn, priv->is_current );

		if( i == 1 ){
			gtk_widget_set_sensitive( up_btn, FALSE );
			//g_debug( "update_detail_buttons: setting up_btn sensitivity to FALSE for i=%u", i );
		}

		if( i == priv->count ){
			gtk_widget_set_sensitive( down_btn, FALSE );
			//g_debug( "update_detail_buttons: setting down_btn sensitivity to FALSE for i=%u", i );
		}
	}
}

static void
signal_row_added( ofaTVAFormProperties *self )
{
	update_detail_buttons( self );
}

static void
signal_row_removed( ofaTVAFormProperties *self )
{
	update_detail_buttons( self );
}

static void
on_mnemo_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = self->priv;

	g_free( priv->mnemo );
	priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;

	priv = self->priv;

	g_free( priv->label );
	priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_det_code_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_det_label_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_det_has_amount_toggled( GtkToggleButton *button, ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	guint row;
	GtkWidget *entry;
	gboolean checked;

	priv = self->priv;
	checked = gtk_toggle_button_get_active( button );
	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_AMOUNT, row );
	gtk_widget_set_sensitive( entry, checked && priv->is_current );
	//g_debug( "on_det_has_amount_toggled: row=%u, checked=%s", row, checked ? "True":"False" );

	check_for_enable_dlg( self );
}

static void
on_det_amount_changed( GtkEntry *entry, ofaTVAFormProperties *self )
{
	check_for_enable_dlg( self );
}

static void
on_button_clicked( GtkButton *button, ofaTVAFormProperties *self )
{
	gint column, row;

	column = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_UINT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	switch( column ){
		case COL_ADD:
			add_empty_row( self );
			break;
		case COL_UP:
			g_return_if_fail( row > 1 );
			exchange_rows( self, row, row-1 );
			break;
		case COL_DOWN:
			g_return_if_fail( row < self->priv->count );
			exchange_rows( self, row, row+1 );
			break;
		case COL_REMOVE:
			remove_row( self, row );
			break;
	}
}

static void
exchange_rows( ofaTVAFormProperties *self, gint row_a, gint row_b )
{
	ofaTVAFormPropertiesPrivate *priv;
	gint i;
	GtkWidget *w_a, *w_b;

	priv = self->priv;

	/* do not move the row number */
	for( i=1 ; i<N_COLUMNS ; ++i ){

		w_a = gtk_grid_get_child_at( GTK_GRID( priv->grid ), i, row_a );
		g_object_ref( w_a );
		gtk_container_remove( GTK_CONTAINER( priv->grid ), w_a );

		w_b = gtk_grid_get_child_at( GTK_GRID( priv->grid ), i, row_b );
		g_object_ref( w_b );
		gtk_container_remove( GTK_CONTAINER( priv->grid ), w_b );

		gtk_grid_attach( GTK_GRID( priv->grid ), w_a, i, row_b, 1, 1 );
		g_object_set_data( G_OBJECT( w_a ), DATA_ROW, GINT_TO_POINTER( row_b ));

		gtk_grid_attach( GTK_GRID( priv->grid ), w_b, i, row_a, 1, 1 );
		g_object_set_data( G_OBJECT( w_b ), DATA_ROW, GINT_TO_POINTER( row_a ));

		g_object_unref( w_a );
		g_object_unref( w_b );
	}

	update_detail_buttons( self );
}

static void
remove_row( ofaTVAFormProperties *self, gint row )
{
	ofaTVAFormPropertiesPrivate *priv;
	gint i, line;
	GtkWidget *widget, *label;
	gchar *str;

	priv = self->priv;

	/* first remove the line */
	for( i=0 ; i<N_COLUMNS ; ++i ){
		gtk_widget_destroy( gtk_grid_get_child_at( GTK_GRID( priv->grid ), i, row ));
	}

	/* then move the following lines one row up */
	for( line=row+1 ; line<=priv->count+1 ; ++line ){
		for( i=0 ; i<N_COLUMNS ; ++i ){
			widget = gtk_grid_get_child_at( GTK_GRID( priv->grid ), i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( priv->grid ), widget );
				gtk_grid_attach( GTK_GRID( priv->grid ), widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
		if( line <= priv->count ){
			/* update the rang number on each moved line */
			label = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_ROW, line-1 );
			g_return_if_fail( label && GTK_IS_LABEL( label ));
			str = g_strdup_printf( "%d", line-1 );
			gtk_label_set_text( GTK_LABEL( label ), str );
			g_free( str );
		}
	}

	/* last update the lines count */
	priv->count -= 1;
	signal_row_removed( self );

	gtk_widget_show_all( GTK_WIDGET( priv->grid ));
}

/*
 * are we able to validate this tva form
 */
static void
check_for_enable_dlg( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	gboolean ok;

	priv = self->priv;
	ok = is_dialog_validable( self );

	gtk_widget_set_sensitive( priv->ok_btn, ok );
}

/*
 * are we able to validate this tva form
 */
static gboolean
is_dialog_validable( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	gboolean ok, exists, subok;

	priv = self->priv;
	ok = ofo_tva_form_is_valid( priv->dossier, priv->mnemo);
	//g_debug( "is_dialog_validable: is_valid=%s", ok ? "True":"False" );

	if( ok ){
		exists = ( ofo_tva_form_get_by_mnemo( priv->dossier, priv->mnemo ) != NULL );
		//g_debug( "is_dialog_validable: exists=%s", exists ? "True":"False" );
		//g_debug( "is_dialog_validable: is_new=%s", priv->is_new ? "True":"False" );
		subok = !priv->is_new &&
						!g_utf8_collate( priv->mnemo, ofo_tva_form_get_mnemo( priv->tva_form ));
		//g_debug( "is_dialog_validable: subok=%s", subok ? "True":"False" );
		ok = !exists || subok;
		//g_debug( "is_dialog_validable: ok=%s", ok ? "True":"False" );
		/* ok &= something does not work */
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_TVA_FORM_PROPERTIES( dialog )));
}

/*
 * either creating a new tva_form (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaTVAFormProperties *self )
{
	ofaTVAFormPropertiesPrivate *priv;
	gint i;
	GtkWidget *entry, *check;
	const gchar *code, *label, *amount;
	gchar *prev_mnemo;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;

	prev_mnemo = g_strdup( ofo_tva_form_get_mnemo( priv->tva_form ));

	ofo_tva_form_set_mnemo( priv->tva_form, priv->mnemo );
	ofo_tva_form_set_label( priv->tva_form, priv->label );
	my_utils_container_notes_get( my_window_get_toplevel( MY_WINDOW( self )), tva_form );

	ofo_tva_form_free_detail_all( priv->tva_form );

	for( i=1 ; i<=priv->count ; ++i ){
		entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_CODE, i );
		code = gtk_entry_get_text( GTK_ENTRY( entry ));
		entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_LABEL, i );
		label = gtk_entry_get_text( GTK_ENTRY( entry ));
		entry = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_AMOUNT, i );
		amount = gtk_entry_get_text( GTK_ENTRY( entry ));
		if( my_strlen( code ) || my_strlen( label ) || my_strlen( amount )){
			check = gtk_grid_get_child_at( GTK_GRID( priv->grid ), COL_HAS_AMOUNT, i );
			ofo_tva_form_add_detail(
					priv->tva_form,
					code, label, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check )), amount );
		}
	}

	if( priv->is_new ){
		priv->updated =
				ofo_tva_form_insert( priv->tva_form, priv->dossier );
	} else {
		priv->updated =
				ofo_tva_form_update( priv->tva_form, priv->dossier, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
