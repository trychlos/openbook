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
#include <stdlib.h>

#include "ui/my-utils.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-guided-input.h"
#include "ui/ofa-journal-combo.h"
#include "ui/ofo-dossier.h"

/* private class data
 */
struct _ofaGuidedInputClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaGuidedInputPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* internals
	 */
	ofaMainWindow  *main_window;
	GtkDialog      *dialog;
	const ofoModel *model;
	GtkGrid        *view;				/* entries view container */

	/* data
	 */
	gint            journal_id;
	GDate           dope;
	GDate           deff;
	gdouble         total_debits;
	gdouble         total_credits;
};

/*
 * columns in the grid view
 */
enum {
	COL_RANG = 0,
	FIRST_COLUMN,
	COL_ACCOUNT = FIRST_COLUMN,
	COL_LABEL,
	COL_DEBIT,
	COL_CREDIT,
	N_COLUMNS
};

/*
 * helpers
 */
typedef struct {
	gint	        column_id;
	gchar          *letter;
	const gchar * (*get_label)( const ofoModel *, gint );
	gboolean      (*is_locked)( const ofoModel *, gint );
	gint            width;
	float           xalign;
	gboolean        expand;
}
	sColumnDef;

#define TITLE_SIDE_MARGIN               6
#define AMOUNTS_WIDTH                  10
#define RANG_WIDTH                      3
#define TOTAUX_TOP_MARGIN               8

/*
 * this works because column_id is greater than zero
 * and this is ok because the column #0 is used by the number of the row
 */
static sColumnDef st_col_defs[] = {
		{ COL_ACCOUNT,
				"A",
				ofo_model_get_detail_account,
				ofo_model_get_detail_account_locked, 10, 0.0, FALSE },
		{ COL_LABEL,
				"L",
				ofo_model_get_detail_label,
				ofo_model_get_detail_label_locked,   20, 0.0, TRUE },
		{ COL_DEBIT,
				"D",
				ofo_model_get_detail_debit,
				ofo_model_get_detail_debit_locked,   AMOUNTS_WIDTH, 1.0, FALSE },
		{ COL_CREDIT,
				"C",
				ofo_model_get_detail_credit,
				ofo_model_get_detail_credit_locked,  AMOUNTS_WIDTH, 1.0, FALSE },
		{ 0 }
};

#define DATA_ENTRY_LEFT				"data-entry-left"
#define DATA_ENTRY_TOP				"data-entry-top"

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-guided-input.ui";
static const gchar  *st_ui_id        = "GuidedInputDlg";

static GObjectClass *st_parent_class = NULL;

static GType     register_type( void );
static void      class_init( ofaGuidedInputClass *klass );
static void      instance_init( GTypeInstance *instance, gpointer klass );
static void      instance_dispose( GObject *instance );
static void      instance_finalize( GObject *instance );
static void      do_initialize_dialog( ofaGuidedInput *self, ofaMainWindow *main, const ofoModel *model );
static gboolean  ok_to_terminate( ofaGuidedInput *self, gint code );
static void      init_dialog_journal( ofaGuidedInput *self );
static void      init_dialog_entries( ofaGuidedInput *self );
static void      add_row_entry( ofaGuidedInput *self, gint i );
static void      add_row_entry_set( ofaGuidedInput *self, gint col_id, gint row );
static const sColumnDef *find_column_def_from_col_id( ofaGuidedInput *self, gint col_id );
static const sColumnDef *find_column_def_from_letter( ofaGuidedInput *self, gchar letter );
static void      on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaGuidedInput *self );
static void      on_dope_changed( GtkEntry *entry, ofaGuidedInput *self );
static void      on_deffet_changed( GtkEntry *entry, ofaGuidedInput *self );
static void      on_entry_changed( GtkEntry *entry, ofaGuidedInput *self );
static gboolean  on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static gboolean  on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self );
static void      check_for_account( ofaGuidedInput *self, GtkEntry *entry  );
static void      check_for_enable_dlg( ofaGuidedInput *self );
static void      update_all_formulas( ofaGuidedInput *self );
static void      update_formula( ofaGuidedInput *self, const gchar *formula, GtkEntry *entry );
static gdouble   compute_formula_solde( ofaGuidedInput *self, gint column_id, gint row );
static void      update_all_totals( ofaGuidedInput *self );
static gdouble   get_amount( ofaGuidedInput *self, gint col_id, gint row );
static gboolean  check_for_journal( ofaGuidedInput *self );
static gboolean  check_for_dates( ofaGuidedInput *self );
static gboolean  check_for_all_entries( ofaGuidedInput *self );
static gboolean  check_for_entry( ofaGuidedInput *self, gint row );
static void      set_comment( ofaGuidedInput *self, const gchar *comment );
static gboolean  do_update( ofaGuidedInput *self );
static gboolean  do_update_with_entry( ofaGuidedInput *self, gint row, const gchar *piece );

GType
ofa_guided_input_get_type( void )
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
	static const gchar *thisfn = "ofa_guided_input_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaGuidedInputClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaGuidedInput ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaGuidedInput", &info, 0 );

	return( type );
}

static void
class_init( ofaGuidedInputClass *klass )
{
	static const gchar *thisfn = "ofa_guided_input_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaGuidedInputClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_guided_input_instance_init";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_GUIDED_INPUT( instance );

	self->private = g_new0( ofaGuidedInputPrivate, 1 );

	self->private->dispose_has_run = FALSE;

	self->private->journal_id = -1;
	g_date_clear( &self->private->dope, 1 );
	g_date_clear( &self->private->deff, 1 );
}

static void
instance_dispose( GObject *window )
{
	static const gchar *thisfn = "ofa_guided_input_instance_dispose";
	ofaGuidedInputPrivate *priv;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( window ));

	priv = ( OFA_GUIDED_INPUT( window ))->private;

	if( !priv->dispose_has_run ){
		g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

		priv->dispose_has_run = TRUE;

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
	static const gchar *thisfn = "ofa_guided_input_instance_finalize";
	ofaGuidedInput *self;

	g_return_if_fail( OFA_IS_GUIDED_INPUT( window ));

	g_debug( "%s: window=%p (%s)", thisfn, ( void * ) window, G_OBJECT_TYPE_NAME( window ));

	self = OFA_GUIDED_INPUT( window );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( window );
	}
}

/**
 * ofa_guided_input_run:
 * @main: the main window of the application.
 *
 * Update the properties of an journal
 */
void
ofa_guided_input_run( ofaMainWindow *main_window, const ofoModel *model )
{
	static const gchar *thisfn = "ofa_guided_input_run";
	ofaGuidedInput *self;
	gint code;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p, journal=%p",
			thisfn, ( void * ) main_window, ( void * ) model );

	self = g_object_new( OFA_TYPE_GUIDED_INPUT, NULL );

	do_initialize_dialog( self, main_window, model );

	g_debug( "%s: call gtk_dialog_run", thisfn );
	do {
		code = gtk_dialog_run( self->private->dialog );
		g_debug( "%s: gtk_dialog_run code=%d", thisfn, code );
		/* pressing Escape key makes gtk_dialog_run returns -4 GTK_RESPONSE_DELETE_EVENT */
	}
	while( !ok_to_terminate( self, code ));

	g_object_unref( self );
}

static void
do_initialize_dialog( ofaGuidedInput *self, ofaMainWindow *main, const ofoModel *model )
{
	static const gchar *thisfn = "ofa_guided_input_do_initialize_dialog";
	GError *error;
	GtkBuilder *builder;
	ofaGuidedInputPrivate *priv;
	GtkWidget *entry;

	priv = self->private;
	priv->main_window = main;
	priv->model = model;

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

		init_dialog_journal( self );

		entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-dope" );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_dope_changed ), self );

		entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-deffet" );
		g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_deffet_changed ), self );

		init_dialog_entries( self );

		check_for_enable_dlg( self );
	}

	gtk_widget_show_all( GTK_WIDGET( priv->dialog ));
}

/*
 * return %TRUE to allow quitting the dialog
 */
static gboolean
ok_to_terminate( ofaGuidedInput *self, gint code )
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
init_dialog_journal( ofaGuidedInput *self )
{
	GtkWidget *combo;
	ofaJournalComboParms parms;

	self->private->journal_id = ofo_model_get_journal( self->private->model );

	parms.dialog = self->private->dialog;
	parms.dossier = ofa_main_window_get_dossier( self->private->main_window );
	parms.combo_name = "p1-journal";
	parms.label_name = NULL;
	parms.disp_mnemo = FALSE;
	parms.disp_label = TRUE;
	parms.pfn = ( ofaJournalComboCb ) on_journal_changed;
	parms.user_data = self;
	parms.initial_id = self->private->journal_id;

	ofa_journal_combo_init_dialog( &parms );

	combo = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-journal" );
	g_return_if_fail( combo && GTK_IS_COMBO_BOX( combo ));

	gtk_widget_set_sensitive( combo, !ofo_model_get_journal_locked( self->private->model ));
}

static void
init_dialog_entries( ofaGuidedInput *self )
{
	GtkWidget *view;
	gint count,i;
	GtkLabel *label;
	GtkEntry *entry;

	view = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-entries" );
	g_return_if_fail( view && GTK_IS_GRID( view ));
	self->private->view = GTK_GRID( view );

	label = GTK_LABEL( gtk_label_new( _( "Account" )));
	gtk_widget_set_margin_left( GTK_WIDGET( label ), TITLE_SIDE_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_ACCOUNT, 0, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Entry label" )));
	gtk_widget_set_margin_left( GTK_WIDGET( label ), TITLE_SIDE_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_LABEL, 0, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Debit" )));
	gtk_widget_set_margin_right( GTK_WIDGET( label ), TITLE_SIDE_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_DEBIT, 0, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Credit" )));
	gtk_widget_set_margin_right( GTK_WIDGET( label ), TITLE_SIDE_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_CREDIT, 0, 1, 1 );

	count = ofo_model_get_detail_count( self->private->model );
	for( i=0 ; i<count ; ++i ){
		add_row_entry( self, i );
	}

	label = GTK_LABEL( gtk_label_new( _( "Total :" )));
	gtk_widget_set_margin_top( GTK_WIDGET( label ), TOTAUX_TOP_MARGIN );
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_LABEL, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_DEBIT, count+1, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_widget_set_margin_top( GTK_WIDGET( entry ), TOTAUX_TOP_MARGIN );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_CREDIT, count+1, 1, 1 );

	label = GTK_LABEL( gtk_label_new( _( "Diff :" )));
	gtk_misc_set_alignment( GTK_MISC( label ), 1.0, 0.5 );
	gtk_grid_attach( self->private->view, GTK_WIDGET( label ), COL_LABEL, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_DEBIT, count+2, 1, 1 );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_width_chars( entry, AMOUNTS_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_CREDIT, count+2, 1, 1 );
}

static void
add_row_entry( ofaGuidedInput *self, gint i )
{
	GtkEntry *entry;
	gchar *str;

	/* col #0: rang: number of the entry */
	str = g_strdup_printf( "%2d", i+1 );
	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), FALSE );
	gtk_entry_set_alignment( entry, 1.0 );
	gtk_entry_set_text( entry, str );
	gtk_entry_set_width_chars( entry, RANG_WIDTH );
	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), COL_RANG, i+1, 1, 1 );
	g_free( str );

	/* other columns starting with COL_ACCOUNT=1 */
	add_row_entry_set( self, COL_ACCOUNT, i+1 );
	add_row_entry_set( self, COL_LABEL, i+1 );
	add_row_entry_set( self, COL_DEBIT, i+1 );
	add_row_entry_set( self, COL_CREDIT, i+1 );
}

static void
add_row_entry_set( ofaGuidedInput *self, gint col_id, gint row )
{
	GtkEntry *entry;
	const sColumnDef *col_def;
	const gchar *str;
	gboolean locked;

	col_def = find_column_def_from_col_id( self, col_id );
	g_return_if_fail( col_def );

	entry = GTK_ENTRY( gtk_entry_new());
	gtk_widget_set_hexpand( GTK_WIDGET( entry ), col_def->expand );
	gtk_entry_set_width_chars( entry, col_def->width );
	gtk_entry_set_alignment( entry, col_def->xalign );

	str = (*col_def->get_label)( self->private->model, row-1 );

	if( str && !ofo_model_detail_is_formula( str )){
		gtk_entry_set_text( entry, str );
	}

	locked = (*col_def->is_locked)( self->private->model, row-1 );
	gtk_widget_set_sensitive( GTK_WIDGET( entry ), !locked );

	g_object_set_data( G_OBJECT( entry ), DATA_ENTRY_LEFT, GINT_TO_POINTER( col_id ));
	g_object_set_data( G_OBJECT( entry ), DATA_ENTRY_TOP, GINT_TO_POINTER( row ));

	if( !locked ){
		g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_entry_changed ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-in-event", G_CALLBACK( on_entry_focus_in ), self );
		g_signal_connect( G_OBJECT( entry ), "focus-out-event", G_CALLBACK( on_entry_focus_out ), self );
	}

	gtk_grid_attach( self->private->view, GTK_WIDGET( entry ), col_id, row, 1, 1 );
}

static const sColumnDef *
find_column_def_from_col_id( ofaGuidedInput *self, gint col_id )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].column_id == col_id ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

static const sColumnDef *
find_column_def_from_letter( ofaGuidedInput *self, gchar letter )
{
	gint i;

	for( i=0 ; st_col_defs[i].column_id ; ++i ){
		if( st_col_defs[i].letter[0] == letter ){
			return(( const sColumnDef * ) &st_col_defs[i] );
		}
	}

	return( NULL );
}

/*
 * ofaJournalCombo callback
 */
static void
on_journal_changed( gint id, const gchar *mnemo, const gchar *label, ofaGuidedInput *self )
{
	self->private->journal_id = id;

	check_for_enable_dlg( self );
}

static void
on_dope_changed( GtkEntry *entry, ofaGuidedInput *self )
{
	gchar *str, *comment;
	GtkWidget *wdeff;

	g_date_set_parse( &self->private->dope, gtk_entry_get_text( entry ));

	str = my_utils_display_from_date( &self->private->dope, MY_UTILS_DATE_DMMM );
	comment = g_strdup_printf( "Operation date: %s", str );
	set_comment( self, comment );
	g_free( comment );
	g_free( str );

	if( g_date_valid( &self->private->dope )){
		str = my_utils_display_from_date( &self->private->dope, MY_UTILS_DATE_DDMM );
		wdeff = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-deffet" );
		if( entry && GTK_IS_WIDGET( wdeff )){
			gtk_entry_set_text( GTK_ENTRY( wdeff ), str );
		}
		g_free( str );
	}

	check_for_enable_dlg( self );
}

static void
on_deffet_changed( GtkEntry *entry, ofaGuidedInput *self )
{
	gchar *str, *comment;

	g_date_set_parse( &self->private->deff, gtk_entry_get_text( entry ));

	str = my_utils_display_from_date( &self->private->deff, MY_UTILS_DATE_DMMM );
	comment = g_strdup_printf( "Effect date: %s", str );
	set_comment( self, comment );
	g_free( comment );
	g_free( str );

	check_for_enable_dlg( self );
}

static void
on_entry_changed( GtkEntry *entry, ofaGuidedInput *self )
{
	check_for_enable_dlg( self );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_in( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	gint row;

	row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_TOP ));
	if( row > 0 ){
		set_comment( self, ofo_model_get_detail_comment( self->private->model, row-1 ));
	}

	return( FALSE );
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_entry_focus_out( GtkEntry *entry, GdkEvent *event, ofaGuidedInput *self )
{
	gint left;

	/* check the entry we are leaving
	 */
	left = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_LEFT ));

	switch( left ){
		case COL_ACCOUNT:
			check_for_account( self, entry );
			break;
		case COL_LABEL:
		case COL_DEBIT:
		case COL_CREDIT:
			break;
	}

	set_comment( self, "" );

	return( FALSE );
}

/*
 * check that the account exists
 * else open a dialog for selection
 */
static void
check_for_account( ofaGuidedInput *self, GtkEntry *entry  )
{
	ofoDossier *dossier;
	ofoAccount *account;
	const gchar *asked_account;
	gchar *number;

	dossier = ofa_main_window_get_dossier( self->private->main_window );
	asked_account = gtk_entry_get_text( entry );
	account = ofo_dossier_get_account( dossier, asked_account );
	if( !account || ofo_account_is_root( account )){
		number = ofa_account_select_run( self->private->main_window, asked_account );
		if( number ){
			gtk_entry_set_text( entry, number );
			g_free( number );
		}
	}
}

/*
 * this is called after each field changes
 * so a good place to handle all modifications
 *
 * Note that we control *all* fields so that we are able to visually
 * highlight the erroneous ones
 */
static void
check_for_enable_dlg( ofaGuidedInput *self )
{
	gboolean ok;
	GtkWidget *btn;

	if( self->private->view ){
		g_return_if_fail( GTK_IS_GRID( self->private->view ));

		update_all_formulas( self );
		update_all_totals( self );

		ok = TRUE;
		ok &= check_for_journal( self );
		ok &= check_for_dates( self );
		ok &= check_for_all_entries( self );

		btn = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "btn-ok" );
		if( btn && GTK_IS_BUTTON( btn )){
			gtk_widget_set_sensitive( btn, ok );
		}
	}
}

static void
update_all_formulas( ofaGuidedInput *self )
{
	gint count, idx, col_id;
	const sColumnDef *col_def;
	const gchar *str;
	GtkWidget *entry;

	count = ofo_model_get_detail_count( self->private->model );
	for( idx=0 ; idx<count ; ++idx ){
		for( col_id=FIRST_COLUMN ; col_id<N_COLUMNS ; ++col_id ){
			col_def = find_column_def_from_col_id( self, col_id );
			if( col_def ){
				str = ( *col_def->get_label )( self->private->model, idx );
				if( ofo_model_detail_is_formula( str )){
					entry = gtk_grid_get_child_at( self->private->view, col_id, idx+1 );
					if( entry && GTK_IS_ENTRY( entry )){
						update_formula( self, str, GTK_ENTRY( entry ));
					}
				}
			}
		}
	}
}

static void
update_formula( ofaGuidedInput *self, const gchar *formula, GtkEntry *entry )
{
	static const gchar *thisfn = "ofa_guided_input_update_formula";
	gchar **tokens, **iter;
	gchar init;
	ofoTaux *rate;
	gdouble solde;
	gchar *str;
	gint col, row;
	GtkWidget *widget;
	const sColumnDef *col_def;

	tokens = g_regex_split_simple( "[-+*/]", formula+1, 0, 0 );
	iter = tokens;
	g_debug( "%s: formula='%s'", thisfn, formula );
	while( *iter ){
		/*g_debug( "%s: iter='%s'", thisfn, *iter );*/
		/*
		 * we have:
		 * - [ALDC]<number>
		 *    A: account
		 *     L: label
		 *      D: debit
		 *       C: credit
		 * or:
		 * - a token:
		 *   SOLDE
		 *   IDEM (same column, previous row)
		 * or:
		 * - a rate mnemonic
		 */
		if( !g_utf8_collate( *iter, "SOLDE" )){
			col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_LEFT ));
			row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_TOP ));
			solde = compute_formula_solde( self, col, row );
			str = g_strdup_printf( "%.2lf", solde );
			gtk_entry_set_text( entry, str );
			g_free( str );

		} else if( !g_utf8_collate( *iter, "IDEM" )){
			row = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_TOP ));
			col = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( entry ), DATA_ENTRY_LEFT ));
			widget = gtk_grid_get_child_at( self->private->view, col, row-1 );
			if( widget && GTK_IS_ENTRY( widget )){
				gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
			}

		} else {
			init = *iter[0];
			row = atoi(( *iter )+1 );
			/*g_debug( "%s: init=%c row=%d", thisfn, init, row );*/
			if( row > 0 && row <= ofo_model_get_detail_count( self->private->model )){
				col_def = find_column_def_from_letter( self, init );
				if( col_def ){
					widget = gtk_grid_get_child_at( self->private->view, col_def->column_id, row );
					if( widget && GTK_IS_ENTRY( widget )){
						gtk_entry_set_text( entry, gtk_entry_get_text( GTK_ENTRY( widget )));
					} else {
						g_warning( "%s: no entry found at col=%d, row=%d", thisfn, col_def->column_id, row );
					}
				} else {
					g_warning( "%s: col_def not found for '%c' letter", thisfn, init );
				}

			} else {
				g_debug( "%s: searching for taux %s", thisfn, *iter );
				rate = ofo_dossier_get_taux(
						ofa_main_window_get_dossier( self->private->main_window ), *iter, NULL );
				if( rate && OFO_IS_TAUX( rate )){
					g_warning( "%s: TODO", thisfn );
				} else {
					g_warning( "%s: taux %s not found", thisfn, *iter );
				}
			}
		}
		iter++;
	}

	g_strfreev( tokens );
}

static gdouble
compute_formula_solde( ofaGuidedInput *self, gint column_id, gint row )
{
	gdouble dsold, csold;
	gint count, idx;

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		if( column_id != COL_DEBIT || row != idx+1 ){
			dsold += get_amount( self, COL_DEBIT, idx+1 );
		}
		if( column_id != COL_CREDIT || row != idx+1 ){
			csold += get_amount( self, COL_CREDIT, idx+1 );
		}
	}

	return( column_id == COL_DEBIT ? csold-dsold : dsold-csold );
}

/*
 * totals and diffs are set are rows (count+1) and (count+2) respectively
 */
static void
update_all_totals( ofaGuidedInput *self )
{
	gdouble dsold, csold;
	gint count, idx;
	GtkWidget *entry;
	gchar *str, *str2;

	count = ofo_model_get_detail_count( self->private->model );
	csold = 0.0;
	dsold = 0.0;
	for( idx=0 ; idx<count ; ++idx ){
		dsold += get_amount( self, COL_DEBIT, idx+1 );
		csold += get_amount( self, COL_CREDIT, idx+1 );
	}

	self->private->total_debits = dsold;
	self->private->total_credits = csold;

	entry = gtk_grid_get_child_at( self->private->view, COL_DEBIT, count+1 );
	if( entry && GTK_IS_ENTRY( entry )){
		str = g_strdup_printf( "%.2lf", dsold );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_CREDIT, count+1 );
	if( entry && GTK_IS_ENTRY( entry )){
		str = g_strdup_printf( "%.2lf", csold );
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
		g_free( str );
	}

	if( dsold > csold ){
		str = g_strdup_printf( "%.2lf", dsold-csold );
		str2 = g_strdup( "" );
	} else if( dsold < csold ){
		str = g_strdup( "" );
		str2 = g_strdup_printf( "%.2lf", csold-dsold );
	} else {
		str = g_strdup( "" );
		str2 = g_strdup( "" );
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_DEBIT, count+2 );
	if( entry && GTK_IS_ENTRY( entry )){
		gtk_entry_set_text( GTK_ENTRY( entry ), str );
	}

	entry = gtk_grid_get_child_at( self->private->view, COL_CREDIT, count+2 );
	if( entry && GTK_IS_ENTRY( entry )){
		gtk_entry_set_text( GTK_ENTRY( entry ), str2 );
	}

	g_free( str );
	g_free( str2 );
}

static gdouble
get_amount( ofaGuidedInput *self, gint col_id, gint row )
{
	const sColumnDef *col_def;
	GtkWidget *entry;

	col_def = find_column_def_from_col_id( self, col_id );
	if( col_def ){
		entry = gtk_grid_get_child_at( self->private->view, col_def->column_id, row );
		if( entry && GTK_IS_ENTRY( entry )){
			return( g_strtod( gtk_entry_get_text( GTK_ENTRY( entry )), NULL ));
		}
	}

	return( 0.0 );
}

/*
 * Returns TRUE if a journal is set
 */
static gboolean
check_for_journal( ofaGuidedInput *self )
{
	return( self->private->journal_id > 0 );
}

/*
 * Returns TRUE if the dates are set
 */
static gboolean
check_for_dates( ofaGuidedInput *self )
{
	gboolean ok, oki;
	GtkWidget *entry;

	ok = TRUE;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-dope" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	oki = g_date_valid( &self->private->dope );
	my_utils_entry_set_valid( GTK_ENTRY( entry ), oki );
	ok &= oki;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-deffet" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	oki = g_date_valid( &self->private->deff );
	my_utils_entry_set_valid( GTK_ENTRY( entry ), oki );
	ok &= oki;

	return( ok );
}

/*
 * Returns TRUE if the entries are valid:
 * - for entries which have a not-nul balance:
 *   > account is valid
 *   > label is set
 * - totals are the same (no diff) and not nuls
 *
 * Note that have to check *all* entries in order to be able to visually
 * highlight the erroneous fields
 */
static gboolean
check_for_all_entries( ofaGuidedInput *self )
{
	gboolean ok;
	gint count, idx;
	gdouble deb, cred;

	ok = TRUE;
	count = ofo_model_get_detail_count( self->private->model );

	for( idx=0 ; idx<count ; ++idx ){
		deb = get_amount( self, COL_DEBIT, idx+1 );
		cred = get_amount( self, COL_CREDIT, idx+1 );
		if( deb+cred != 0.0 ){
			if( !check_for_entry( self, idx+1 )){
				ok = FALSE;
			}
		}
	}

	ok &= (self->private->total_debits != 0.0 || self->private->total_credits != 0.0 );

	return( ok );
}

static gboolean
check_for_entry( ofaGuidedInput *self, gint row )
{
	gboolean ok;
	GtkWidget *entry;
	ofoAccount *account;
	const gchar *str;

	ok = TRUE;

	entry = gtk_grid_get_child_at( self->private->view, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );

	account = ofo_dossier_get_account(
			ofa_main_window_get_dossier( self->private->main_window ),
			gtk_entry_get_text( GTK_ENTRY( entry )));
	ok &= ( account && OFO_IS_ACCOUNT( account ));

	entry = gtk_grid_get_child_at( self->private->view, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	str = gtk_entry_get_text( GTK_ENTRY( entry ));
	ok &= ( str && g_utf8_strlen( str, -1 ));

	return( ok );
}

static void
set_comment( ofaGuidedInput *self, const gchar *comment )
{
	GtkWidget *widget;

	widget = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-comment" );
	if( widget && GTK_IS_ENTRY( widget )){
		gtk_entry_set_text( GTK_ENTRY( widget ), comment );
	}
}

/*
 * generate the entries
 */
static gboolean
do_update( ofaGuidedInput *self )
{
	gboolean ok;
	GtkWidget *entry;
	gint count, idx;
	gdouble deb, cred;
	const gchar *piece;

	ok = TRUE;

	piece = NULL;
	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( self->private->dialog ), "p1-piece" );
	if( entry && GTK_IS_ENTRY( entry )){
		piece = gtk_entry_get_text( GTK_ENTRY( entry ));
	}

	count = ofo_model_get_detail_count( self->private->model );

	for( idx=0 ; idx<count ; ++idx ){
		deb = get_amount( self, COL_DEBIT, idx+1 );
		cred = get_amount( self, COL_CREDIT, idx+1 );
		if( deb+cred != 0.0 ){
			if( !do_update_with_entry( self, idx+1, piece )){
				ok = FALSE;
			}
		}
	}

	return( ok );
}

/*
 * generate the entries
 */
static gboolean
do_update_with_entry( ofaGuidedInput *self, gint row, const gchar *piece )
{
	gboolean ok;
	GtkWidget *entry;
	const gchar *account_number;
	ofoAccount *account;
	const gchar *label;
	gdouble deb, cre, amount;
	ofaEntrySens sens;

	entry = gtk_grid_get_child_at( self->private->view, COL_ACCOUNT, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	account_number = gtk_entry_get_text( GTK_ENTRY( entry ));
	account = ofo_dossier_get_account(
					ofa_main_window_get_dossier( self->private->main_window ),
					account_number );
	g_return_val_if_fail( account && OFO_IS_ACCOUNT( account ), FALSE );

	entry = gtk_grid_get_child_at( self->private->view, COL_LABEL, row );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), FALSE );
	label = gtk_entry_get_text( GTK_ENTRY( entry ));
	g_return_val_if_fail( label && g_utf8_strlen( label, -1 ), FALSE );

	deb = get_amount( self, COL_DEBIT, row );
	cre = get_amount( self, COL_CREDIT, row );
	if( deb > cre ){
		amount = deb - cre;
		sens = ENT_SENS_DEBIT;
	} else {
		amount = cre - deb;
		sens = ENT_SENS_CREDIT;
	}

	ok = ofo_dossier_entry_insert(
			ofa_main_window_get_dossier( self->private->main_window ),
			&self->private->deff, &self->private->dope, label, piece,
			account_number,
			ofo_account_get_devise( account ),
			self->private->journal_id,
			amount, sens );

	return( ok );
}
