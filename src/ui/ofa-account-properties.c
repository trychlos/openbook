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
#include "ui/ofa-account-properties.h"
#include "ui/ofo-dossier.h"
#include "ui/ofo-devise.h"

#if 0
static gboolean pref_quit_on_escape = TRUE;
static gboolean pref_confirm_on_cancel = FALSE;
static gboolean pref_confirm_on_escape = FALSE;
#endif

/* private class data
 */
struct _ofaAccountPropertiesClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaAccountPropertiesPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow *main_window;
	GtkDialog     *dialog;
	ofoAccount    *account;
	gboolean       updated;

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

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaAccountPropertiesClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaAccountProperties *self, ofaMainWindow *main, ofoAccount *account );
static gboolean  ok_to_terminate( ofaAccountProperties *self, gint code );
static void      on_number_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_label_changed( GtkEntry *entry, ofaAccountProperties *self );
static void      on_devise_changed( GtkComboBox *combo, ofaAccountProperties *self );
static void      on_root_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_detail_toggled( GtkRadioButton *btn, ofaAccountProperties *self );
static void      on_type_toggled( GtkRadioButton *btn, ofaAccountProperties *self, const gchar *type );
static void      check_for_enable_dlg( ofaAccountProperties *self );
static gboolean  do_update( ofaAccountProperties *self );
static void      error_duplicate( ofaAccountProperties *self, ofoAccount *existing );

GType
ofa_account_properties_get_type( void )
{
	static GType window_type = 0;

	if( !window_type ){
		window_type = register_type();
	}

	return( window_type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_account_properties_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaAccountPropertiesClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaAccountProperties ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaAccountProperties", &info, 0 );

	return( type );
}

static void
class_init( ofaAccountPropertiesClass *klass )
{
	static const gchar *thisfn = "ofa_account_properties_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaAccountPropertiesClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_account_properties_instance_init";
	ofaAccountProperties *self;

	g_return_if_fail( OFA_IS_ACCOUNT_PROPERTIES( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_ACCOUNT_PROPERTIES( instance );

	self->private = g_new0( ofaAccountPropertiesPrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->updated = FALSE;
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_account_properties_instance_dispose";
	ofaAccountPropertiesPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNT_PROPERTIES( window ));

	priv = ( OFA_ACCOUNT_PROPERTIES( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

		g_free( priv->number );
		g_free( priv->label );
		g_free( priv->type );
		g_free( priv->maj_user );

		gtk_widget_destroy( GTK_WIDGET( priv->dialog ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( window );
		}
	}
}

static void
instance_finalize( GObject *window )
{
	static const gchar *thisfn = "ofa_account_properties_instance_finalize";
	ofaAccountProperties *self;

	g_return_if_fail( OFA_IS_ACCOUNT_PROPERTIES( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_ACCOUNT_PROPERTIES( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
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
	gint code;
	gboolean updated;

	g_return_val_if_fail( OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p, account=%p",
			thisfn, ( void * ) main_window, ( void * ) account );

	self = g_object_new( OFA_TYPE_ACCOUNT_PROPERTIES, NULL );

	do_initialize_dialog( self, main_window, account );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	updated = self->private->updated;
	g_object_unref( self );

	return( updated );
}

static void
do_initialize_dialog( ofaAccountProperties *self, ofaMainWindow *main, ofoAccount *account )
{
	static const gchar *thisfn = "ofa_account_properties_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaAccountPropertiesPrivate *priv;
	gchar *title;
	const gchar *acc_number;
	GtkEntry *entry;
	GtkLabel *label;
	gchar *str;
	GtkComboBox *combo;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i, idx;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkRadioButton *root_btn, *detail_btn;
	GtkCellRenderer *text_cell;
	gchar *notes;
	ofoDossier *dossier;
	GList *devset, *idev;

	priv = self->private;
	priv->main_window = main;
	priv->account = account;

	/* create the GtkDialog */
	error = NULL;
	builder = gtk_builder_new();
	if( gtk_builder_add_from_file( builder, st_ui_xml, &error )){
		priv->dialog = GTK_DIALOG( gtk_builder_get_object( builder, st_ui_id ));
		if( !priv->dialog ){
			g_warning( "%s: unable to find '%s' object in '%s' file", thisfn, st_ui_id, st_ui_xml );
		}
	} else {
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
	}
	g_object_unref( builder );

	/* initialize the newly created dialog */
	if( priv->dialog ){

		/*gtk_window_set_transient_for( GTK_WINDOW( priv->dialog ), GTK_WINDOW( main ));*/

		acc_number = ofo_account_get_number( account );
		if( !acc_number ){
			title = g_strdup( _( "Defining a new account" ));
		} else {
			title = g_strdup_printf( _( "Updating account %s" ), acc_number );
		}
		gtk_window_set_title( GTK_WINDOW( priv->dialog ), title );

		priv->number = g_strdup( ofo_account_get_number( account ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-number" ));
		if( priv->number ){
			gtk_entry_set_text( entry, priv->number );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_number_changed ), self );

		priv->label = g_strdup( ofo_account_get_label( account ));
		entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-label" ));
		if( priv->label ){
			gtk_entry_set_text( entry, priv->label );
		}
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_label_changed ), self );

		priv->devise = ofo_account_get_devise( account );
		combo = GTK_COMBO_BOX( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-devise" ));

		model = GTK_TREE_MODEL( gtk_list_store_new(
				N_COLUMNS,
				G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING ));
		gtk_combo_box_set_model( combo, model );
		g_object_unref( model );

		text_cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", COL_DEVISE );

		text_cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), text_cell, FALSE );
		gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT( combo ), text_cell, "text", COL_LABEL );

		dossier = ofa_main_window_get_dossier( main );
		devset = ofo_devise_get_dataset( dossier );

		idx = -1;
		for( i=0, idev=devset ; idev ; ++i, idev=idev->next ){
			gtk_list_store_append( GTK_LIST_STORE( model ), &iter );
			gtk_list_store_set(
					GTK_LIST_STORE( model ),
					&iter,
					COL_ID,     ofo_devise_get_id( OFO_DEVISE( idev->data )),
					COL_DEVISE, ofo_devise_get_code( OFO_DEVISE( idev->data )),
					COL_LABEL,  ofo_devise_get_label( OFO_DEVISE( idev->data )),
					-1 );
			if( priv->devise == ofo_devise_get_id( OFO_DEVISE( idev->data ))){
				idx = i;
			}
		}
		g_signal_connect( G_OBJECT( combo ), "changed", G_CALLBACK( on_devise_changed ), self );
		if( idx == -1 ){
			gtk_combo_box_set_active( combo, idx );
		}

		priv->type = g_strdup( ofo_account_get_type_account( account ));
		root_btn = GTK_RADIO_BUTTON( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-root-account" ));
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( root_btn ), FALSE );
		detail_btn = GTK_RADIO_BUTTON( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-detail-account" ));
		gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( detail_btn ), FALSE );

		if( priv->type && g_utf8_strlen( priv->type, -1 )){
			if( !g_utf8_collate( priv->type, "R" )){
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( root_btn ), TRUE );
			} else {
				g_assert( !g_utf8_collate( priv->type, "D" ));
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( detail_btn ), TRUE );
			}
		}
		g_signal_connect( G_OBJECT( root_btn ), "toggled", G_CALLBACK( on_root_toggled ), self );
		g_signal_connect( G_OBJECT( detail_btn ), "toggled", G_CALLBACK( on_detail_toggled ), self );
		if( !priv->type || !g_utf8_strlen( priv->type, -1 )){
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( detail_btn ), TRUE );
		}

		priv->deb_mnt = ofo_account_get_deb_mnt( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-deb-mnt" ));
		str = g_strdup_printf( "%.2f €", priv->deb_mnt );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->deb_ecr = ofo_account_get_deb_ecr( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-deb-ecr" ));
		if( priv->deb_ecr ){
			str = g_strdup_printf( "%u", priv->deb_ecr );
		} else {
			str = g_strdup( "" );
		}
		gtk_label_set_text( label, str );
		g_free( str );

		memcpy( &priv->deb_date, ofo_account_get_deb_date( account ), sizeof( GDate ));
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-deb-date" ));
		str = my_utils_display_from_date( &priv->deb_date, MY_UTILS_DATE_DMMM );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->cre_mnt = ofo_account_get_cre_mnt( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-cre-mnt" ));
		str = g_strdup_printf( "%.2f €", priv->cre_mnt );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->cre_ecr = ofo_account_get_cre_ecr( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-cre-ecr" ));
		if( priv->cre_ecr ){
			str = g_strdup_printf( "%u", priv->cre_ecr );
		} else {
			str = g_strdup( "" );
		}
		gtk_label_set_text( label, str );
		g_free( str );

		memcpy( &priv->cre_date, ofo_account_get_cre_date( account ), sizeof( GDate ));
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-cre-date" ));
		str = my_utils_display_from_date( &priv->cre_date, MY_UTILS_DATE_DMMM );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->bro_deb_mnt = ofo_account_get_bro_deb_mnt( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-bro-deb-mnt" ));
		str = g_strdup_printf( "%.2f €", priv->bro_deb_mnt );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->bro_deb_ecr = ofo_account_get_bro_deb_ecr( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-bro-deb-ecr" ));
		if( priv->bro_deb_ecr ){
			str = g_strdup_printf( "%u", priv->bro_deb_ecr );
		} else {
			str = g_strdup( "" );
		}
		gtk_label_set_text( label, str );
		g_free( str );

		memcpy( &priv->bro_deb_date, ofo_account_get_bro_deb_date( account ), sizeof( GDate ));
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-bro-deb-date" ));
		str = my_utils_display_from_date( &priv->bro_deb_date, MY_UTILS_DATE_DMMM );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->bro_cre_mnt = ofo_account_get_bro_cre_mnt( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-bro-cre-mnt" ));
		str = g_strdup_printf( "%.2f €", priv->bro_cre_mnt );
		gtk_label_set_text( label, str );
		g_free( str );

		priv->bro_cre_ecr = ofo_account_get_bro_cre_ecr( account );
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-bro-cre-ecr" ));
		if( priv->bro_cre_ecr ){
			str = g_strdup_printf( "%u", priv->bro_cre_ecr );
		} else {
			str = g_strdup( "" );
		}
		gtk_label_set_text( label, str );
		g_free( str );

		memcpy( &priv->bro_cre_date, ofo_account_get_bro_cre_date( account ), sizeof( GDate ));
		label = GTK_LABEL( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p2-bro-cre-date" ));
		str = my_utils_display_from_date( &priv->bro_cre_date, MY_UTILS_DATE_DMMM );
		gtk_label_set_text( label, str );
		g_free( str );

		notes = g_strdup( ofo_account_get_notes( account ));
		if( notes ){
			text = GTK_TEXT_VIEW( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p3-notes" ));
			buffer = gtk_text_buffer_new( NULL );
			gtk_text_buffer_set_text( buffer, notes, -1 );
			gtk_text_view_set_buffer( text, buffer );
		}
	}

	check_for_enable_dlg( self );
	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaAccountProperties *self, gint code )
{
	gboolean quit = FALSE;

	switch( code ){
		case GTK_RESPONSE_NONE:
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_CANCEL:
			quit = TRUE;
			break;

		case GTK_RESPONSE_OK:
			quit = do_update( self );
			break;
	}

	return( quit );
}

static void
on_number_changed( GtkEntry *entry, ofaAccountProperties *self )
{
	g_free( self->private->number );
	self->private->number = g_strdup( gtk_entry_get_text( entry ));

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

	entry = GTK_ENTRY( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-number" ));
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), vierge );

	is_root = ( priv->type && !g_utf8_collate( priv->type, "R" ));
	root_btn = GTK_RADIO_BUTTON( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-root-account" ));
	detail_btn = GTK_RADIO_BUTTON( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-detail-account" ));
	if( priv->type && !g_utf8_collate( priv->type, "D" )){
		gtk_widget_set_sensitive( GTK_WIDGET( root_btn ), vierge );
		gtk_widget_set_sensitive( GTK_WIDGET( detail_btn ), vierge );
	} else {
		gtk_widget_set_sensitive( GTK_WIDGET( root_btn ), TRUE );
		gtk_widget_set_sensitive( GTK_WIDGET( detail_btn ), TRUE );
	}

	combo = GTK_COMBO_BOX( my_utils_container_get_child_by_name( GTK_CONTAINER( priv->dialog ), "p1-devise" ));
	gtk_widget_set_sensitive( GTK_WIDGET( combo ), vierge && !is_root );

	ok_enabled = ofo_account_is_valid_data( priv->number, priv->label, priv->devise, priv->type );
	button = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "btn-ok" );
	gtk_widget_set_sensitive( button, ok_enabled );
}

static gboolean
do_update( ofaAccountProperties *self )
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *prev_number;
	gchar *notes;
	ofoDossier *dossier;
	ofoAccount *existing;

	prev_number = g_strdup( ofo_account_get_number( self->private->account ));
	dossier = ofa_main_window_get_dossier( self->private->main_window );
	existing = ofo_account_get_by_number( dossier, self->private->number );

	if( existing ){
		/* c'est un nouveau compte, ou bien un compte existant dont on
		 * veut changer le numéro: no lunk, le nouveau numéro de compte
		 * existe déjà
		 */
		if( !prev_number || g_utf8_collate( prev_number, self->private->number )){
			error_duplicate( self, existing );
			g_free( prev_number );
			return( FALSE );
		}
	}

	/* le nouveau numéro de compte n'est pas encore utilisé,
	 * ou bien il est déjà utilisé par ce même compte (n'a pas été modifié)
	 */
	ofo_account_set_number( self->private->account, self->private->number );
	ofo_account_set_label( self->private->account, self->private->label );
	ofo_account_set_type( self->private->account, self->private->type );
	ofo_account_set_devise( self->private->account, self->private->devise );

	text = GTK_TEXT_VIEW(
			my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p3-notes" ));
	buffer = gtk_text_view_get_buffer( text );
	gtk_text_buffer_get_start_iter( buffer, &start );
	gtk_text_buffer_get_end_iter( buffer, &end );
	notes = gtk_text_buffer_get_text( buffer, &start, &end, TRUE );
	ofo_account_set_notes( self->private->account, notes );
	g_free( notes );

	if( !prev_number ){
		self->private->updated =
				ofo_account_insert( self->private->account, dossier );
	} else {
		self->private->updated =
				ofo_account_update( self->private->account, dossier, prev_number );
	}

	g_free( prev_number );

	return( self->private->updated );
}

static void
error_duplicate( ofaAccountProperties *self, ofoAccount *existing )
{
	GtkMessageDialog *dlg;
	gchar *msg;

	msg = g_strdup_printf(
			_( "Unable to set the account number to '%s' "
				"as this one is already used by the existing '%s'" ),
				ofo_account_get_number( existing ),
				ofo_account_get_label( existing ));

	dlg = GTK_MESSAGE_DIALOG( gtk_message_dialog_new(
				GTK_WINDOW( self->private->dialog ),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK,
				"%s", msg ));

	gtk_dialog_run( GTK_DIALOG( dlg ));
	gtk_widget_destroy( GTK_WIDGET( dlg ));
	g_free( msg );
}
