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

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "core/my-window-prot.h"

#include "ui/ofa-account-select.h"
#include "ui/ofa-currency-combo.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-dossier-cur.h"

/* private instance data
 */
struct _ofaDossierCurPrivate {

	/* runtime data
	 */
	GSList    *currencies;				/* used currencies, from entries */

	/* internals
	 */
	GtkGrid   *grid;
	gint       count;					/* total count of rows in the grid */
	GtkWidget *message;
};

#define DATA_COLUMN                     "ofa-data-column"
#define DATA_ROW                        "ofa-data-row"
#define DATA_COMBO                      "ofa-data-combo"

/* the columns in the dynamic grid
 */
enum {
	COL_ADD = 0,
	COL_CURRENCY,
	COL_ACCOUNT,
	COL_SELECT,
	COL_REMOVE,
	N_COLUMNS
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-dossier-cur.ui";
static const gchar *st_ui_id            = "DossierCurDlg";

G_DEFINE_TYPE( ofaDossierCur, ofa_dossier_cur, MY_TYPE_DIALOG )

static void       v_init_dialog( myDialog *dialog );
static void       add_empty_row( ofaDossierCur *self );
static void       add_button( ofaDossierCur *self, const gchar *stock_id, gint column, gint row );
static void       on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierCur *self );
static void       on_account_changed( GtkEntry *entry, ofaDossierCur *self );
static void       on_account_select( ofaDossierCur *self, gint row );
static void       on_button_clicked( GtkButton *button, ofaDossierCur *self );
static void       remove_row( ofaDossierCur *self, gint row );
static void       set_currency( ofaDossierCur *self, gint row, const gchar *code );
static void       set_account( ofaDossierCur *self, const gchar *currency, const gchar *account );
static gint       find_currency_row( ofaDossierCur *self, const gchar *currency );
static GObject   *get_currency_combo_at( ofaDossierCur *self, gint row );
static void       check_for_enable_dlg( ofaDossierCur *self );
static gboolean   is_dialog_validable( ofaDossierCur *self );
static gboolean   v_quit_on_ok( myDialog *dialog );
static gboolean   do_update( ofaDossierCur *self );

static void
dossier_cur_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_cur_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( OFA_IS_DOSSIER_CUR( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_cur_parent_class )->finalize( instance );
}

static void
dossier_cur_dispose( GObject *instance )
{
	ofaDossierCurPrivate *priv;

	g_return_if_fail( OFA_IS_DOSSIER_CUR( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_DOSSIER_CUR( instance )->priv;

		ofo_entry_free_currencies( priv->currencies );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_cur_parent_class )->dispose( instance );
}

static void
ofa_dossier_cur_init( ofaDossierCur *self )
{
	static const gchar *thisfn = "ofa_dossier_cur_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( OFA_IS_DOSSIER_CUR( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_DOSSIER_CUR, ofaDossierCurPrivate );
}

static void
ofa_dossier_cur_class_init( ofaDossierCurClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_cur_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_cur_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_cur_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaDossierCurPrivate ));
}

/**
 * ofa_dossier_cur_run:
 * @main: the main window of the application.
 *
 * Update the properties of a rate
 */
void
ofa_dossier_cur_run( ofaMainWindow *main_window, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_dossier_cur_run";
	ofaDossierCur *self;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_DOSSIER_CUR,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	gtk_window_set_modal( my_window_get_toplevel( MY_WINDOW( self )), TRUE );
	/*if( parent ){
		gtk_window_set_transient_for( my_window_get_toplevel( MY_WINDOW( self )), parent );
	}*/

	my_dialog_run_dialog( MY_DIALOG( self ));

	g_object_unref( self );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaDossierCur *self;
	ofaDossierCurPrivate *priv;
	gint i;
	GSList *currencies, *it;
	GtkContainer *container;
	ofoDossier *dossier;
	const gchar *account, *currency;
	GdkRGBA color;

	self = OFA_DOSSIER_CUR( dialog );
	priv = self->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));
	dossier = MY_WINDOW( dialog )->prot->dossier;

	priv->message = my_utils_container_get_child_by_name( container, "p2-message" );
	gdk_rgba_parse( &color, "#ff0000" );
	gtk_widget_override_color( priv->message, GTK_STATE_FLAG_NORMAL, &color );

	priv->grid = GTK_GRID( my_utils_container_get_child_by_name( container, "p2-grid" ));
	priv->count = 1;
	add_button( self, "gtk-add", COL_ADD, priv->count );

	/* display all used currencies (from entries)
	 * then display the corresponding account number (if set) */
	priv->currencies = ofo_entry_get_currencies( dossier );
	for( i=1, it=priv->currencies ; it ; ++i, it=it->next ){
		add_empty_row( self );
		set_currency( self, i, ( const gchar * ) it->data );
	}

	/* for currencies already recorded in dossier_cur,
	 * set the account number  */
	currencies = ofo_dossier_get_currencies( dossier );
	for( it=currencies ; it ; it=it->next ){
		currency = ( const gchar * ) it->data;
		account = ofo_dossier_get_sld_account( dossier, currency );
		set_account( self, currency, account );
	}

	check_for_enable_dlg( self );
}

/*
 * insert a row at the given position, counted from zero
 * priv->count maintains the count of rows in the grid, including the
 * headers, but not counting the last row with just an 'Add' button.
 */
static void
add_empty_row( ofaDossierCur *self )
{
	ofaDossierCurPrivate *priv;
	GtkWidget *widget;
	ofaCurrencyCombo *combo;
	gint row;

	priv = self->priv;
	row = priv->count;

	gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, COL_ADD, row ));

	/* currency combo box */
	widget = gtk_alignment_new( 0.5, 0.5, 1, 1 );
	gtk_grid_attach( priv->grid, widget, COL_CURRENCY, row, 1, 1 );
	combo = ofa_currency_combo_new();
	ofa_currency_combo_attach_to( combo, GTK_CONTAINER( widget ));
	ofa_currency_combo_set_columns( combo, CURRENCY_DISP_CODE );
	ofa_currency_combo_set_main_window( combo, MY_WINDOW( self )->prot->main_window );
	g_signal_connect( combo, "changed", G_CALLBACK( on_currency_changed ), self );
	g_object_set_data( G_OBJECT( combo ), DATA_ROW, GINT_TO_POINTER( row ));
	g_object_set_data( G_OBJECT( widget ), DATA_COMBO, combo);

	/* account number */
	widget = gtk_entry_new();
	g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( row ));
	gtk_entry_set_width_chars( GTK_ENTRY( widget ), 10 );
	gtk_grid_attach( priv->grid, widget, COL_ACCOUNT, row, 1, 1 );
	g_signal_connect( widget, "changed", G_CALLBACK( on_account_changed ), self );

	/* account select */
	add_button( self, "gtk-index", COL_SELECT, row );

	/* management buttons */
	add_button( self, "gtk-remove", COL_REMOVE, row );
	add_button( self, "gtk-add", COL_ADD, row+1 );

	priv->count += 1;
	gtk_widget_show_all( GTK_WIDGET( priv->grid ));
}

/*
 * add an 'Add' button to the row (counted from zero)
 */
static void
add_button( ofaDossierCur *self, const gchar *stock_id, gint column, gint row )
{
	GtkWidget *image;
	GtkButton *button;

	button = GTK_BUTTON( gtk_button_new());
	g_object_set_data( G_OBJECT( button ), DATA_COLUMN, GINT_TO_POINTER( column ));
	g_object_set_data( G_OBJECT( button ), DATA_ROW, GINT_TO_POINTER( row ));
	image = gtk_image_new_from_icon_name( stock_id, GTK_ICON_SIZE_BUTTON );
	gtk_button_set_image( button, image );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_button_clicked ), self );
	gtk_grid_attach( self->priv->grid, GTK_WIDGET( button ), column, row, 1, 1 );
}

static void
on_currency_changed( ofaCurrencyCombo *combo, const gchar *code, ofaDossierCur *self )
{
	check_for_enable_dlg( self );
}

static void
on_account_changed( GtkEntry *entry, ofaDossierCur *self )
{
	check_for_enable_dlg( self );
}

static void
on_account_select( ofaDossierCur *self, gint row )
{
	ofaDossierCurPrivate *priv;
	GtkWidget *entry;
	gchar *acc_number;

	priv = self->priv;
	entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	acc_number = ofa_account_select_run(
						MY_WINDOW( self )->prot->main_window,
						gtk_entry_get_text( GTK_ENTRY( entry )));

	if( acc_number ){
		gtk_entry_set_text( GTK_ENTRY( entry ), acc_number );
	}

	g_free( acc_number );
}

static void
on_button_clicked( GtkButton *button, ofaDossierCur *self )
{
	gint column, row;

	column = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_COLUMN ));
	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( button ), DATA_ROW ));
	switch( column ){
		case COL_ADD:
			add_empty_row( self );
			break;
		case COL_SELECT:
			on_account_select( self, row );
			break;
		case COL_REMOVE:
			remove_row( self, row );
			break;
	}
}

/*
 * we have clicked on the 'Remove' button on the 'row' row
 * row is counted from zero
 * priv->count maintains the count of rows in the grid, including the
 * headers, but not counting the last row with just an 'Add' button.
 */
static void
remove_row( ofaDossierCur *self, gint row )
{
	ofaDossierCurPrivate *priv;
	gint i, line;
	GtkWidget *widget;

	priv = self->priv;

	/* first remove the line
	 * note that there is no 'add' button in a used line */
	for( i=0 ; i<N_COLUMNS ; ++i ){
		if( i != COL_ADD ){
			gtk_widget_destroy( gtk_grid_get_child_at( priv->grid, i, row ));
		}
	}

	/* then move the follow lines one row up */
	for( line=row+1 ; line<=priv->count+1 ; ++line ){
		for( i=0 ; i<N_COLUMNS ; ++i ){
			widget = gtk_grid_get_child_at( priv->grid, i, line );
			if( widget ){
				g_object_ref( widget );
				gtk_container_remove( GTK_CONTAINER( priv->grid ), widget );
				gtk_grid_attach( priv->grid, widget, i, line-1, 1, 1 );
				g_object_set_data( G_OBJECT( widget ), DATA_ROW, GINT_TO_POINTER( line-1 ));
				g_object_unref( widget );
			}
		}
	}

	gtk_widget_show_all( GTK_WIDGET( priv->grid ));

	/* last update the lines count */
	priv->count -= 1;
}

/*
 * at initialization time, select the given currency at the given row
 */
static void
set_currency( ofaDossierCur *self, gint row, const gchar *code )
{
	GObject *combo;

	combo = get_currency_combo_at( self, row );
	g_return_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ));

	ofa_currency_combo_set_selected( OFA_CURRENCY_COMBO( combo ), code );
}

/*
 * at initialization time, set the given account for the given currency
 */
static void
set_account( ofaDossierCur *self, const gchar *currency, const gchar *account )
{
	ofaDossierCurPrivate *priv;
	gint row;
	GtkWidget *entry;

	priv = self->priv;
	row = find_currency_row( self, currency );

	if( !row ){
		add_empty_row( self );
		row = priv->count;
		set_currency( self, row, currency );
	}

	entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	gtk_entry_set_text( GTK_ENTRY( entry ), account );
}

static gint
find_currency_row( ofaDossierCur *self, const gchar *currency )
{
	ofaDossierCurPrivate *priv;
	GObject *combo;
	gint row, cmp;
	gchar *selected;

	priv = self->priv;

	for( row=1 ; row<=priv->count ; ++row ){

		combo = get_currency_combo_at( self, row );
		g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), -1 );

		selected = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		cmp = g_utf8_collate( selected, currency );
		g_free( selected );

		if( cmp == 0 ){
			return( row );
		}
	}

	return( -1 );
}

static GObject *
get_currency_combo_at( ofaDossierCur *self, gint row )
{
	ofaDossierCurPrivate *priv;
	GtkWidget *align;
	GObject *combo;

	priv = self->priv;

	align = gtk_grid_get_child_at( priv->grid, COL_CURRENCY, row );
	g_return_val_if_fail( align && GTK_IS_ALIGNMENT( align ), NULL );

	combo = g_object_get_data( G_OBJECT( align ), DATA_COMBO );
	g_return_val_if_fail( combo && OFA_IS_CURRENCY_COMBO( combo ), NULL );

	return( combo );
}

/*
 * is the dialog validable ?
 * yes:
 * - if all currencies from entries are set with a valid account number
 * - the account is with the right currency
 * - if no currency is set more than once
 */
static void
check_for_enable_dlg( ofaDossierCur *self )
{
	GtkWidget *button;
	gboolean ok;

	ok = is_dialog_validable( self );

	button = my_utils_container_get_child_by_name(
					GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))), "btn-ok" );

	gtk_widget_set_sensitive( button, ok );
}

static gboolean
is_dialog_validable( ofaDossierCur *self )
{
	ofaDossierCurPrivate *priv;
	GSList *it;
	gint row;
	const gchar *acc_number, *acc_currency, *cstr;
	GtkWidget *entry;
	GObject *combo;
	ofoAccount *account;
	gchar *code, *str;
	GSList *cursets, *find;
	gboolean ok;

	priv = self->priv;
	ok = TRUE;
	cursets = NULL;
	gtk_label_set_text( GTK_LABEL( priv->message ), "" );

	for( row=1 ; row<priv->count ; ++row ){
		combo = get_currency_combo_at( self, row );
		g_return_val_if_fail( OFA_IS_CURRENCY_COMBO( combo ), FALSE );

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( code && g_utf8_strlen( code, -1 )){

			entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

			acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));
			if( !acc_number || !g_utf8_strlen( acc_number, -1 )){
				str = g_strdup_printf( _( "%s: empty account number" ), code );
				gtk_label_set_text( GTK_LABEL( priv->message ), str );
				g_free( str );
				ok = FALSE;
				break;
			}

			account = ofo_account_get_by_number( MY_WINDOW( self )->prot->dossier, acc_number );
			if( !account ){
				str = g_strdup_printf( _( "%s: invalid account number: %s" ), code, acc_number );
				gtk_label_set_text( GTK_LABEL( priv->message ), str );
				g_free( str );
				ok = FALSE;
				break;
			}

			g_return_val_if_fail( OFO_IS_ACCOUNT( account ), FALSE );
			if( ofo_account_is_root( account )){
				str = g_strdup_printf( _( "%s: unauthorized root account: %s" ),
								code, acc_number );
				gtk_label_set_text( GTK_LABEL( priv->message ), str );
				g_free( str );
				ok = FALSE;
				break;
			}

			acc_currency = ofo_account_get_currency( account );
			if( g_utf8_collate( code, acc_currency )){
				str = g_strdup_printf( _( "%s: incompatible account currency: %s: %s" ),
								code, acc_number, acc_currency );
				gtk_label_set_text( GTK_LABEL( priv->message ), str );
				g_free( str );
				ok = FALSE;
				break;
			}

			find = g_slist_find_custom( cursets, code, ( GCompareFunc ) g_utf8_collate );
			if( find ){
				str = g_strdup_printf( _( "%s: duplicate currency" ), code );
				gtk_label_set_text( GTK_LABEL( priv->message ), str );
				g_free( str );
				ok = FALSE;
				break;
			}

			cursets = g_slist_prepend( cursets, g_strdup( code ));
		}
		g_free( code );
	}

	/* if all currencies are rightly set, also check that at least
	 * mandatory currencies are set */
	if( ok ){
		for( it=priv->currencies ; it ; it=it->next ){
			cstr = ( const gchar * ) it->data;
			find = g_slist_find_custom( cursets, cstr, ( GCompareFunc ) g_utf8_collate );
			if( !find ){
				str = g_strdup_printf( _( "%s: unset mandatory currency" ), cstr );
				gtk_label_set_text( GTK_LABEL( priv->message ), str );
				g_free( str );
				ok = FALSE;
				break;
			}
		}
	}

	g_slist_free_full( cursets, ( GDestroyNotify ) g_free );

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_DOSSIER_CUR( dialog )));
}

/*
 * either creating a new rate (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 * Please note that a record is uniquely identified by the mnemo + the date
 */
static gboolean
do_update( ofaDossierCur *self )
{
	ofaDossierCurPrivate *priv;
	ofoDossier *dossier;
	gint row;
	GtkWidget *entry;
	GObject *combo;
	gchar *code;
	const gchar *acc_number;
	gboolean ok;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;
	dossier = MY_WINDOW( self )->prot->dossier;

	ofo_dossier_reset_currencies( dossier );

	for( row=1 ; row<priv->count ; ++row ){
		combo = get_currency_combo_at( self, row );
		g_return_val_if_fail( OFA_IS_CURRENCY_COMBO( combo ), FALSE );

		code = ofa_currency_combo_get_selected( OFA_CURRENCY_COMBO( combo ));
		if( code && g_utf8_strlen( code, -1 )){

			entry = gtk_grid_get_child_at( priv->grid, COL_ACCOUNT, row );
			g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
			acc_number = gtk_entry_get_text( GTK_ENTRY( entry ));

			if( acc_number && g_utf8_strlen( acc_number, -1 )){
				ofo_dossier_set_sld_account( dossier, code, acc_number );
			}
		}
		g_free( code );
	}

	ok = ofo_dossier_update_currencies( dossier );

	return( ok );
}
