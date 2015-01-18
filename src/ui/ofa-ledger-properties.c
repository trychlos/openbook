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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "core/my-window-prot.h"

#include "ui/ofa-ledger-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaLedgerPropertiesPrivate {

	/* internals
	 */
	ofoLedger *ledger;
	gboolean   is_new;
	gboolean   updated;

	/* data
	 */
	gchar      *mnemo;
	gchar      *label;
	gchar      *upd_user;
	GTimeVal    upd_stamp;
	GDate       closing;
};

/* columns displayed in the exercice combobox
 */
enum {
	EXE_COL_BEGIN = 0,
	EXE_COL_END,
	EXE_N_COLUMNS
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-ledger-properties.ui";
static const gchar  *st_ui_id  = "LedgerPropertiesDlg";

G_DEFINE_TYPE( ofaLedgerProperties, ofa_ledger_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_balances_page( ofaLedgerProperties *self );
static void      on_mnemo_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void      check_for_enable_dlg( ofaLedgerProperties *self );
static gboolean  is_dialog_validable( ofaLedgerProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaLedgerProperties *self );

static void
ledger_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_ledger_properties_finalize";
	ofaLedgerPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_LEDGER_PROPERTIES( instance ));

	/* free data members here */
	priv = OFA_LEDGER_PROPERTIES( instance )->priv;
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->upd_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_properties_parent_class )->finalize( instance );
}

static void
ledger_properties_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_LEDGER_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_ledger_properties_parent_class )->dispose( instance );
}

static void
ofa_ledger_properties_init( ofaLedgerProperties *self )
{
	static const gchar *thisfn = "ofa_ledger_properties_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_LEDGER_PROPERTIES( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
							self, OFA_TYPE_LEDGER_PROPERTIES, ofaLedgerPropertiesPrivate );

	self->priv->is_new = FALSE;
	self->priv->updated = FALSE;
	my_date_clear( &self->priv->closing );
}

static void
ofa_ledger_properties_class_init( ofaLedgerPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_ledger_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = ledger_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = ledger_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;

	g_type_class_add_private( klass, sizeof( ofaLedgerPropertiesPrivate ));
}

/**
 * ofa_ledger_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an ledger
 */
gboolean
ofa_ledger_properties_run( ofaMainWindow *main_window, ofoLedger *ledger )
{
	static const gchar *thisfn = "ofa_ledger_properties_run";
	ofaLedgerProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, ledger=%p",
				thisfn, ( void * ) main_window, ( void * ) ledger );

	self = g_object_new(
					OFA_TYPE_LEDGER_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->priv->ledger = ledger;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->priv->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaLedgerPropertiesPrivate *priv;
	gchar *title, *str;
	const gchar *jou_mnemo;
	GtkEntry *entry;
	GtkContainer *container;
	GtkWidget *label;

	priv = OFA_LEDGER_PROPERTIES( dialog )->priv;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	jou_mnemo = ofo_ledger_get_mnemo( priv->ledger );
	if( !jou_mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new ledger" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » ledger" ), jou_mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->mnemo = g_strdup( jou_mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-mnemo" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), dialog );

	priv->label = g_strdup( ofo_ledger_get_label( priv->ledger ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	my_date_set_from_date( &priv->closing, ofo_ledger_get_last_close( priv->ledger ));
	label = my_utils_container_get_child_by_name( container, "p1-last-close" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));
	str = my_date_to_str( &priv->closing, MY_DATE_DMYY );
	gtk_label_set_text( GTK_LABEL( label ), str );
	g_free( str );

	init_balances_page( OFA_LEDGER_PROPERTIES( dialog ));

	my_utils_init_notes_ex(
			container, ledger, ofo_dossier_is_current( MY_WINDOW( dialog )->prot->dossier ));
	my_utils_init_upd_user_stamp_ex( container, ledger );

	check_for_enable_dlg( OFA_LEDGER_PROPERTIES( dialog ));
}

/*
 * the "Debit" and "Credit" titles are already displayed on row 0
 * for each currency, we add
 * - a title line "balance for xxx currency"
 * - three lines for validated, rough and future balances
 */
static void
init_balances_page( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	GtkWindow *container;
	GtkWidget *grid, *label;
	GList *currencies, *it;
	const gchar *code, *symbol;
	ofoCurrency *currency;
	gint i, count, digits;
	gchar *str;
	GdkRGBA color;

	priv = self->priv;

	container = my_window_get_toplevel( MY_WINDOW( self ));

	grid = my_utils_container_get_child_by_name( GTK_CONTAINER( container ), "p2-grid" );
	g_return_if_fail( grid && GTK_IS_GRID( grid ));

	gdk_rgba_parse( &color, "#0000ff" );

	currencies = ofo_ledger_get_currencies( priv->ledger );
	count = g_list_length( currencies );
	/* 4 lines by currency */
	gtk_grid_insert_row( GTK_GRID( grid ), 4*count );

	for( i=0, it=currencies ; it ; ++i, it=it->next ){
		code = ( const gchar * ) it->data;
		currency = ofo_currency_get_by_code( MY_WINDOW( self )->prot->dossier, code );
		g_return_if_fail( currency && OFO_IS_CURRENCY( currency ));
		digits = ofo_currency_get_digits( currency );
		symbol = ofo_currency_get_symbol( currency );

		str = g_strdup_printf( _( "%s balance" ), code );
		label = gtk_label_new( str );
		g_free( str );
		gtk_widget_override_color( GTK_WIDGET( label ), GTK_STATE_FLAG_NORMAL, &color );
		gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 4*i+1, 1, 1 );

		label = gtk_label_new( _( "      Validated balance :" ));
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 4*i+2, 1, 1 );

		label = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 4*i+2, 1, 1 );
		str = my_double_to_str_ex( ofo_ledger_get_val_debit( priv->ledger, code ), digits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = gtk_label_new( "" );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 4*i+2, 1, 1 );
		gtk_label_set_text( GTK_LABEL( label ), symbol );

		label = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 4*i+2, 1, 1 );
		str = my_double_to_str_ex( ofo_ledger_get_val_credit( priv->ledger, code ), digits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = gtk_label_new( "" );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 4*i+2, 1, 1 );
		gtk_label_set_text( GTK_LABEL( label ), symbol );

		label = gtk_label_new( _( "Rough balance :" ));
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 4*i+3, 1, 1 );

		label = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 4*i+3, 1, 1 );
		str = my_double_to_str_ex( ofo_ledger_get_rough_debit( priv->ledger, code ), digits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = gtk_label_new( "" );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 4*i+3, 1, 1 );
		gtk_label_set_text( GTK_LABEL( label ), symbol );

		label = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 4*i+3, 1, 1 );
		str = my_double_to_str_ex( ofo_ledger_get_rough_credit( priv->ledger, code ), digits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = gtk_label_new( "" );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 4*i+3, 1, 1 );
		gtk_label_set_text( GTK_LABEL( label ), symbol );

		label = gtk_label_new( _( "Future balance :" ));
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 0, 4*i+4, 1, 1 );

		label = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 1, 4*i+4, 1, 1 );
		str = my_double_to_str_ex( ofo_ledger_get_futur_debit( priv->ledger, code ), digits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = gtk_label_new( "" );
		gtk_grid_attach( GTK_GRID( grid ), label, 2, 4*i+4, 1, 1 );
		gtk_label_set_text( GTK_LABEL( label ), symbol );

		label = gtk_label_new( "" );
		gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
		gtk_grid_attach( GTK_GRID( grid ), label, 3, 4*i+4, 1, 1 );
		str = my_double_to_str_ex( ofo_ledger_get_futur_credit( priv->ledger, code ), digits );
		gtk_label_set_text( GTK_LABEL( label ), str );
		g_free( str );

		label = gtk_label_new( "" );
		gtk_grid_attach( GTK_GRID( grid ), label, 4, 4*i+4, 1, 1 );
		gtk_label_set_text( GTK_LABEL( label ), symbol );
	}
}

static void
on_mnemo_changed( GtkEntry *entry, ofaLedgerProperties *self )
{
	g_free( self->priv->mnemo );
	self->priv->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaLedgerProperties *self )
{
	g_free( self->priv->label );
	self->priv->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaLedgerProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
						"btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaLedgerProperties *self )
{
	gboolean ok;
	ofaLedgerPropertiesPrivate *priv;
	ofoLedger *exists;

	priv = self->priv;

	ok = ofo_ledger_is_valid( priv->mnemo, priv->label );

	if( ok ){
		exists = ofo_ledger_get_by_mnemo(
				MY_WINDOW( self )->prot->dossier, priv->mnemo );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_ledger_get_mnemo( priv->ledger )));
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_LEDGER_PROPERTIES( dialog )));
}

/*
 * either creating a new ledger (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 */
static gboolean
do_update( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	gchar *prev_mnemo;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->priv;
	prev_mnemo = g_strdup( ofo_ledger_get_mnemo( priv->ledger ));

	/* the new mnemo is not yet used,
	 * or it is used by this same ledger (does not have been modified yet)
	 */
	ofo_ledger_set_mnemo( priv->ledger, priv->mnemo );
	ofo_ledger_set_label( priv->ledger, priv->label );
	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), ledger );

	if( priv->is_new ){
		priv->updated =
				ofo_ledger_insert( priv->ledger, MY_WINDOW( self )->prot->dossier );
	} else {
		priv->updated =
				ofo_ledger_update( priv->ledger, MY_WINDOW( self )->prot->dossier, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
