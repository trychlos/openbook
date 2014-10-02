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
#include "api/ofo-journal.h"

#include "core/my-window-prot.h"

#include "ui/ofa-devise-combo.h"
#include "ui/ofa-journal-properties.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaJournalPropertiesPrivate {

	/* internals
	 */
	ofoJournal     *journal;
	gboolean        is_new;
	gboolean        updated;

	/* page 2: balances display
	 */
	gint            exe_id;
	gchar          *dev_code;

	/* UI
	 */
	ofaDeviseCombo *dev_combo;

	/* data
	 */
	gchar          *mnemo;
	gchar          *label;
	gchar          *maj_user;
	GTimeVal        maj_stamp;
	GDate           cloture;
};

/* columns displayed in the exercice combobox
 */
enum {
	EXE_COL_BEGIN = 0,
	EXE_COL_END,
	EXE_COL_EXE_ID,
	EXE_N_COLUMNS
};

static const gchar  *st_ui_xml = PKGUIDIR "/ofa-journal-properties.ui";
static const gchar  *st_ui_id  = "JournalPropertiesDlg";

G_DEFINE_TYPE( ofaJournalProperties, ofa_journal_properties, MY_TYPE_DIALOG )

static void      v_init_dialog( myDialog *dialog );
static void      init_balances_page( ofaJournalProperties *self );
static void      on_mnemo_changed( GtkEntry *entry, ofaJournalProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaJournalProperties *self );
static void      on_exe_changed( GtkComboBox *box, ofaJournalProperties *self );
static void      on_devise_changed( const gchar *dev_code, ofaJournalProperties *self );
static void      display_balances( ofaJournalProperties *self );
static void      check_for_enable_dlg( ofaJournalProperties *self );
static gboolean  is_dialog_validable( ofaJournalProperties *self );
static gboolean  v_quit_on_ok( myDialog *dialog );
static gboolean  do_update( ofaJournalProperties *self );

static void
journal_properties_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_journal_properties_finalize";
	ofaJournalPropertiesPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_JOURNAL_PROPERTIES( instance ));

	priv = OFA_JOURNAL_PROPERTIES( instance )->private;

	/* free data members here */
	g_free( priv->dev_code );
	g_free( priv->mnemo );
	g_free( priv->label );
	g_free( priv->maj_user );
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journal_properties_parent_class )->finalize( instance );
}

static void
journal_properties_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_JOURNAL_PROPERTIES( instance ));

	if( !MY_WINDOW( instance )->protected->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_journal_properties_parent_class )->dispose( instance );
}

static void
ofa_journal_properties_init( ofaJournalProperties *self )
{
	static const gchar *thisfn = "ofa_journal_properties_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_JOURNAL_PROPERTIES( self ));

	self->private = g_new0( ofaJournalPropertiesPrivate, 1 );

	self->private->is_new = FALSE;
	self->private->updated = FALSE;
}

static void
ofa_journal_properties_class_init( ofaJournalPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_journal_properties_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = journal_properties_dispose;
	G_OBJECT_CLASS( klass )->finalize = journal_properties_finalize;

	MY_DIALOG_CLASS( klass )->init_dialog = v_init_dialog;
	MY_DIALOG_CLASS( klass )->quit_on_ok = v_quit_on_ok;
}

/**
 * ofa_journal_properties_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
gboolean
ofa_journal_properties_run( ofaMainWindow *main_window, ofoJournal *journal )
{
	static const gchar *thisfn = "ofa_journal_properties_run";
	ofaJournalProperties *self;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, journal=%p",
				thisfn, ( void * ) main_window, ( void * ) journal );

	self = g_object_new(
					OFA_TYPE_JOURNAL_PROPERTIES,
					MY_PROP_MAIN_WINDOW, main_window,
					MY_PROP_DOSSIER,     ofa_main_window_get_dossier( main_window ),
					MY_PROP_WINDOW_XML,  st_ui_xml,
					MY_PROP_WINDOW_NAME, st_ui_id,
					NULL );

	self->private->journal = journal;

	my_dialog_run_dialog( MY_DIALOG( self ));

	updated = self->private->updated;

	g_object_unref( self );

	return( updated );
}

static void
v_init_dialog( myDialog *dialog )
{
	ofaJournalPropertiesPrivate *priv;
	gchar *title;
	const gchar *jou_mnemo;
	GtkEntry *entry;
	GtkContainer *container;

	priv = OFA_JOURNAL_PROPERTIES( dialog )->private;
	container = GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( dialog )));

	jou_mnemo = ofo_journal_get_mnemo( priv->journal );
	if( !jou_mnemo ){
		priv->is_new = TRUE;
		title = g_strdup( _( "Defining a new journal" ));
	} else {
		title = g_strdup_printf( _( "Updating « %s » journal" ), jou_mnemo );
	}
	gtk_window_set_title( GTK_WINDOW( container ), title );

	priv->mnemo = g_strdup( jou_mnemo );
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-mnemo" ));
	if( priv->mnemo ){
		gtk_entry_set_text( entry, priv->mnemo );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_mnemo_changed ), dialog );

	priv->label = g_strdup( ofo_journal_get_label( priv->journal ));
	entry = GTK_ENTRY( my_utils_container_get_child_by_name( container, "p1-label" ));
	if( priv->label ){
		gtk_entry_set_text( entry, priv->label );
	}
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), dialog );

	init_balances_page( OFA_JOURNAL_PROPERTIES( dialog ));

	my_utils_init_notes_ex( container, journal );
	my_utils_init_maj_user_stamp_ex( container, journal );

	check_for_enable_dlg( OFA_JOURNAL_PROPERTIES( dialog ));
}

static void
init_balances_page( ofaJournalProperties *self )
{
	GtkContainer *container;
	ofaDeviseComboParms parms;
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
	parms.pfnSelected = ( ofaDeviseComboCb ) on_devise_changed;
	parms.user_data = self;
	parms.initial_code = ofo_dossier_get_default_devise( MY_WINDOW( self )->protected->dossier );

	self->private->dev_combo = ofa_devise_combo_new( &parms );

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

	list = ofo_journal_get_exe_list( self->private->journal );
	idx = -1;
	current_exe_id = ofo_dossier_get_current_exe_id( MY_WINDOW( self )->protected->dossier );

	for( ili=list, i=0 ; ili ; ili=ili->next, ++i ){
		exe_id = GPOINTER_TO_INT( ili->data );
		if( exe_id == current_exe_id ){
			idx = i;
		}
		begin = ofo_dossier_get_exe_deb( MY_WINDOW( self )->protected->dossier, exe_id );
		end = ofo_dossier_get_exe_fin( MY_WINDOW( self )->protected->dossier, exe_id );
		if( !begin || !g_date_valid( begin )){
			sbegin = g_strdup( "" );
			if( !end || !g_date_valid( end )){
				send = g_strdup( _( "Current exercice" ));
			} else {
				send = my_date2_to_str( end, MY_DATE_DMMM );
			}
		} else {
			sbegin = my_date2_to_str( begin, MY_DATE_DMMM );
			if( !end || !g_date_valid( end )){
				send = g_strdup( "" );
			} else {
				send = my_date2_to_str( end, MY_DATE_DMMM );
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
on_mnemo_changed( GtkEntry *entry, ofaJournalProperties *self )
{
	g_free( self->private->mnemo );
	self->private->mnemo = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_label_changed( GtkEntry *entry, ofaJournalProperties *self )
{
	g_free( self->private->label );
	self->private->label = g_strdup( gtk_entry_get_text( entry ));

	check_for_enable_dlg( self );
}

static void
on_exe_changed( GtkComboBox *box, ofaJournalProperties *self )
{
	GtkTreeModel *tmodel;
	GtkTreeIter iter;

	if( gtk_combo_box_get_active_iter( box, &iter )){
		tmodel = gtk_combo_box_get_model( box );
		gtk_tree_model_get( tmodel, &iter, EXE_COL_EXE_ID, &self->private->exe_id, -1 );
		display_balances( self );
	}
}

/*
 * ofaDeviseComboCb
 */
static void
on_devise_changed( const gchar *dev_code, ofaJournalProperties *self )
{
	g_free( self->private->dev_code );
	self->private->dev_code = g_strdup( dev_code );
	display_balances( self );
}

static void
display_balances( ofaJournalProperties *self )
{
	ofaJournalPropertiesPrivate *priv;
	GtkContainer *container;
	GtkLabel *label;
	gchar *str;

	priv = self->private;

	if( priv->exe_id <= 0 || !priv->dev_code || !g_utf8_strlen( priv->dev_code, -1 )){
		return;
	}

	container = ( GtkContainer * ) my_window_get_toplevel( MY_WINDOW( self ));
	g_return_if_fail( container && GTK_IS_CONTAINER( container ));

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-clo-deb" ));
	str = g_strdup_printf( "%'.2lf", ofo_journal_get_clo_deb( priv->journal, priv->exe_id, priv->dev_code ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-clo-cre" ));
	str = g_strdup_printf( "%'.2lf", ofo_journal_get_clo_cre( priv->journal, priv->exe_id, priv->dev_code ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-deb" ));
	str = g_strdup_printf( "%'.2lf", ofo_journal_get_deb( priv->journal, priv->exe_id, priv->dev_code ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-deb-date" ));
	str = my_date2_to_str( ofo_journal_get_deb_date( priv->journal, priv->exe_id, priv->dev_code ), MY_DATE_DMYY );
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-cre" ));
	str = g_strdup_printf( "%'.2lf", ofo_journal_get_cre( priv->journal, priv->exe_id, priv->dev_code ));
	gtk_label_set_text( label, str );
	g_free( str );

	label = GTK_LABEL( my_utils_container_get_child_by_name( container, "p2-cre-date" ));
	str = my_date2_to_str( ofo_journal_get_cre_date( priv->journal, priv->exe_id, priv->dev_code ), MY_DATE_DMYY );
	gtk_label_set_text( label, str );
	g_free( str );
}

static void
check_for_enable_dlg( ofaJournalProperties *self )
{
	GtkWidget *button;

	button = my_utils_container_get_child_by_name(
						GTK_CONTAINER( my_window_get_toplevel( MY_WINDOW( self ))),
						"btn-ok" );

	gtk_widget_set_sensitive( button, is_dialog_validable( self ));
}

static gboolean
is_dialog_validable( ofaJournalProperties *self )
{
	gboolean ok;
	ofaJournalPropertiesPrivate *priv;
	ofoJournal *exists;

	priv = self->private;

	ok = ofo_journal_is_valid( priv->mnemo, priv->label );

	if( ok ){
		exists = ofo_journal_get_by_mnemo(
				MY_WINDOW( self )->protected->dossier, priv->mnemo );
		ok &= !exists ||
				( !priv->is_new && !g_utf8_collate( priv->mnemo, ofo_journal_get_mnemo( priv->journal )));
	}

	return( ok );
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
v_quit_on_ok( myDialog *dialog )
{
	return( do_update( OFA_JOURNAL_PROPERTIES( dialog )));
}

/*
 * either creating a new journal (prev_mnemo is empty)
 * or updating an existing one, and prev_mnemo may have been modified
 */
static gboolean
do_update( ofaJournalProperties *self )
{
	ofaJournalPropertiesPrivate *priv;
	gchar *prev_mnemo;

	g_return_val_if_fail( is_dialog_validable( self ), FALSE );

	priv = self->private;
	prev_mnemo = g_strdup( ofo_journal_get_mnemo( priv->journal ));

	/* le nouveau mnemo n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même journal (n'a pas été modifié)
	 */
	ofo_journal_set_mnemo( priv->journal, priv->mnemo );
	ofo_journal_set_label( priv->journal, priv->label );
	my_utils_getback_notes_ex( my_window_get_toplevel( MY_WINDOW( self )), journal );

	if( priv->is_new ){
		priv->updated =
				ofo_journal_insert( priv->journal );
	} else {
		priv->updated =
				ofo_journal_update( priv->journal, prev_mnemo );
	}

	g_free( prev_mnemo );

	return( priv->updated );
}
