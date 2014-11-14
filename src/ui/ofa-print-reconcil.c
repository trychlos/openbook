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
#include "api/ofa-settings.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-print.h"
#include "ui/ofa-print-reconcil.h"

/* private instance data
 */
struct _ofaPrintReconcilPrivate {
	gboolean       dispose_has_run;

	/* initialization data
	 */
	ofaMainWindow *main_window;

	/* internals
	 */
	ofoDossier    *dossier;
	ofoAccount    *account;
	ofoCurrency   *currency;
	GDate          date;
	GList         *entries;
	gdouble        account_solde;

	/* UI
	 */
	GtkEntry      *account_entry;
	GtkLabel      *account_label;
	GtkWidget     *date_entry;
	GtkLabel      *date_label;

	/* other datas
	 */
	gdouble        page_width;
	gdouble        page_height;
	gint           pages_count;
	PangoLayout   *layout;
	gdouble        body_count_rtab;
	gdouble        body_effect_ltab;
	gdouble        body_ledger_ltab;
	gdouble        body_ref_ltab;
	gint           body_ref_max_size;		/* Pango units */
	gdouble        body_label_ltab;
	gint           body_label_max_size;		/* Pango units */
	gdouble        body_debit_rtab;
	gdouble        body_credit_rtab;
	gdouble        body_solde_rtab;
	gdouble        last_y;
	gint           last_entry;				/* last printed entry, from zero */
};

static const gchar  *st_ui_xml       = PKGUIDIR "/ofa-print-reconcil.piece.ui";

static const gchar  *st_pref_account = "PrintReconciliationAccount";
static const gchar  *st_pref_date    = "PrintReconciliationDate";

/* these are parms which describe the page layout
 */
/* makes use of the same font family for all fields */
static const gchar  *st_font_family                    = "Sans";
static const gint    st_body_font_size                 = 9;
/* the space between body lines */
#define st_body_line_spacing                             st_body_font_size*.5
/* as we use a white-on-cyan columns header, we keep a 2px left and right margins */
static const gint    st_page_margin                    = 2;

/* the columns of the body */
#define st_effect_width                                54/9*st_body_font_size
#define st_journal_width                               36/9*st_body_font_size
#define st_ref_width                                   64/9*st_body_font_size
#define st_amount_width                                90/9*st_body_font_size
#define st_column_spacing                              4
/*
(openbook:29799): OFA-DEBUG: '99/99/9999   ' width=61
(openbook:29799): OFA-DEBUG: 'XXXXXX   ' width=46   -> 107
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXX    ' width=71 ->
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXX' width=62
(openbook:29799): OFA-DEBUG: 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX   ' width=441
(openbook:29799): OFA-DEBUG: '   99 999 999,99' width=75
1 space ~ 3px
70 chars = 432 => 1'X' ~ 6.17 px
*/

#define COLOR_BLACK                 0,      0,      0
#define COLOR_WHITE                 1,      1,      1
#define COLOR_DARK_RED              0.5,    0,      0
#define COLOR_DARK_CYAN             0,      0.5156, 0.5156
#define COLOR_LIGHT_GRAY            0.9375, 0.9375, 0.9375
#define COLOR_GRAY                  0.6,    0.6,    0.6


G_DEFINE_TYPE( ofaPrintReconcil, ofa_print_reconcil, G_TYPE_OBJECT )

static gboolean     print_reconciliation_operate( ofaPrintReconcil *self );
static GObject     *on_create_custom_widget( GtkPrintOperation *operation, ofaPrintReconcil *self );
static void         on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintReconcil *self );
static void         widget_error( ofaPrintReconcil *self, const gchar *msg );
static void         on_account_changed( GtkEntry *entry, ofaPrintReconcil *self );
static void         on_account_select( GtkButton *button, ofaPrintReconcil *self );
static void         on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self );
static void         build_body_layout( ofaPrintReconcil *self, GtkPrintContext *context );
static void         on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintReconcil *self );
static void         draw_header( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gboolean is_last );
static void         draw_reconciliation_start( ofaPrintReconcil *self, GtkPrintContext *context );
static void         draw_line( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gint line_num, ofoEntry *entry );
static void         draw_reconciliation_end( ofaPrintReconcil *self, GtkPrintContext *context );
static gchar       *display_account_solde( ofaPrintReconcil *self, gdouble amount );
static void         on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self );

static void
print_reconcil_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_print_reconcil_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PRINT_RECONCIL( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_reconcil_parent_class )->finalize( instance );
}

static void
print_reconcil_dispose( GObject *instance )
{
	ofaPrintReconcilPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PRINT_RECONCIL( instance ));

	priv = OFA_PRINT_RECONCIL( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->entries ){
			ofo_entry_free_dataset( priv->entries );
			priv->entries = NULL;
		}
		if( priv->layout ){
			g_clear_object( &priv->layout );
		}
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_reconcil_parent_class )->dispose( instance );
}

static void
ofa_print_reconcil_init( ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_instance_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PRINT_RECONCIL( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PRINT_RECONCIL, ofaPrintReconcilPrivate );
	self->priv->dispose_has_run = FALSE;
	self->priv->entries = NULL;
	self->priv->last_entry = -1;
	my_date_clear( &self->priv->date );
}

static void
ofa_print_reconcil_class_init( ofaPrintReconcilClass *klass )
{
	static const gchar *thisfn = "ofa_print_reconcil_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = print_reconcil_dispose;
	G_OBJECT_CLASS( klass )->finalize = print_reconcil_finalize;

	g_type_class_add_private( klass, sizeof( ofaPrintReconcilPrivate ));
}

/**
 * ofa_print_reconcil_run:
 * @main: the main window of the application.
 *
 * Print the reconciliation summary
 */
gboolean
ofa_print_reconcil_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_print_reconcil_run";
	ofaPrintReconcil *self;
	gboolean printed;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_PRINT_RECONCIL, NULL );
	self->priv->main_window = main_window;
	self->priv->dossier = ofa_main_window_get_dossier( main_window );
	printed = print_reconciliation_operate( self );
	g_object_unref( self );

	return( printed );
}

/*
 * print_reconciliation_operate:
 *
 * run the GtkPrintOperation operation
 * returns %TRUE if the print has been successful
 */
static gboolean
print_reconciliation_operate( ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	gboolean printed;
	GtkWidget *dialog;
	gchar *str;
	GError *error;
	GtkPageSetup *psetup;
	GtkPaperSize *psize;

	priv = self->priv;
	printed = FALSE;
	error = NULL;
	print = gtk_print_operation_new ();

	/* unit_none gives width=559,2, height=783,5 */
	gtk_print_operation_set_unit( print, GTK_UNIT_NONE );
	gtk_print_operation_set_custom_tab_label( print, _( "Reconciliation Summary" ));

	g_signal_connect( print, "create-custom-widget", G_CALLBACK( on_create_custom_widget ), self );
	g_signal_connect( print, "custom-widget-apply", G_CALLBACK( on_custom_widget_apply ), self );
	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), self );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), self );
	g_signal_connect( print, "end-print", G_CALLBACK( on_end_print ), self );

	psize = gtk_paper_size_new( GTK_PAPER_NAME_A4 );
	psetup = gtk_page_setup_new();
	gtk_page_setup_set_paper_size( psetup, psize );
	gtk_paper_size_free( psize );
	gtk_page_setup_set_orientation( psetup, GTK_PAGE_ORIENTATION_LANDSCAPE );
	gtk_print_operation_set_default_page_setup( print, psetup );
	g_object_unref( psetup );

	res = gtk_print_operation_run(
					print,
					GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                GTK_WINDOW( priv->main_window ),
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){

		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		dialog = gtk_message_dialog_new(
							GTK_WINDOW( priv->main_window ),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"%s", str );
		g_free( str );
		g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
		gtk_widget_show( dialog );
		g_error_free( error );

	} else if( res == GTK_PRINT_OPERATION_RESULT_APPLY ){

		if( priv->pages_count <= 1 ){
			str = g_strdup_printf(
					_( "The Account Reconciliation Summary has been successfully printed\n"
							"(%u printed page)" ), priv->pages_count );
		} else {
			str = g_strdup_printf(
					_( "The Account Reconciliation Summary has been successfully printed\n"
							"(%u printed pages)" ), priv->pages_count );
		}
		dialog = gtk_message_dialog_new(
							GTK_WINDOW( priv->main_window ),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_INFO,
							GTK_BUTTONS_CLOSE,
							"%s", str );
		g_free( str );
		g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
		gtk_widget_show( dialog );

		printed = TRUE;
	}

	g_object_unref( print );

	return( printed );
}

static GObject*
on_create_custom_widget( GtkPrintOperation *operation, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_create_custom_widget";
	ofaPrintReconcilPrivate *priv;
	GtkWidget *box, *frame;
	GtkWidget *entry, *button, *label;
	gchar *text;

	g_debug( "%s: operation=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) self );

	priv = self->priv;

	box = my_utils_builder_load_from_path( st_ui_xml, "box-reconcil" );
	frame = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "frame-reconcil" );
	g_object_ref( frame );
	gtk_container_remove( GTK_CONTAINER( box ), frame );
	g_object_unref( box );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-entry" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );
	text = ofa_settings_get_string( st_pref_account );
	if( text && g_utf8_strlen( text, -1 )){
		gtk_entry_set_text( GTK_ENTRY( entry ), text );
	}
	g_free( text );
	priv->account_entry = GTK_ENTRY( entry );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-select" );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), NULL );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_account_select ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-label" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	priv->account_label = GTK_LABEL( label );

	priv->date_entry = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "date-entry" );
	my_editable_date_init( GTK_EDITABLE( priv->date_entry ));
	my_editable_date_set_format( GTK_EDITABLE( priv->date_entry ), MY_DATE_DMYY );
	text = ofa_settings_get_string( st_pref_date );
	my_date_set_from_sql( &priv->date, text );
	if( my_date_is_valid( &priv->date )){
		my_editable_date_set_date( GTK_EDITABLE( priv->date_entry ), &priv->date );
	}
	g_free( text );
	label = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "date-label" );
	my_editable_date_set_label( GTK_EDITABLE( priv->date_entry ), label, MY_DATE_DMMM );

	return( G_OBJECT( frame ));
}

static void
on_account_changed( GtkEntry *entry, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	const gchar *str;

	priv = self->priv;
	str = gtk_entry_get_text( entry );
	priv->account = ofo_account_get_by_number( priv->dossier, str );
	if( priv->account ){
		gtk_label_set_text( priv->account_label, ofo_account_get_label( priv->account ));
	} else {
		gtk_label_set_text( priv->account_label, "" );
	}
}

static void
on_account_select( GtkButton *button, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	gchar *number;

	priv = self->priv;
	number = ofa_account_select_run(
						priv->main_window,
						gtk_entry_get_text( priv->account_entry ));
	if( number ){
		gtk_entry_set_text( priv->account_entry, number );
		g_free( number );
	}
}

static void
on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	const gchar *mnemo;
	gchar *sdate;

	priv = self->priv;

	my_date_set_from_date( &priv->date,
			my_editable_date_get_date( GTK_EDITABLE( priv->date_entry ), NULL ));

	if( !priv->account || !OFO_IS_ACCOUNT( priv->account )){
		widget_error( self, _( "Invalid account\n"
				"The print operation will be canceled" ));
		gtk_print_operation_cancel( operation );

	} else if( !my_date_is_valid( &priv->date )){
		widget_error( self, _( "Invalid reconciliation date\n"
				"The print operation will be canceled" ));
		gtk_print_operation_cancel( operation );

	} else {
		sdate = my_date_to_str( &priv->date, MY_DATE_SQL );
		ofa_settings_set_string( st_pref_date, sdate );
		g_free( sdate );

		ofa_settings_set_string( st_pref_account, ofo_account_get_number( priv->account ));

		priv->entries = ofo_entry_get_dataset_for_print_reconcil(
								priv->dossier,
								ofo_account_get_number( priv->account ),
								&priv->date );

		mnemo = ofo_account_get_currency( priv->account );
		priv->currency = ofo_currency_get_by_code( priv->dossier, mnemo );
	}
}

static void
widget_error( ofaPrintReconcil *self, const gchar *msg )
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
						GTK_WINDOW( self->priv->main_window ),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"%s", msg );
	g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
	gtk_widget_show( dialog );
}

/*
 * mainly here: compute the count of printed pages, knowing that:
 * - the first page will display the starting reconciliation summary
 * - the last page (maybe the same) will display the ending summary
 */
static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_begin_print";
	ofaPrintReconcilPrivate *priv;
	gdouble reconcil_height_first, reconcil_height_last;
	gdouble header_height, header_height_other;
	gdouble line_height, footer_height;
	gint entries_count;
	gdouble need_height, avail_height;
	gint lpp_first, lpp_other, lines_rest;

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->priv;

	priv->page_width = gtk_print_context_get_width( context ) - 2*st_page_margin;
	priv->page_height = gtk_print_context_get_height( context );

	g_debug( "%s; context_width=%lf, context_height=%lf, page_width=%lf, page_height=%lf",
			thisfn,
			gtk_print_context_get_width( context ), gtk_print_context_get_height( context ),
			priv->page_width, priv->page_height );

	reconcil_height_first =
			st_body_line_spacing
			+ st_body_font_size+1
			+ st_body_line_spacing;

	reconcil_height_last =
			reconcil_height_first
			+ st_body_line_spacing
			+ 3*st_body_font_size;

	header_height =
			ofa_print_header_dossier_get_height( 1 )
			+ ofa_print_header_title_get_height( 1 )
			+ ofa_print_header_subtitle_get_height( 1 )
			+ 2*st_body_font_size;						/* column headers */

	header_height_other = header_height + st_body_line_spacing/2;
	line_height = st_body_font_size + st_body_line_spacing;
	footer_height = ofa_print_footer_get_height( 1, FALSE );

	entries_count = g_list_length( self->priv->entries );
	g_debug( "%s: entries_count=%u", thisfn, entries_count );

	/* do we can print all entries and reconciliation summaries on a
	 * single page ? */
	need_height =
			entries_count * line_height
			+ reconcil_height_first
			+ reconcil_height_last;

	avail_height =
			priv->page_height
			- header_height
			- footer_height;

	priv->pages_count = 1;

	/* if we have more than one page:
	 *  remove the 'lpp_first' lines displayed on first page
	 *  recompute the available height on a standard page
	 *  compute the maximum line per page on a standard page */
	if( need_height > avail_height ){
		lpp_first = ( avail_height - reconcil_height_first ) / line_height;
		avail_height += header_height - header_height_other;
		lpp_other = avail_height / line_height;
		lines_rest = entries_count - lpp_first;
		while( TRUE ){
			priv->pages_count += 1;
			if( lines_rest < lpp_other ){
				need_height = ( lines_rest * line_height ) + reconcil_height_last;
				if( need_height <= avail_height ){
					break;
				}
			}
			lines_rest -= lpp_other;
		}
	}

	gtk_print_operation_set_n_pages( operation, priv->pages_count );

	/*g_debug( "%s: context_height=%lf, st_body_font_size=%d, st_header_part=%lf, st_footer_part=%lf, st_body_line_spacing=%lf, lines_per_page=%d",
				thisfn, context_height, st_body_font_size, st_header_part, st_footer_part, st_body_line_spacing, priv->lines_per_page );
	g_debug( "%s: entries_count=%d, pages_count=%d", thisfn, entries_count, priv->pages_count );
	context_height=783,569764, st_body_font_size=10, st_header_part=0,202020, st_footer_part=0,101010, st_body_line_spacing=1,200000, lines_per_page=45
	entries_count=70, pages_count=2
	*/

	priv->layout = gtk_print_context_create_pango_layout( context );
	/* context_width=559, pango_layout_width=572416 */
	build_body_layout( self, context );
}

static void
build_body_layout( ofaPrintReconcil *self, GtkPrintContext *context )
{
	ofaPrintReconcilPrivate *priv;
	gint digits;
	gchar *str;

	priv = self->priv;

	/* keep the leftest column to display a line number */
	str = g_strdup_printf( "%d", g_list_length( priv->entries ));
	digits = g_utf8_strlen( str, -1 );
	g_free( str );
	priv->body_count_rtab = st_page_margin + digits * 6*7/9;

	/* starting from the left : body_effect_ltab on the left margin */
	priv->body_effect_ltab = priv->body_count_rtab + st_column_spacing;
	priv->body_ledger_ltab = priv->body_effect_ltab+ st_effect_width + st_column_spacing;
	priv->body_ref_ltab = priv->body_ledger_ltab + st_journal_width + st_column_spacing;
	priv->body_label_ltab = priv->body_ref_ltab + st_ref_width + st_column_spacing;

	/* starting from the right */
	priv->body_solde_rtab = gtk_print_context_get_width( context ) - st_page_margin;
	priv->body_credit_rtab = priv->body_solde_rtab - st_amount_width - st_column_spacing;
	priv->body_debit_rtab = priv->body_credit_rtab - st_amount_width - st_column_spacing;

	/* max size in Pango units */
	priv->body_ref_max_size = st_ref_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_debit_rtab - st_amount_width - st_column_spacing - priv->body_label_ltab )*PANGO_SCALE;
}

static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_draw_page";
	ofaPrintReconcilPrivate *priv;
	gboolean is_first, is_last;
	gdouble max_y;
	GList *ent;
	gint count;

	g_debug( "%s: operation=%p, context=%p, page_num=%d, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, page_num, ( void * ) self );

	priv = self->priv;

	is_first = ( page_num == 0 );
	is_last = ( page_num == priv->pages_count-1 );

	draw_header( self, operation, context, page_num, is_last );

	if( is_first ){
		draw_reconciliation_start( self, context );
	} else {
		/* keep a small line spacing after column headers */
		priv->last_y += st_body_line_spacing/2;
	}

	max_y = priv->page_height - ofa_print_footer_get_height( 1, FALSE );
	max_y -= st_body_font_size+st_body_line_spacing;

	for( count=0, ent=g_list_nth( priv->entries, priv->last_entry+1 ) ;
								ent && priv->last_y<max_y ; ent=ent->next, count++ ){

		draw_line( self, operation, context, page_num, count, OFO_ENTRY( ent->data ));
	}

	priv->last_entry += count;

	/* last page: display the reconciliated solde */
	if( is_last ){
		draw_reconciliation_end( self, context );
	}

	ofa_print_footer_render( context, priv->layout, page_num, priv->pages_count );
}

static void
draw_header( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gboolean is_last )
{
	ofaPrintReconcilPrivate *priv;
	cairo_t *cr;
	gchar *str;
	gdouble y, width;

	priv = self->priv;

	y = 0;

	/* dossier header */
	ofa_print_header_dossier_render(
			context, priv->layout, page_num, y,
			ofa_main_window_get_dossier( priv->main_window ));
	y += ofa_print_header_dossier_get_height( page_num );

	/* print title in line 3 */
	ofa_print_header_title_render(
			context, priv->layout, page_num, y,
			_( "Account Reconciliation Summary" ));
	y+= ofa_print_header_title_get_height( page_num );

	/* account number and label in line 4 */
	str = g_strdup_printf(
				_( "Account %s - %s" ),
				ofo_account_get_number( priv->account ),
				ofo_account_get_label( priv->account ));
	ofa_print_header_subtitle_render(
			context, priv->layout, page_num, y, str );
	g_free( str );
	y+= ofa_print_header_subtitle_get_height( page_num );

	/* column headers
	 * draw a rectangle for one line with spacings as:
	 * spacing(bfs/2) + line(bfs) + spacing(bfs/2) */
	ofa_print_header_title_set_color( context, priv->layout );
	cr = gtk_print_context_get_cairo_context( context );
	width = gtk_print_context_get_width( context );
	cairo_rectangle( cr, 0, y, width, 2*st_body_font_size );
	cairo_fill( cr );

	/* columns title are white on same dark cyan background */
	cairo_set_source_rgb( cr, COLOR_WHITE );

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size-1 );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	y += 0.5*st_body_font_size;

	ofa_print_set_text( context, priv->layout,
			priv->body_effect_ltab, y, _( "Effect date" ), PANGO_ALIGN_LEFT );

	ofa_print_set_text( context, priv->layout,
			priv->body_ledger_ltab, y, _( "Ledger" ), PANGO_ALIGN_LEFT );

	ofa_print_set_text( context, priv->layout,
			priv->body_ref_ltab, y, _( "Piece" ), PANGO_ALIGN_LEFT );

	ofa_print_set_text( context, priv->layout,
			priv->body_label_ltab, y, _( "Label" ), PANGO_ALIGN_LEFT );

	ofa_print_set_text( context, priv->layout,
			priv->body_debit_rtab, y, _( "Debit" ), PANGO_ALIGN_RIGHT );

	ofa_print_set_text( context, priv->layout,
			priv->body_credit_rtab, y, _( "Credit" ), PANGO_ALIGN_RIGHT );

	ofa_print_set_text( context, priv->layout,
			priv->body_solde_rtab, y, _( "Solde" ), PANGO_ALIGN_RIGHT );

	/* this set the 'y' height just after the column headers */
	y += 1.5*st_body_font_size;

	priv->last_y = y;

	/*g_debug( "draw_header: final y=%lf", y ); */
	/* w_header: final y=138,500000
(openbook:27358): OFA-DEBUG: ofa_print_reconcil_on_draw_page: operation=0x7a9430, context=0xb55e80, self=0x8afd00
(openbook:27358): OFA-DEBUG: draw_header: final y=116,00000 */
}

static void
draw_reconciliation_start( ofaPrintReconcil *self, GtkPrintContext *context )
{
	ofaPrintReconcilPrivate *priv;
	const GDate *date;
	gchar *str, *str_solde, *sdate;
	gdouble y;

	priv = self->priv;

	y = priv->last_y;
	y += st_body_line_spacing;

	ofa_print_header_title_set_color( context, priv->layout );
	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size+1 );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	date = ofo_account_get_global_deffect( priv->account );
	if( !my_date_is_valid( date )){
		date = ( const GDate * ) &priv->date;
	}
	sdate = my_date_to_str( date, MY_DATE_DMYY );
	priv->account_solde = ofo_account_get_global_solde( priv->account );
	str_solde = display_account_solde( self, priv->account_solde );
	str = g_strdup_printf( _( "Account solde on %s is %s" ), sdate, str_solde );
	g_free( sdate );
	g_free( str_solde );
	ofa_print_set_text( context, priv->layout, priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	y += st_body_font_size+1 + st_body_line_spacing;

	priv->last_y = y;
}

/*
 * num_line is counted from 0 in the page
 * line number is last_entry + count + 2
 *
 * (printable)width(A4)=559
 * date  journal  piece    label      debit   credit   solde
 * 10    6        max(10)  max(80)      15d      15d     15d
 */
static void
draw_line( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gint line_num, ofoEntry *entry )
{
	ofaPrintReconcilPrivate *priv;
	gchar *str;
	cairo_t *cr;
	gdouble y;
	const gchar *conststr;
	gdouble amount;

	priv = self->priv;

	y = priv->last_y;

	cr = gtk_print_context_get_cairo_context( context );

	/* have a rubber every other line */
	if( line_num % 2 ){
		ofa_print_rubber( context, priv->layout,
				y-0.5*st_body_line_spacing, st_body_font_size+st_body_line_spacing );
	}

	/* display the line number starting from 1 */
	cairo_set_source_rgb( cr, COLOR_GRAY );
	str = g_strdup_printf( "%s %u", st_font_family, 7 );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );
	str = g_strdup_printf( "%d", priv->last_entry+2+line_num );
	ofa_print_set_text( context, priv->layout, priv->body_count_rtab, y+1, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	/* reset color and font */
	cairo_set_source_rgb( cr, COLOR_BLACK );
	str = g_strdup_printf( "%s %u", st_font_family, st_body_font_size );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	/* 0 is not really the edge of the sheet, but includes the printer margin */
	/* y is in context units
	 * add 20% to get some visual spaces between lines */

	str = my_date_to_str( ofo_entry_get_deffect( entry ), MY_DATE_DMYY );
	ofa_print_set_text( context, priv->layout,
			priv->body_effect_ltab, y, str, PANGO_ALIGN_LEFT );
	g_free( str );

	ofa_print_set_text( context, priv->layout,
			priv->body_ledger_ltab, y, ofo_entry_get_ledger( entry ), PANGO_ALIGN_LEFT );

	conststr = ofo_entry_get_ref( entry );
	if( conststr && g_utf8_strlen( conststr, -1 )){
		/* width is in Pango units = pixels*scale = device_units*scale */
		/* text-width-ellipsize: doesn't work */
		pango_layout_set_text( priv->layout, conststr, -1 );
		/*pango_layout_set_width( priv->layout, priv->body_ref_max_size );
		pango_layout_set_ellipsize( priv->layout, PANGO_ELLIPSIZE_END );*/
		/*g_debug( "ref: is_wrapped=%s, is_ellipsized=%s",
				pango_layout_is_wrapped( priv->layout )?"True":"False",
				pango_layout_is_ellipsized( priv->layout )?"True":"False" ); */
		my_utils_pango_layout_ellipsize( priv->layout, priv->body_ref_max_size );
		cairo_move_to( cr, priv->body_ref_ltab, y );
		pango_cairo_update_layout( cr, priv->layout );
		pango_cairo_show_layout( cr, priv->layout );
		pango_layout_set_width( priv->layout, -1 );
	}

	pango_layout_set_text( priv->layout, ofo_entry_get_label( entry ), -1 );
	/*pango_layout_set_width( priv->layout, priv->body_label_max_size );
	pango_layout_set_ellipsize( priv->layout, PANGO_ELLIPSIZE_END );*/
	/*g_debug( "label: is_wrapped=%s, is_ellipsized=%s",
			pango_layout_is_wrapped( priv->layout )?"True":"False",
			pango_layout_is_ellipsized( priv->layout )?"True":"False" );*/
	my_utils_pango_layout_ellipsize( priv->layout, priv->body_label_max_size );
	cairo_move_to( cr, priv->body_label_ltab, y );
	pango_cairo_update_layout( cr, priv->layout );
	pango_cairo_show_layout( cr, priv->layout );
	pango_layout_set_width( priv->layout, -1 );

	amount = ofo_entry_get_debit( entry );
	if( amount ){
		str = my_double_to_str( amount );
		ofa_print_set_text( context, priv->layout, priv->body_debit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde -= amount;
	}

	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = my_double_to_str( amount );
		ofa_print_set_text( context, priv->layout, priv->body_credit_rtab, y, str, PANGO_ALIGN_RIGHT );
		g_free( str );
		priv->account_solde += amount;
	}

	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );
	str = my_double_to_str( priv->account_solde );
	ofa_print_set_text( context, priv->layout, priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	y += st_body_font_size + st_body_line_spacing;

	priv->last_y = y;
}

static void
draw_reconciliation_end( ofaPrintReconcil *self, GtkPrintContext *context )
{
	ofaPrintReconcilPrivate *priv;
	cairo_t *cr;
	const GDate *date;
	gchar *str, *sdate, *str_amount;
	gdouble y;

	priv = self->priv;

	y = priv->last_y;
	y += st_body_line_spacing;

	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size+1 );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	date = ofo_account_get_global_deffect( priv->account );
	if( !my_date_is_valid( date ) || my_date_compare( date, &priv->date ) < 0 ){
		date = ( const GDate * ) &priv->date;
	}
	sdate = my_date_to_str( date, MY_DATE_DMYY );
	str_amount = display_account_solde( self, priv->account_solde );
	str = g_strdup_printf( _( "Reconciliated account solde on %s is %s" ), sdate, str_amount );
	g_free( sdate );
	g_free( str_amount );
	ofa_print_set_text( context, priv->layout, priv->body_solde_rtab, y, str, PANGO_ALIGN_RIGHT );
	g_free( str );

	y += st_body_font_size+1 + st_body_line_spacing;

	cairo_set_source_rgb( cr, COLOR_BLACK );

	y += st_body_line_spacing;

	str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size );
	ofa_print_set_font( context, priv->layout, str );
	g_free( str );

	pango_layout_set_width( priv->layout, priv->page_width*PANGO_SCALE );
	pango_layout_set_wrap( priv->layout, PANGO_WRAP_WORD );
	ofa_print_set_text( context, priv->layout, st_page_margin, y,
			_( "This reconciliated solde "
				"should be the same, though inversed, "
				"that the one of the account extraction sent by your bank.\n"
				"If this is note the case, then you have most probably "
				"forgotten to reconciliate "
				"some of the above entries, or some other entries have been recorded "
				"by your bank, are present in your account extraction, but are not "
				"found in your ledgers." ), PANGO_ALIGN_LEFT );

	y += 3*st_body_font_size;

	priv->last_y = y;
}

static gchar *
display_account_solde( ofaPrintReconcil *self, gdouble amount )
{
	ofaPrintReconcilPrivate *priv;
	gint decimals;
	gchar *str, *str_amount;

	priv = self->priv;

	decimals = ofo_currency_get_digits( priv->currency );
	str_amount = my_double_to_str_ex( amount, decimals );
	str = g_strdup_printf( "%s %s", str_amount, ofo_currency_get_symbol( priv->currency ));
	g_free( str_amount );

	return( str );
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_end_print";

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );
}
