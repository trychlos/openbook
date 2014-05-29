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

#include "ui/my-utils.h"
#include "ui/ofa-base-dialog-prot.h"
#include "ui/ofa-account-properties.h"
#include "ui/ofa-main-window.h"
#include "ui/ofo-base.h"
#include "ui/ofo-account.h"
#include "ui/ofo-devise.h"
#include "ui/ofo-dossier.h"

#if 0
static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;
#endif

/* private instance data
 */
struct _ofaAccountPropertiesPrivate {

	/* internals
	 */
	ofoAccount    *account;
	gboolean       is_new;
	gboolean       updated;
	gboolean       number_ok;

	/* data
	 */
	gchar         *number;
	gchar         *label;
	gint           devise;
	gchar         *type;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
	gdouble        deb_mnt;
	gint           deb_ecr;
	GDate          deb_date;
	gdouble        cre_mnt;
	gint           cre_ecr;
	GDate          cre_date;
	gdouble        bro_deb_mnt;
	gint           bro_deb_ecr;
	GDate          bro_deb_date;
	gdouble        bro_cre_mnt;
	gint           bro_cre_ecr;
	GDate          bro_cre_date;
};

/* column ordering in the devise selection listview
 */
enum {
	COL_ID = 0,
	COL_DEVISE,
	COL_LABEL,
	N_COLUMNS
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-account-properties.ui";
static const gchar  *st_ui_id        = "AccountPropertiesDlg";

G_DEFINE_TYPE( ofaAccountProperties, ofa_account_properties, OFA_TYPE_BASE_DIALOG )

static void      v_account_properties_init_dialog( ofaBaseDialog *dialog );
static void      get_entry_number( ofaAccountProperties *self, GtkEntry **entry );
static void      get_radio_root_detail( ofaAccountProperties *self, GtkRadioButton **root, GtkRadioButton **detail );
static gboolean  v_account_properties_quit_on_ok( ofaBaseDialog *dialog );
static void      on_number_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_devise_changed( GtkComboBox *combo, ofaAccountProperties *self );
static void      on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, const gchar *type );
static void      check_for_enable_dlg( ofaAccountProperties *self );
static gboolean  is_dialog_validable( ofaAccountProperties *self );
static gboolean  do_update( ofaAccountProperties *self );

static void
account_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_properties_finalize";
	ofaAccountPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	priv = OFA_ACCOUNT_PROPERTIES( instance )->private;

	g_free( priv->number );
	g_free( priv->label );
	g_free( priv->type );
	g_free( priv->maj_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_properties_parent_class )->finalize( instance );
}

static void
account_properties_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_account_properties_dispose";

	g_return_if_fail( OFA_IS_ACCOUNT_PROPERTIES( instance ));

	if( !OFA_BASE_DIALOG( instance )->prot->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_account_properties_parent_class )->dispose( instance );
}

static void
ofa_account_properties_init( ofaAccountProperties *self )
{
	static const gchar *thisfn = "ofa_account_properties_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaAccountPropertiesPrivate, 1 );

	self->private->account = NULL;
	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_account_properties_class_init( ofaAccountPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_account_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = account_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = account_properties_finalize;

	OFA_BASE_DIALOG_CLASS( klass )->init_dialog = v_account_properties_init_dialog;
	OFA_BASE_DIALOG_CLASS( klass )->quit_on_ok = v_account_properties_quit_on_ok;
}

/**
 * ofa_account_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an account
 */
gboolean
ofa_account_properties_run( ofaMainWindow *main_window, ofoAccount *account )
{
	static const gchar *thisfn = "ofa_account_properties_run";
	ofaAccountProperties *self;
	gboolean updated;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	g_debug( "%s: main_window=%p, account=%p",
			thisfn, ( void * ) main_window, ( void * ) account );

	self = g_object_new(
				OFA_TYPE_ACCOUNT_PROPERTIES,
				OFA_PROP_MAIN_WINDOW, main_window,
				OFA_PROP_DIALOG_XML,  st_ui_xml,
				OFA_PROP_DIALOG_NAME, st_ui_id,
				NULL );

	self->private->account = account;

	ofa_base_dialog_run_dialog( OFA_BASE_DIALOG( self ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
v_account_properties_init_dialog( ofaBaseDialog *dialog )
{
	ofaAccountProperties *self;
	ofaAccountPropertiesPrivate *priv;
	gchar *title;
	const gchar *acc_number;
	GtkEntry *entry;
	GtkLabel *label;
	gchar *str;
	GtkComboBox *combo;
	GtkTreeModel *tmodel;
	GtkTreeIter iter;
	gint i, idx;
	GtkRadioButton *root_btn, *detail_btn;
	GtkCellRenderer *text_cell;
	ofoDossier *dossier;
	GList *devset, *idev;

	self = OFA_ACCOUNT_PROPERTIES( dialog );
	priv = self->private;

	acc_number = ofo_account_get_number( priv->account );
	if( !acc_number ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new account" ));
	} else {
		title = g_strdup_printf( _( "Updating account %s" ), acc_number );
	}
	gtk_window_set_title( GTK_WINDOW( dialog->prot->dialog ), title );

	priv->number = g_strdup( acc_number );
	get_entry_number( self, &entry );
	if( priv->number ){
		gtk_entry_set_text( entry, priv->number );
	}
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_number_changed ), dialog );

	priv->label = g_strdup( ofo_account_get_label( priv->account ));
	entry = GTK_ENTRY(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect(
			G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	priv->devise = ofo_account_get_devise( priv->account );
	combo = GTK_COMBO_BOX(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p1-devise" ));

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING ));
	gtk_combo_box_set_model( combo, tmodel );
	g_object_unref( tmodel );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", COL_DEVISE );

	text_cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
	gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", COL_LABEL );

	dossier = ofa_base_dialog_get_dossier( dialog );
	devset = ofo_devise_get_dataset( dossier );

	idx = OFO_BASE_UNSET_ID;
	for( i=0, idev=devset ; idev ; ++i, idev=idev->next ){
		gtk_list_store_append( GTK_LIST_STORE( tmodel ), &iter );
		gtk_list_store_set(
				GTK_LIST_STORE( tmodel ),
				&iter,
				COL_ID,     ofo_devise_get_id( OFO_DEVISE( idev->data )),
				COL_DEVISE, ofo_devise_get_code( OFO_DEVISE( idev->data )),
				COL_LABEL,  ofo_devise_get_label( OFO_DEVISE( idev->data )),
				-1 );
		if( priv->devise == ofo_devise_get_id( OFO_DEVISE( idev->data ))){
			idx = i;
		}
	}
	g_signal_connect(
			G_OBJECT( combo ), "changed", G_CALLBACK( on_devise_changed ), dialog );
	if( idx != OFO_BASE_UNSET_ID ){
		gtk_combo_box_set_active( combo, idx );
	}

	priv->type = g_strdup( ofo_account_get_type_account( priv->account ));
	get_radio_root_detail( self, &root_btn, &detail_btn );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( root_btn ), FALSE );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( detail_btn ), FALSE );

	if( priv->type && g_utf8_strlen( priv->type, -1 )){
		if( !g_utf8_collate( priv->type, "R" )){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( root_btn ), TRUE );
		} else {
			g_assert( !g_utf8_collate( priv->type, "D" ));
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( detail_btn ), TRUE );
		}
	}
	g_signal_connect(
			G_OBJECT( root_btn ), "toggled", G_CALLBACK( on_root_toggled ), dialog );
	g_signal_connect(
			G_OBJECT( detail_btn ), "toggled", G_CALLBACK( on_detail_toggled ), dialog );
	if( !priv->type || !g_utf8_strlen( priv->type, -1 )){
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( detail_btn ), TRUE );
	}

	priv->deb_mnt = ofo_account_get_deb_mnt( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-deb-mnt" ));
	str = g_strdup_printf( "%.2f €", priv->deb_mnt );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->deb_ecr = ofo_account_get_deb_ecr( priv->account );
	label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( dialog->prot->dialog ), "p2-deb-ecr" ));
	if( priv->deb_ecr ){
		str = g_strdup_printf( "%u", priv->deb_ecr );
	} else {
		str = g_strdup( "" );
	}
	gtk_label_set_text( label, str );
	g_free( str );

	memcpy( &priv->deb_date, ofo_account_get_deb_date( priv->account ), sizeof( GDate ));
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-deb-date" ));
	str = my_utils_display_from_date( &priv->deb_date, MY_UTILS_DATE_DMMM );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->cre_mnt = ofo_account_get_cre_mnt( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-cre-mnt" ));
	str = g_strdup_printf( "%.2f €", priv->cre_mnt );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->cre_ecr = ofo_account_get_cre_ecr( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-cre-ecr" ));
	if( priv->cre_ecr ){
		str = g_strdup_printf( "%u", priv->cre_ecr );
	} else {
		str = g_strdup( "" );
	}
	gtk_label_set_text( label, str );
	g_free( str );

	memcpy( &priv->cre_date, ofo_account_get_cre_date( priv->account ), sizeof( GDate ));
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-cre-date" ));
	str = my_utils_display_from_date( &priv->cre_date, MY_UTILS_DATE_DMMM );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->bro_deb_mnt = ofo_account_get_bro_deb_mnt( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-bro-deb-mnt" ));
	str = g_strdup_printf( "%.2f €", priv->bro_deb_mnt );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->bro_deb_ecr = ofo_account_get_bro_deb_ecr( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-bro-deb-ecr" ));
	if( priv->bro_deb_ecr ){
		str = g_strdup_printf( "%u", priv->bro_deb_ecr );
	} else {
		str = g_strdup( "" );
	}
	gtk_label_set_text( label, str );
	g_free( str );

	memcpy( &priv->bro_deb_date, ofo_account_get_bro_deb_date( priv->account ), sizeof( GDate ));
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-bro-deb-date" ));
	str = my_utils_display_from_date( &priv->bro_deb_date, MY_UTILS_DATE_DMMM );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->bro_cre_mnt = ofo_account_get_bro_cre_mnt( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-bro-cre-mnt" ));
	str = g_strdup_printf( "%.2f €", priv->bro_cre_mnt );
	gtk_label_set_text( label, str );
	g_free( str );

	priv->bro_cre_ecr = ofo_account_get_bro_cre_ecr( priv->account );
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-bro-cre-ecr" ));
	if( priv->bro_cre_ecr ){
		str = g_strdup_printf( "%u", priv->bro_cre_ecr );
	} else {
		str = g_strdup( "" );
	}
	gtk_label_set_text( label, str );
	g_free( str );

	memcpy( &priv->bro_cre_date, ofo_account_get_bro_cre_date( priv->account ), sizeof( GDate ));
	label = GTK_LABEL(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( dialog->prot->dialog ), "p2-bro-cre-date" ));
	str = my_utils_display_from_date( &priv->bro_cre_date, MY_UTILS_DATE_DMMM );
	gtk_label_set_text( label, str );
	g_free( str );

	my_utils_init_notes_ex2( account );

	if( !priv->is_new ){
		my_utils_init_maj_user_stamp_ex2( account );
	}

	check_for_enable_dlg( OFA_ACCOUNT_PROPERTIES( dialog ));
}

static void
get_entry_number( ofaAccountProperties *self, GtkEntry **entry )
{
	g_return_if_fail( entry );
	*entry = GTK_ENTRY(
					my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-number" ));
}

static void
get_radio_root_detail( ofaAccountProperties *self, GtkRadioButton **root, GtkRadioButton **detail )
{
	g_return_if_fail( root );
	g_return_if_fail( detail );

	*root = GTK_RADIO_BUTTON(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-root-account" ));
	*detail = GTK_RADIO_BUTTON(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-detail-account" ));
}

static gboolean
v_account_properties_quit_on_ok( ofaBaseDialog *dialog )
{
	return( do_update( OFA_ACCOUNT_PROPERTIES( dialog )));
}

static void
on_number_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->private->number );
	self->private->number = g_strdup( gtk_entry_get_text( entry ));
	self->private->number_ok = FALSE;

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_devise_changed( GtkComboBox *box, ofaAccountProperties *self )
{
	static const gchar *thisfn = "ofa_account_properties_on_devise_changed";
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter, COL_ID, &self->private->devise, -1 );
		g_debug( "%s: devise changed to id=%d", thisfn, self->private->devise );
	}

	check_for_enable_dlg( self );
}

static void
on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	on_type_toggled( btn, self, "R" );
}

static void
on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self )
{
	on_type_toggled( btn, self, "D" );
}

static void
on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, const gchar *type )
{
	static const gchar *thisfn = "ofa_account_properties_on_type_toggled";

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( btn ))){
		g_debug( "%s: setting account type to %s", thisfn, type );
		g_free( self->private->type );
		self->private->type = g_strdup( type );
	}

	check_for_enable_dlg( self );
}

static void
check_for_enable_dlg( ofaAccountProperties *self )
{
	ofaAccountPropertiesPrivate *priv;
	gboolean vierge;
	gboolean is_root;
	GtkEntry *entry;
	GtkComboBox *combo;
	GtkRadioButton *root_btn, *detail_btn;
	GtkWidget *button;
	gboolean ok_enabled;

	priv = self->private;

	/* has this account already add some imputation ? */
	vierge = !priv->deb_mnt && !priv->cre_mnt && !priv->bro_deb_mnt && !priv->bro_cre_mnt;

	get_entry_number( self, &entry );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), vierge );

	is_root = ( priv->type && !g_utf8_collate( priv->type, "R" ));

	get_radio_root_detail( self, &root_btn, &detail_btn );

	if( priv->type && !g_utf8_collate( priv->type, "D" )){
		gtk_widget_set_sensitive( GTK_WIDGET( root_btn ), vierge );
		gtk_widget_set_sensitive( GTK_WIDGET( detail_btn ), vierge );
	} else {
		gtk_widget_set_sensitive( GTK_WIDGET( root_btn ), TRUE );
		gtk_widget_set_sensitive( GTK_WIDGET( detail_btn ), TRUE );
	}

	combo = GTK_COMBO_BOX(
				my_utils_container_get_child_by_name(
						GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "p1-devise" ));
	gtk_widget_set_sensitive( GTK_WIDGET( combo ), vierge && !is_root );

	ok_enabled = is_dialog_validable( self );
	button = my_utils_container_get_child_by_name(
							GTK_CONTAINER( OFA_BASE_DIALOG( self )->prot->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button, ok_enabled );
}

static gboolean
is_dialog_validable( ofaAccountProperties *self )
{
	gboolean ok;
	ofaAccountPropertiesPrivate *priv;
	ofoDossier *dossier;
	ofoAccount *exists;
	const gchar *prev;

	priv = self->private;

	ok = ofo_account_is_valid_data( priv->number, priv->label, priv->devise, priv->type );

	/* intrinsec validity is ok
	 * the number may have been modified ; the new number is acceptable
	 * if it doesn't exist yet, or has not been modified
	 * => we are refusing a new number which already exists and is for
	 *    another account
	 */
	if( ok && !self->private->number_ok ){

		dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));
		exists = ofo_account_get_by_number( dossier, self->private->number );
		prev = ofo_account_get_number( priv->account );
		self->private->number_ok = !exists || !g_utf8_collate( prev, priv->number );
		ok &= self->private->number_ok;
	}

	return( ok );
}

static gboolean
do_update( ofaAccountProperties *self )
{
	gchar *prev_number;
	ofoDossier *dossier;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	prev_number = g_strdup( ofo_account_get_number( self->private->account ));
	dossier = ofa_base_dialog_get_dossier( OFA_BASE_DIALOG( self ));

	ofo_account_set_number( self->private->account, self->private->number );
	ofo_account_set_label( self->private->account, self->private->label );
	ofo_account_set_type( self->private->account, self->private->type );
	ofo_account_set_devise( self->private->account, self->private->devise );
	my_utils_getback_notes_ex2( account );

	if( self->private->is_new ){
		self->private->updated =
				ofo_account_insert( self->private->account, dossier );
	} else {
		self->private->updated =
				ofo_account_update( self->private->account, dossier, prev_number );
	}

	g_free( prev_number );

	return( self->private->updated );
}
