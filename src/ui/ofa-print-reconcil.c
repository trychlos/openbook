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
#include "api/ofo-account.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"

#include "ui/ofa-main-window.h"
#include "ui/ofa-account-select.h"
#include "ui/ofa-main-window.h"
#include "ui/ofa-print-reconcil.h"

/* private instance data
 */
struct _ofaPrintReconcilPrivate {
	gboolean       dispose_has_run;

	/* input data
	 */
	ofaMainWindow *main_window;
	ofoDossier    *dossier;

	/* internals
	 */
	ofoAccount    *account;
	GDate          date;
	GList         *entries;
	gdouble        account_solde;

	/* UI
	 */
	GtkEntry      *account_entry;
	GtkLabel      *account_label;
	GtkLabel      *date_label;

	/* other datas
	 */
	gdouble        context_width;
	gdouble        context_height;
	gdouble        page_width;
	gdouble        page_height;
	gint           lines_per_page_first;
	gint           lines_per_page_other;
	gint           pages_count;
	PangoLayout   *header_layout;
	gdouble        header_height_first;
	gdouble        header_height_other;
	PangoLayout   *body_layout;
	gdouble        body_effect_tab;
	gdouble        body_journal_tab;
	gdouble        body_ref_tab;
	gint           body_ref_max_size;		/* Pango units */
	gdouble        body_label_tab;
	gint           body_label_max_size;		/* Pango units */
	gdouble        body_debit_tab;
	gdouble        body_credit_tab;
	gdouble        body_solde_tab;
	PangoLayout   *footer_layout;
	gdouble        last_y;
};

static const gchar  *st_ui_xml      = PKGUIDIR "/ofa-print-reconcil.ui";

/* these are parms which describe the page layout
 */
/* makes use of the same font family for all fields */
static const gchar  *st_font_family = "Sans";
static const gint    st_header_dossier_name_font_size  = 24;
#define st_header_dossier_name_spacing                 st_header_dossier_name_font_size*.25
static const gint    st_header_dossier_label_font_size = 14;
/* we put after the dossier label the same spacing that after the print title
 * or also: put before and after the title the same spacing */
/*#define st_header_dossier_label_spacing   = st_header_dossier_label_font_size*.5;*/
static const gint    st_header_print_title_font_size   = 18;
#define st_header_print_title_spacing                  st_header_print_title_font_size*.5
static const gint    st_body_font_size                 = 9;
/* the space between body lines */
#define st_body_line_spacing                           st_body_font_size*.5
/* as we use a white-on-cyan columns header, we keep a 2px left and right margins */
static const gint    st_page_left_margin               = 2;
static const gint    st_page_right_margin              = 2;

static const gdouble st_header_part = 80/297.0;		/* env. 20% */
static const gdouble st_footer_part = 30/297.0;		/* env. 10% */

/* the columns of the body */
#define st_effect_width                                56/9*st_body_font_size
#define st_journal_width                               37/9*st_body_font_size
#define st_ref_width                                   64/9*st_body_font_size
#define st_amount_width                                66/9*st_body_font_size
#define st_column_spacing                              9/9*st_body_font_size
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


G_DEFINE_TYPE( ofaPrintReconcil, ofa_print_reconcil, G_TYPE_OBJECT )

static GObject     *on_create_custom_widget( GtkPrintOperation *operation, ofaPrintReconcil *self );
static void         on_custom_widget_apply( GtkPrintOperation *operation, GtkWidget *widget, ofaPrintReconcil *self );
static void         on_account_changed( GtkEntry *entry, ofaPrintReconcil *self );
static void         on_account_select( GtkButton *button, ofaPrintReconcil *self );
static void         on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self );
static void         on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintReconcil *self );
static void         on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self );
static PangoLayout *build_header_layout( ofaPrintReconcil *self, GtkPrintContext *context, PangoLayout *layout );
static PangoLayout *build_body_layout( ofaPrintReconcil *self, GtkPrintContext *context, PangoLayout *layout );
static PangoLayout *build_footer_layout( ofaPrintReconcil *self, GtkPrintContext *context, PangoLayout *layout );
static void         draw_header( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );
static void         draw_line( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, gint line_num, ofoEntry *entry );
static void         draw_reconciliated( ofaPrintReconcil *self, GtkPrintContext *context );
static void         draw_footer( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num );

static void
print_reconcil_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_print_reconcil_finalize";
	ofaPrintReconcilPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PRINT_RECONCIL( instance ));

	priv = OFA_PRINT_RECONCIL( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_print_reconcil_parent_class )->finalize( instance );
}

static void
print_reconcil_dispose( GObject *instance )
{
	ofaPrintReconcilPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PRINT_RECONCIL( instance ));

	priv = OFA_PRINT_RECONCIL( instance )->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		if( priv->entries ){
			ofo_entry_free_dataset( priv->entries );
			priv->entries = NULL;
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

	self->private = g_new0( ofaPrintReconcilPrivate, 1 );

	self->private->dispose_has_run = FALSE;

	self->private->entries = NULL;
}

static void
ofa_print_reconcil_class_init( ofaPrintReconcilClass *klass )
{
	static const gchar *thisfn = "ofa_print_reconcil_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = print_reconcil_dispose;
	G_OBJECT_CLASS( klass )->finalize = print_reconcil_finalize;
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
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	gboolean printed;
	GtkWidget *dialog;
	gchar *str;
	GError *error;
	GtkPageSetup *psetup;
	GtkPaperSize *psize;

	g_return_val_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ), FALSE );

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new(
					OFA_TYPE_PRINT_RECONCIL,
					NULL );

	self->private->main_window = main_window;
	self->private->dossier = ofa_main_window_get_dossier( main_window );

	print = gtk_print_operation_new ();

	g_signal_connect( print, "create-custom-widget", G_CALLBACK( on_create_custom_widget ), self );
	g_signal_connect( print, "custom-widget-apply", G_CALLBACK( on_custom_widget_apply ), self );
	g_signal_connect( print, "begin-print", G_CALLBACK( on_begin_print ), self );
	g_signal_connect( print, "draw-page", G_CALLBACK( on_draw_page ), self );
	g_signal_connect( print, "end-print", G_CALLBACK( on_end_print ), self );

	error = NULL;
	printed = FALSE;

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
	                GTK_WINDOW( main_window ),
	                &error );

	if( res == GTK_PRINT_OPERATION_RESULT_ERROR ){
		str = g_strdup_printf( _( "Error while printing document:\n%s" ), error->message );
		dialog = gtk_message_dialog_new(
							GTK_WINDOW( main_window ),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"%s", str );
		g_signal_connect( dialog, "response", G_CALLBACK( gtk_widget_destroy ), NULL );
		gtk_widget_show( dialog );
		g_error_free( error );

	} else if( res == GTK_PRINT_OPERATION_RESULT_APPLY ){
	      /*if (settings != NULL)
	        g_object_unref (settings);
	      settings = g_object_ref (gtk_print_operation_get_print_settings (print));
	    }*/
		printed = TRUE;
	}

	g_object_unref( self );

	return( printed );
}

#if 0
static void
setup_page( ofaPrintReconcil *self, GtkPrintOperation *operation )
{
	GtkPageSetup *setup;

	setup = gtk_page_setup_new();
	gtk_page_setup_set_orientation( setup, GTK_PAGE_ORIENTATION_PORTRAIT );
	gtk_page_setup_set_paper_size( setup, GTK_PAPER_NAME_A4 );
	gtk_print_operation_set_default_page_setup( operation, setup );
	g_object_unref( setup );

	gtk_print_operation_set_unit( operation, GTK_UNIT_MM );
}
#endif

static GObject*
on_create_custom_widget( GtkPrintOperation *operation, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_create_custom_widget";
	GtkWidget *box, *frame;
	GtkWidget *entry, *button, *label;
	myDateParse parms;

	g_debug( "%s: operation=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) self );

	box = my_utils_builder_load_from_path( st_ui_xml, "box-reconcil" );
	frame = my_utils_container_get_child_by_name( GTK_CONTAINER( box ), "frame-reconcil" );
	g_object_ref( frame );
	gtk_container_remove( GTK_CONTAINER( box ), frame );
	g_object_unref( box );

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-entry" );
	g_return_val_if_fail( entry && GTK_IS_ENTRY( entry ), NULL );
	g_signal_connect( G_OBJECT( entry ), "changed", G_CALLBACK( on_account_changed ), self );
	self->private->account_entry = GTK_ENTRY( entry );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-select" );
	g_return_val_if_fail( button && GTK_IS_BUTTON( button ), NULL );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_account_select ), self );

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "account-label" );
	g_return_val_if_fail( label && GTK_IS_LABEL( label ), NULL );
	self->private->account_label = GTK_LABEL( label );

	memset( &parms, '\0', sizeof( parms ));
	parms.entry = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "date-entry" );
	parms.entry_format = MY_DATE_DDMM;
	parms.label = my_utils_container_get_child_by_name( GTK_CONTAINER( frame ), "date-label" );
	parms.label_format = MY_DATE_DMMM;
	parms.date = &self->private->date;
	my_date_parse_from_entry( &parms );

	return( G_OBJECT( frame ));
}

static void
on_account_changed( GtkEntry *entry, ofaPrintReconcil *self )
{
	ofaPrintReconcilPrivate *priv;
	const gchar *str;

	priv = self->private;
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

	priv = self->private;
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

	priv = self->private;

	if( g_date_valid( &priv->date ) &&
			priv->account && OFO_IS_ACCOUNT( priv->account )){

		priv->entries = ofo_entry_get_dataset_for_print_reconcil(
								priv->dossier,
								ofo_account_get_number( priv->account ),
								&priv->date );
	}
}

static void
on_begin_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_begin_print";
	ofaPrintReconcilPrivate *priv;
	gint entries_count, other_count, last_count;
	PangoLayout *layout;
	gdouble footer_height, reconcil_height, last_height, header_height;

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->private;

	priv->context_width = gtk_print_context_get_width( context );
	priv->page_width = priv->context_width - st_page_left_margin - st_page_right_margin;
	priv->context_height = gtk_print_context_get_height( context );
	priv->page_height = priv->context_height;

	g_debug( "%s; context_width=%lf, context_height=%lf, page_width=%lf, page_height=%lf",
			thisfn,
			priv->context_width, priv->context_height,
			priv->page_width, priv->page_height );

	priv->header_height_other =
			st_header_dossier_name_font_size + st_header_dossier_name_spacing
			+ st_header_dossier_label_font_size + st_header_print_title_spacing
			+ st_header_print_title_font_size + st_header_print_title_spacing
			+ st_body_font_size + 2*st_body_line_spacing
			+ 0.5*st_body_font_size
			+ st_body_font_size + 2*st_body_line_spacing;

	priv->header_height_first =
			priv->header_height_other
			+ st_body_line_spacing
			+ st_body_font_size + st_body_line_spacing;

	footer_height =
			st_body_font_size + 2*st_body_line_spacing;

	reconcil_height =
			st_body_font_size + 2*st_body_line_spacing
			+ 3*( st_body_font_size ) + st_body_line_spacing;

	entries_count = g_list_length( self->private->entries );

	priv->lines_per_page_first =
			( priv->page_height - priv->header_height_first - footer_height )
			/ ( st_body_font_size+st_body_line_spacing );

	priv->lines_per_page_other =
			( priv->page_height - priv->header_height_other - footer_height )
			/ ( st_body_font_size+st_body_line_spacing );

	if( entries_count <= priv->lines_per_page_first ){
		priv->pages_count = 1;
		last_count = entries_count;
		header_height = priv->header_height_first;
	} else {
		other_count = entries_count - priv->lines_per_page_first;
		priv->pages_count = 1 + ( other_count / priv->lines_per_page_other );
		last_count = other_count % priv->lines_per_page_other;
		if( last_count ){
			priv->pages_count += 1;
		}
		header_height = priv->header_height_other;
	}

	if( last_count == 0 ){
		if( entries_count > 0 ){
			priv->pages_count += 1;
		}
	} else {
		last_height = last_count * ( st_body_font_size+st_body_line_spacing );
		if( last_height + reconcil_height > priv->page_height - header_height - footer_height ){
			priv->pages_count += 1;
		}
	}

	gtk_print_operation_set_n_pages( operation, priv->pages_count );

	g_debug( "%s: entries_count=%u, st_header_part=%lf, st_footer_part=%lf, st_body_font_size=%u, st_body_line_spacing=%lf",
			thisfn,
			entries_count, st_header_part, st_footer_part,
			st_body_font_size, st_body_line_spacing );

	/*g_debug( "%s: context_height=%lf, st_body_font_size=%d, st_header_part=%lf, st_footer_part=%lf, st_body_line_spacing=%lf, lines_per_page=%d",
				thisfn, context_height, st_body_font_size, st_header_part, st_footer_part, st_body_line_spacing, priv->lines_per_page );
	g_debug( "%s: entries_count=%d, pages_count=%d", thisfn, entries_count, priv->pages_count );
	context_height=783,569764, st_body_font_size=10, st_header_part=0,202020, st_footer_part=0,101010, st_body_line_spacing=1,200000, lines_per_page=45
	entries_count=70, pages_count=2
	*/

	layout = gtk_print_context_create_pango_layout( context );

	/*g_debug( "%s: context_height=%lf", thisfn, gtk_print_context_get_height( context ));
	g_debug( "%s: lines_per_page=%d", thisfn, self->private->lines_per_page );
	context_height = 783,569764
	lines_per_page = 32 */
	/*g_debug( "%s: context_width=%u, pango_layout_width=%u", thisfn, width, pango_layout_get_width( priv->layout ));*/
	/* context_width=559, pango_layout_width=572416 */

	priv->header_layout = build_header_layout( self, context, layout );
	priv->body_layout = build_body_layout( self, context, layout );
	priv->footer_layout = build_footer_layout( self, context, layout );

	g_object_unref( layout );
}

static void
on_draw_page( GtkPrintOperation *operation, GtkPrintContext *context, gint page_num, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_draw_page";
	ofaPrintReconcilPrivate *priv;
	gint first, count, max;
	GList *ent;

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->private;

	draw_header( self, operation, context, page_num );

	if( page_num == 0 ){
		first = 0;
		max = priv->lines_per_page_first;
	} else {
		first = priv->lines_per_page_first + priv->lines_per_page_other*( page_num-1 );
		max = priv->lines_per_page_other;
	}

	for( count=0, ent=g_list_nth( priv->entries, first ) ;
								ent && count<max ; ent=ent->next, count++ ){

		draw_line( self, operation, context, page_num, count, OFO_ENTRY( ent->data ));
	}

	/* last page: display the reconciliated solde */
	if( page_num == priv->pages_count - 1 ){
		draw_reconciliated( self, context );
	}

	draw_footer( self, operation, context, page_num );
}

static void
on_end_print( GtkPrintOperation *operation, GtkPrintContext *context, ofaPrintReconcil *self )
{
	static const gchar *thisfn = "ofa_print_reconcil_on_end_print";
	ofaPrintReconcilPrivate *priv;

	g_debug( "%s: operation=%p, context=%p, self=%p",
			thisfn, ( void * ) operation, ( void * ) context, ( void * ) self );

	priv = self->private;

	g_clear_object( &priv->header_layout );
	g_clear_object( &priv->body_layout );
	g_clear_object( &priv->footer_layout );
}

static PangoLayout *
build_header_layout( ofaPrintReconcil *self, GtkPrintContext *context, PangoLayout *layout )
{
	PangoLayout *h_layout;

	h_layout = pango_layout_copy( layout );

	return( h_layout );
}

static PangoLayout *
build_body_layout( ofaPrintReconcil *self, GtkPrintContext *context, PangoLayout *layout )
{
	ofaPrintReconcilPrivate *priv;
	PangoLayout *b_layout;
	PangoFontDescription *desc;
	gchar *str;

	b_layout = pango_layout_copy( layout );

	priv = self->private;

	/* we are uning a unique font to draw the lines */
	str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size );
	desc = pango_font_description_from_string( str );
	g_free( str );
	pango_layout_set_font_description( b_layout, desc );
	pango_font_description_free( desc );

	/* starting from the left : body_effect_tab on the left margin */
	priv->body_effect_tab = st_page_left_margin;
	priv->body_journal_tab = priv->body_effect_tab+ st_effect_width + st_column_spacing;
	priv->body_ref_tab = priv->body_journal_tab + st_journal_width + st_column_spacing;
	priv->body_label_tab = priv->body_ref_tab + st_ref_width + st_column_spacing;

	/* starting from the right */
	priv->body_solde_tab = priv->page_width + st_page_left_margin;
	priv->body_credit_tab = priv->body_solde_tab - st_amount_width - st_column_spacing;
	priv->body_debit_tab = priv->body_credit_tab - st_amount_width - st_column_spacing;

	/* max size in Pango units */
	priv->body_ref_max_size = st_ref_width*PANGO_SCALE;
	priv->body_label_max_size = ( priv->body_debit_tab - st_amount_width - st_column_spacing - priv->body_label_tab )*PANGO_SCALE;

	return( b_layout );
}

/*
 * the tabulations should be set for this layout
 * but - as of 2014- 6-20, only left tabs are supported :(
 * as we are using only one font in the footer, set also it here
 */
static PangoLayout *
build_footer_layout( ofaPrintReconcil *self, GtkPrintContext *context, PangoLayout *layout )
{
	PangoLayout *f_layout;
	PangoFontDescription *desc;
	/*PangoTabArray *array;*/
	gchar *str;

	f_layout = pango_layout_copy( layout );

	str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size-2 );
	desc = pango_font_description_from_string( str );
	g_free( str );
	pango_font_description_set_style( desc, PANGO_STYLE_ITALIC );
	pango_layout_set_font_description( f_layout, desc );
	pango_font_description_free( desc );

	return( f_layout );
}

static void
draw_header( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	ofaPrintReconcilPrivate *priv;
	PangoFontDescription *desc;
	cairo_t *cr;
	PangoRectangle rc;
	gchar *str, *sdate;
	gdouble y;

	priv = self->private;

	cr = gtk_print_context_get_cairo_context( context );

	/* the dark red used for PWI Consultants */
	cairo_set_source_rgb( cr, COLOR_DARK_RED );

	/* dossier name on line 1 */
	y = 0;
	str = g_strdup_printf( "%s Bold Italic %d", st_font_family, st_header_dossier_name_font_size );
	desc = pango_font_description_from_string( str );
	g_free( str );
	pango_font_description_set_variant( desc, PANGO_VARIANT_SMALL_CAPS );
	pango_layout_set_font_description( priv->header_layout, desc );
	pango_font_description_free( desc );
	pango_layout_set_text( priv->header_layout, ofo_dossier_get_name( priv->dossier ), -1 );
	cairo_move_to( cr, st_page_left_margin, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );
	y += st_header_dossier_name_font_size + st_header_dossier_name_spacing;

	/* dossier label on line 2 */
	str = g_strdup_printf( "%s Bold %d", st_font_family, st_header_dossier_label_font_size );
	desc = pango_font_description_from_string( str );
	pango_layout_set_font_description( priv->header_layout, desc );
	pango_font_description_free( desc );
	pango_layout_set_text( priv->header_layout, ofo_dossier_get_label( priv->dossier ), -1 );
	cairo_move_to( cr, st_page_left_margin, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );
	y += st_header_dossier_label_font_size + st_header_print_title_spacing;

	/* print title in line 3 */
	str = g_strdup_printf( "%s Bold %d", st_font_family, st_header_print_title_font_size );
	desc = pango_font_description_from_string( str );
	pango_layout_set_font_description( priv->header_layout, desc );
	pango_font_description_free( desc );
	/* a dark cyan used for summary title */
	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );
	pango_layout_set_text( priv->header_layout, _( "Account Reconciliation Summary"), -1 );
	pango_layout_get_pixel_extents( priv->header_layout, NULL, &rc );
	cairo_move_to( cr, ( priv->context_width-rc.width )/2, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );
	y += st_header_print_title_font_size + st_header_print_title_spacing;

	/* account number and label in line 4 */
	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size+1 );
	desc = pango_font_description_from_string( str );
	pango_layout_set_font_description( priv->header_layout, desc );
	pango_font_description_free( desc );
	/* a dark cyan used for summary title */
	/*cairo_set_source_rgb( cr, COLOR_BLACK );*/
	str = g_strdup_printf( "%s %s - %s", _( "Account"),
			ofo_account_get_number( priv->account ),
			ofo_account_get_label( priv->account ));
	pango_layout_set_text( priv->header_layout, str, -1 );
	pango_layout_get_pixel_extents( priv->header_layout, NULL, &rc );
	cairo_move_to( cr, ( priv->context_width-rc.width )/2, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );
	y += st_body_font_size + 2*st_body_line_spacing;

	/* columns headers in line 5 */
	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );
	cairo_rectangle( cr,
				0, y,
				priv->context_width, 2*st_body_font_size );
	cairo_fill( cr );
	y += 0.5*st_body_font_size;

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size-1 );
	desc = pango_font_description_from_string( str );
	pango_layout_set_font_description( priv->header_layout, desc );
	pango_font_description_free( desc );
	/* columns title are white on same dark cyan background */
	cairo_set_source_rgb( cr, COLOR_WHITE );
	pango_layout_set_text( priv->header_layout, _( "Effect date" ), -1 );
	cairo_move_to( cr, priv->body_effect_tab, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	pango_layout_set_text( priv->header_layout, _( "Journal" ), -1 );
	cairo_move_to( cr, priv->body_journal_tab, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	pango_layout_set_text( priv->header_layout, _( "Piece" ), -1 );
	cairo_move_to( cr, priv->body_ref_tab, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	pango_layout_set_text( priv->header_layout, _( "Label" ), -1 );
	cairo_move_to( cr, priv->body_label_tab, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	pango_layout_set_text( priv->header_layout, _( "Debit" ), -1 );
	pango_layout_get_pixel_extents( priv->header_layout, NULL, &rc );
	cairo_move_to( cr, priv->body_debit_tab-rc.width, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	pango_layout_set_text( priv->header_layout, _( "Credit" ), -1 );
	pango_layout_get_pixel_extents( priv->header_layout, NULL, &rc );
	cairo_move_to( cr, priv->body_credit_tab-rc.width, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	pango_layout_set_text( priv->header_layout, _( "Solde" ), -1 );
	pango_layout_get_pixel_extents( priv->header_layout, NULL, &rc );
	cairo_move_to( cr, priv->body_solde_tab-rc.width, y );
	pango_cairo_update_layout( cr, priv->header_layout );
	pango_cairo_show_layout( cr, priv->header_layout );

	y += st_body_font_size + st_body_line_spacing;

	if( page_num == 0 ){
		y += st_body_line_spacing;
		cairo_set_source_rgb( cr, COLOR_DARK_CYAN );
		str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size+1 );
		desc = pango_font_description_from_string( str );
		pango_layout_set_font_description( priv->header_layout, desc );
		pango_font_description_free( desc );

		sdate = my_date_dto_str( ofo_account_get_global_deffect( priv->account ), MY_DATE_DDMM );
		if( !sdate || !g_utf8_strlen( sdate, -1 )){
			g_free( sdate );
			sdate = my_date_dto_str( &priv->date, MY_DATE_DDMM );
		}
		priv->account_solde = ofo_account_get_global_solde( priv->account );
		str = g_strdup_printf(
						"Account solde on %s is %+'.2lf",
						sdate, priv->account_solde );
		g_free( sdate );
		pango_layout_set_text( priv->header_layout, str, -1 );
		g_free( str );
		pango_layout_get_pixel_extents( priv->header_layout, NULL, &rc );
		cairo_move_to( cr, priv->body_solde_tab-rc.width, y );
		pango_cairo_update_layout( cr, priv->header_layout );
		pango_cairo_show_layout( cr, priv->header_layout );

		y += st_body_font_size + 2*st_body_line_spacing;
	}

	priv->last_y = y;

	/*g_debug( "draw_header: final y=%lf", y ); */
	/* w_header: final y=138,500000
(openbook:27358): OFA-DEBUG: ofa_print_reconcil_on_draw_page: operation=0x7a9430, context=0xb55e80, self=0x8afd00
(openbook:27358): OFA-DEBUG: draw_header: final y=116,00000 */
}

/*
 * num_line is counted from 0 in the page
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
	PangoRectangle rc;
	const gchar *conststr;
	gdouble amount;

	priv = self->private;

	cr = gtk_print_context_get_cairo_context( context );

	if( page_num == 0 ){
		y = priv->header_height_first;
	} else {
		y = priv->header_height_other;
	}
	y += line_num * ( st_body_font_size + st_body_line_spacing );

	/* have a rubber every other line */
	if( line_num % 2 ){
		cairo_set_source_rgb( cr, COLOR_LIGHT_GRAY );
		cairo_rectangle( cr,
					0, y - 0.5*st_body_line_spacing,
					priv->context_width, st_body_font_size+st_body_line_spacing );
		cairo_fill( cr );
	}

	cairo_set_source_rgb( cr, COLOR_BLACK );

	/* 0 is not really the edge of the sheet, but includes the printer margin */
	/* y is in context units
	 * add 20% to get some visual spaces between lines */

	str = my_date_dto_str( ofo_entry_get_deffect( entry ), MY_DATE_DDMM );
	pango_layout_set_text( priv->body_layout, str, -1 );
	g_free( str );
	cairo_move_to( cr, priv->body_effect_tab, y );
	pango_cairo_update_layout( cr, priv->body_layout );
	pango_cairo_show_layout( cr, priv->body_layout );

	pango_layout_set_text( priv->body_layout, ofo_entry_get_journal( entry ), -1 );
	cairo_move_to( cr, priv->body_journal_tab, y );
	pango_cairo_update_layout( cr, priv->body_layout );
	pango_cairo_show_layout( cr, priv->body_layout );

	conststr = ofo_entry_get_ref( entry );
	if( conststr ){
		/* width is in Pango units = pixels*scale = device_units*scale */
		/* text-width-ellipsize: doesn't work */
		pango_layout_set_text( priv->body_layout, conststr, -1 );
		/*pango_layout_set_width( priv->body_layout, priv->body_ref_max_size );
		pango_layout_set_ellipsize( priv->body_layout, PANGO_ELLIPSIZE_END );*/
		/*g_debug( "ref: is_wrapped=%s, is_ellipsized=%s",
				pango_layout_is_wrapped( priv->body_layout )?"True":"False",
				pango_layout_is_ellipsized( priv->body_layout )?"True":"False" ); */
		my_utils_pango_layout_ellipsize( priv->body_layout, priv->body_ref_max_size );
		cairo_move_to( cr, priv->body_ref_tab, y );
		pango_cairo_update_layout( cr, priv->body_layout );
		pango_cairo_show_layout( cr, priv->body_layout );
		pango_layout_set_width( priv->body_layout, -1 );
	}

	pango_layout_set_text( priv->body_layout, ofo_entry_get_label( entry ), -1 );
	/*pango_layout_set_width( priv->body_layout, priv->body_label_max_size );
	pango_layout_set_ellipsize( priv->body_layout, PANGO_ELLIPSIZE_END );*/
	/*g_debug( "label: is_wrapped=%s, is_ellipsized=%s",
			pango_layout_is_wrapped( priv->body_layout )?"True":"False",
			pango_layout_is_ellipsized( priv->body_layout )?"True":"False" );*/
	my_utils_pango_layout_ellipsize( priv->body_layout, priv->body_label_max_size );
	cairo_move_to( cr, priv->body_label_tab, y );
	pango_cairo_update_layout( cr, priv->body_layout );
	pango_cairo_show_layout( cr, priv->body_layout );
	pango_layout_set_width( priv->body_layout, -1 );

	amount = ofo_entry_get_debit( entry );
	if( amount ){
		str = g_strdup_printf( "%'.2lf", amount );
		pango_layout_set_text( priv->body_layout, str, -1 );
		g_free( str );
		pango_layout_get_pixel_extents( priv->body_layout, NULL, &rc );
		cairo_move_to( cr, priv->body_debit_tab-rc.width, y );
		pango_cairo_update_layout( cr, priv->body_layout );
		pango_cairo_show_layout( cr, priv->body_layout );
		priv->account_solde -= amount;
	}

	amount = ofo_entry_get_credit( entry );
	if( amount ){
		str = g_strdup_printf( "%'.2lf", amount );
		pango_layout_set_text( priv->body_layout, str, -1 );
		g_free( str );
		pango_layout_get_pixel_extents( priv->body_layout, NULL, &rc );
		cairo_move_to( cr, priv->body_credit_tab-rc.width, y );
		pango_cairo_update_layout( cr, priv->body_layout );
		pango_cairo_show_layout( cr, priv->body_layout );
		priv->account_solde += amount;
	}

	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );
	str = g_strdup_printf( "%'.2lf", priv->account_solde );
	pango_layout_set_text( priv->body_layout, str, -1 );
	g_free( str );
	pango_layout_get_pixel_extents( priv->body_layout, NULL, &rc );
	cairo_move_to( cr, priv->body_solde_tab-rc.width, y );
	pango_cairo_update_layout( cr, priv->body_layout );
	pango_cairo_show_layout( cr, priv->body_layout );

	priv->last_y = y;
}

static void
draw_reconciliated( ofaPrintReconcil *self, GtkPrintContext *context )
{
	ofaPrintReconcilPrivate *priv;
	PangoFontDescription *desc;
	cairo_t *cr;
	gchar *str, *sdate;
	PangoRectangle rc;
	gdouble y;

	priv = self->private;

	cr = gtk_print_context_get_cairo_context( context );

	y = priv->last_y + st_body_font_size + 2*st_body_line_spacing;
	cairo_set_source_rgb( cr, COLOR_DARK_CYAN );

	str = g_strdup_printf( "%s Bold %d", st_font_family, st_body_font_size+1 );
	desc = pango_font_description_from_string( str );
	pango_layout_set_font_description( priv->body_layout, desc );
	pango_font_description_free( desc );

	sdate = my_date_dto_str( ofo_account_get_global_deffect( priv->account ), MY_DATE_DDMM );
	if( !sdate || !g_utf8_strlen( sdate, -1 )){
		g_free( sdate );
		sdate = my_date_dto_str( &priv->date, MY_DATE_DDMM );
	}
	str = g_strdup_printf(
					"Reconciliated account solde on %s is %+'.2lf",
					sdate, priv->account_solde );
	g_free( sdate );
	pango_layout_set_text( priv->body_layout, str, -1 );
	g_free( str );
	pango_layout_get_pixel_extents( priv->body_layout, NULL, &rc );
	cairo_move_to( cr, priv->body_solde_tab-rc.width, y );
	pango_cairo_update_layout( cr, priv->body_layout );
	pango_cairo_show_layout( cr, priv->body_layout );

	y += st_body_font_size + 2*st_body_line_spacing;
	cairo_set_source_rgb( cr, COLOR_BLACK );

	str = g_strdup_printf( "%s %d", st_font_family, st_body_font_size );
	desc = pango_font_description_from_string( str );
	pango_layout_set_font_description( priv->body_layout, desc );
	pango_font_description_free( desc );

	pango_layout_set_text( priv->body_layout, _(
			"This reconciliated solde "
			"should be the same, though inversed, "
			"that the one of the account extraction sent par your bank.\n"
			"If this is note the case, then you have forgotten to reconciliate "
			"some of the above entries, or some other entries have been recorded "
			"by your bank, are present in your account extraction, but are not "
			"found in your journals." ), -1 );
	pango_layout_set_width( priv->body_layout, priv->page_width*PANGO_SCALE );
	pango_layout_set_wrap( priv->body_layout, PANGO_WRAP_WORD );
	cairo_move_to( cr, st_page_left_margin, y );
	pango_cairo_update_layout( cr, priv->body_layout );
	pango_cairo_show_layout( cr, priv->body_layout );
}

/*
 * page_num is counted from zero
 */
static void
draw_footer( ofaPrintReconcil *self, GtkPrintOperation *operation, GtkPrintContext *context, gint page_num )
{
	ofaPrintReconcilPrivate *priv;
	cairo_t *cr;
	gchar *str, *stamp_str;
	GTimeVal stamp;
	PangoRectangle rc;
	gdouble y;

	priv = self->private;

	cr = gtk_print_context_get_cairo_context( context );
	cairo_set_source_rgb( cr, 0.5, 0.5, 0.5 );

	my_utils_stamp_get_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_DMYYHM );
	str = g_strdup_printf( "Printed on %s - Page %d/%d", stamp_str, 1+page_num, priv->pages_count );
	g_free( stamp_str );
	pango_layout_set_text( priv->footer_layout, str, -1 );
	g_free( str );
	pango_layout_get_pixel_extents( priv->footer_layout, NULL, &rc );
	y = priv->context_height;
	cairo_move_to( cr, priv->page_width-rc.width, y );
	pango_cairo_update_layout( cr, priv->footer_layout );
	pango_cairo_show_layout( cr, priv->footer_layout );

	y -= st_body_font_size - st_body_line_spacing;
	cairo_set_line_width( cr, 0.5 );
	cairo_move_to( cr, 0, y );
	cairo_line_to( cr, priv->context_width, y );
	cairo_stroke( cr );
}
