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
#include "api/my-utils.h"
#include "api/ofo-dossier.h"
#include "api/ofo-ledger.h"

#include "core/my-window-prot.h"

#include "ui/ofa-currency-combo.h"
#include "ui/ofa-ledger-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaLedgerPropertiesPrivate {

	/* internals
	 */
	ofoLedger      *ledger;
	gboolean        is_new;
	gboolean        updated;

	/* page 2: balances display
	 */
	gint            exe_id;
	gchar          *currency;

	/* UI
	 */
	ofaCurrencyCombo *dev_combo;

	/* data
	 */
	gchar          *mnemo;
	gchar          *label;
	gchar          *upd_user;
	GTimeVal        upd_stamp;
	GDate           closing;
};

/* columns displayed in the exercice combobox
 */
enum {
	EXE_COL_BEGIN = 0,
	EXE_COL_END,
	EXE_COL_EXE_ID,
	EXE_N_COLUMNS
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-ledger-properties.ui";
static const gchar  *st_ui_id  = "LedgerPropertiesDlg";

G_DEFINE_TYPE( ofaLedgerProperties, ofa_ledger_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_balances_page( ofaLedgerProperties *self );
static void      on_mnemo_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaLedgerProperties *self );
static void      on_exe_changed( GtkComboBox *box, ofaLedgerProperties *self );
static void      on_currency_changed( const gchar *currency, ofaLedgerProperties *self );
static void      display_balances( ofaLedgerProperties *self );
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
	g_free( priv->currency );
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

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

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
	gchar *title;
	const gchar *jou_mnemo;
	GtkEntry *entry;
	GtkContainer *container;

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

	init_balances_page( OFA_LEDGER_PROPERTIES( dialog ));

	my_utils_init_notes_ex( container, ledger );
	my_utils_init_upd_user_stamp_ex( container, ledger );

	check_for_enable_dlg( OFA_LEDGER_PROPERTIES( dialog ));
}

static void
init_balances_page( ofaLedgerProperties *self )
{
	GtkContainer *container;
	ofaCurrencyComboParms parms;
	GtkComboBox *exe_box;
	GtkTreeModel *tmodel;
	GtkCellRenderer *text_cell;
	gint current_exe_id;
	GList *list, *ili;
	gint exe_id, idx, i;
	const GDate *begin, *end;
	gchar *sbegin, *send;
	GtkTreeIter iter;

	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self )));

	parms.container = container;
	parms.dossier = MY_WINDOW( self )->protected->dossier;
	parms.combo_name = "p2-dev-combo";
	parms.label_name = NULL;
	parms.disp_code = FALSE;
	parms.disp_label = TRUE;
	parms.pfnSelected = ( ofaCurrencyComboCb ) on_currency_changed;
	parms.user_data = self;
	parms.initial_code = ofo_dossier_get_default_currency( MY_WINDOW( self )->protected->dossier );

	self->priv->dev_combo = ofa_currency_combo_new( &parms );

	exe_box = ( GtkComboBox * ) my_utils_container_get_child_by_name( container, "p2-exe-combo" );
	g_return_if_fail( exe_box && GTK_IS_COMBO_BOX( exe_box ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			EXE_N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT ));
	gtk_combo_box_set_model( exe_box, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( exe_box ), text_cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( exe_box ), text_cell, "text", EXE_COL_BEGIN );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( exe_box ), text_cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( exe_box ), text_cell, "text", EXE_COL_END );

	list = ofo_ledger_get_exe_list( self->priv->ledger );
	idx = -1;
	current_exe_id = ofo_dossier_get_current_exe_id( MY_WINDOW( self )->protected->dossier );

	for( ili=list, i=0 ; ili ; ili=ili->next, ++i ){
		exe_id = GPOINTER_TO_INT( ili->data );
		if( exe_id == current_exe_id ){
			idx = i;
		}
		begin = ofo_dossier_get_exe_begin( MY_WINDOW( self )->protected->dossier, exe_id );
		end = ofo_dossier_get_exe_end( MY_WINDOW( self )->protected->dossier, exe_id );
		if( !my_date_is_valid( begin )){
			sbegin = g_strdup( "" );
			if( !my_date_is_valid( end )){
				send = g_strdup( _( "Current exercice" ));
			} else {
				send = my_date_to_str( end, MY_DATE_DMMM );
			}
		} else {
			sbegin = my_date_to_str( begin, MY_DATE_DMMM );
			if( !my_date_is_valid( end )){
				send = g_strdup( "" );
			} else {
				send = my_date_to_str( end, MY_DATE_DMMM );
			}
		}

		gtk_list_store_insert_with_values(
				GTK_LIST_STORE( tmodel ),
				&iter,
				-1,
				EXE_COL_BEGIN,  sbegin,
				EXE_COL_END,    send,
				EXE_COL_EXE_ID, exe_id,
				-1 );

		g_free( sbegin );
		g_free( send );
	}

	g_signal_connect( G_OBJECT( exe_box ), "changed", G_CALLBACK( on_exe_changed ), self );

	if( idx != -1 ){
		gtk_combo_box_set_active( exe_box, idx );
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
on_exe_changed( GtkComboBox *box, ofaLedgerProperties *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter, EXE_COL_EXE_ID, &self->priv->exe_id, -1 );
		display_balances( self );
	}
}

/*
 * ofaCurrencyComboCb
 */
static void
on_currency_changed( const gchar *currency, ofaLedgerProperties *self )
{
	g_free( self->priv->currency );
	self->priv->currency = g_strdup( currency );
	display_balances( self );
}

static void
display_balances( ofaLedgerProperties *self )
{
	ofaLedgerPropertiesPrivate *priv;
	GtkContainer *container;
	GtkLabel *label;
	gchar *str;

	priv = self->priv;

	if( priv->exe_id <= 0 || !priv->currency || !g_utf8_strlen( priv->currency, -1 )){
		return;
	}

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-clo-deb" ));
	str = g_strdup_printf( "%'.2lf", ofo_ledger_get_clo_deb( priv->ledger, priv->exe_id, priv->currency ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-clo-cre" ));
	str = g_strdup_printf( "%'.2lf", ofo_ledger_get_clo_cre( priv->ledger, priv->exe_id, priv->currency ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-deb" ));
	str = g_strdup_printf( "%'.2lf", ofo_ledger_get_deb( priv->ledger, priv->exe_id, priv->currency ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-deb-date" ));
	str = my_date_to_str( ofo_ledger_get_deb_date( priv->ledger, priv->exe_id, priv->currency ), MY_DATE_DMYY );
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-cre" ));
	str = g_strdup_printf( "%'.2lf", ofo_ledger_get_cre( priv->ledger, priv->exe_id, priv->currency ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-cre-date" ));
	str = my_date_to_str( ofo_ledger_get_cre_date( priv->ledger, priv->exe_id, priv->currency ), MY_DATE_DMYY );
	gtk_label_set_text( label, str );
	g_free( str );
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
				MY_WINDOW( self )->protected->dossier, priv->mnemo );
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
				ofo_ledger_insert( priv->ledger );
	} else {
		priv->updated =
				ofo_ledger_update( priv->ledger, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
