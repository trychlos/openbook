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
#include "ui/ofa-main-page.h"
#include "ui/ofo-base.h"

/* private instance data
 */
struct _ofaMainPagePrivate {
	gboolean       dispose_has_run;

	/* properties set at instanciation time
	 */
	ofaMainWindow *main_window;
	ofoDossier    *dossier;
	GtkGrid       *grid;
	gint           theme;

	/* UI
	 */
	GtkButton     *btn_new;
	GtkButton     *btn_update;
	GtkButton     *btn_delete;
};

/* class properties
 */
enum {
	PROP_WINDOW_ID = 1,
	PROP_DOSSIER_ID,
	PROP_GRID_ID,
	PROP_THEME_ID,
};

/* signals defined
 */
enum {
	JOURNAL_CHANGED = 0,
	LAST_SIGNAL
};

static gint st_signals[ LAST_SIGNAL ] = { 0 };

G_DEFINE_TYPE( ofaMainPage, ofa_main_page, G_TYPE_OBJECT )

static void       do_setup_page( ofaMainPage *page );
static void       v_setup_page( ofaMainPage *page );
static GtkWidget *do_setup_view( ofaMainPage *page );
static GtkWidget *do_setup_buttons( ofaMainPage *page );
static GtkWidget *v_setup_buttons( ofaMainPage *page );
static void       do_init_view( ofaMainPage *page );
static void       do_on_new_clicked( GtkButton *button, ofaMainPage *page );
static void       do_on_update_clicked( GtkButton *button, ofaMainPage *page );
static void       do_on_delete_clicked( GtkButton *button, ofaMainPage *page );
static void       on_journal_changed_class_handler( ofaMainPage *page, ofaMainPageUpdateType type, ofoBase *journal );
static void       on_grid_finalized( ofaMainPage *self, GObject *grid );

static void
main_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_page_finalize";
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	priv = OFA_MAIN_PAGE( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_page_parent_class )->finalize( instance );
}

static void
main_page_dispose( GObject *instance )
{
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	priv = ( OFA_MAIN_PAGE( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_main_page_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
main_page_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	priv = OFA_MAIN_PAGE( instance )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_WINDOW_ID:
				g_value_set_pointer( value, priv->main_window );
				break;

			case PROP_DOSSIER_ID:
				g_value_set_pointer( value, priv->dossier );
				break;

			case PROP_GRID_ID:
				g_value_set_pointer( value, priv->grid );
				break;

			case PROP_THEME_ID:
				g_value_set_int( value, priv->theme );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * the user asks to set a property and provides it into a GValue
 * read the content of the provided GValue and set our instance datas
 */
static void
main_page_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	priv = OFA_MAIN_PAGE( instance )->private;

	if( !priv->dispose_has_run ){

		switch( property_id ){
			case PROP_WINDOW_ID:
				priv->main_window = g_value_get_pointer( value );
				break;

			case PROP_DOSSIER_ID:
				priv->dossier = g_value_get_pointer( value );
				break;

			case PROP_GRID_ID:
				priv->grid = g_value_get_pointer( value );
				break;

			case PROP_THEME_ID:
				priv->theme = g_value_get_int( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
main_page_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_page_constructed";
	ofaMainPage *self;
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	self = OFA_MAIN_PAGE( instance );
	priv = self->private;

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( ofa_main_page_parent_class )->constructed ){
		G_OBJECT_CLASS( ofa_main_page_parent_class )->constructed( instance );
	}

#if 0
	g_debug( "%s: main_window=%p, dossier=%p, theme=%d, grid=%p",
			thisfn,
			( void * ) priv->main_window,
			( void * ) priv->dossier,
			priv->theme,
			( void * ) priv->grid );
#else
	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));
#endif

	/* attach a weak reference to the grid widget to unref this object
	 * and (more useful) the derived class which handles it */
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));
	g_object_weak_ref(
			G_OBJECT( priv->grid ),
			( GWeakNotify ) on_grid_finalized,
			OFA_MAIN_PAGE( instance ));

	/* let the child class setup its page */
	do_setup_page( self );
	do_init_view( self );
}

static void
ofa_main_page_init( ofaMainPage *self )
{
	static const gchar *thisfn = "ofa_main_page_init";

	g_return_if_fail( OFA_IS_MAIN_PAGE( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaMainPagePrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->theme = -1;
}

static void
ofa_main_page_class_init( ofaMainPageClass *klass )
{
	static const gchar *thisfn = "ofa_main_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = main_page_constructed;
	G_OBJECT_CLASS( klass )->get_property = main_page_get_property;
	G_OBJECT_CLASS( klass )->set_property = main_page_set_property;
	G_OBJECT_CLASS( klass )->dispose = main_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = main_page_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WINDOW_ID,
			g_param_spec_pointer(
					MAIN_PAGE_PROP_WINDOW,
					"Main window",
					"The main window (ofaMainWindow *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_DOSSIER_ID,
			g_param_spec_pointer(
					MAIN_PAGE_PROP_DOSSIER,
					"Current dossier",
					"The currently opened dossier (ofoDossier *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_GRID_ID,
			g_param_spec_pointer(
					MAIN_PAGE_PROP_GRID,
					"Page grid",
					"The top child of the page (GtkGrid *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_THEME_ID,
			g_param_spec_int(
					MAIN_PAGE_PROP_THEME,
					"Theme",
					"The theme handled by this class (gint)",
					-1,
					INT_MAX,
					-1,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	/**
	 * main-page-signal-journal-updated:
	 *
	 * The signal is emitted when an #ofoJournal is created, updated or
	 * deleted.
	 *
	 * The class handler calls the class initialize_gtk_toplevel() virtual
	 * method.
	 *
	 * The default virtual method just does nothing.
	 */
	st_signals[ JOURNAL_CHANGED ] =
			g_signal_new_class_handler(
					OFA_SIGNAL_JOURNAL_UPDATED,
					G_TYPE_FROM_CLASS( klass ),
					G_SIGNAL_RUN_LAST,
					G_CALLBACK( on_journal_changed_class_handler ),
					NULL,
					NULL,
					g_cclosure_marshal_VOID__UINT_POINTER,
					G_TYPE_NONE,
					2,
					G_TYPE_UINT,
					G_TYPE_POINTER );

	klass->setup_page = v_setup_page;
	klass->setup_view = NULL;
	klass->setup_buttons = v_setup_buttons;
	klass->init_view = NULL;
	klass->on_new_clicked = NULL;
	klass->on_update_clicked = NULL;
	klass->on_delete_clicked = NULL;
}

static void
do_setup_page( ofaMainPage *page )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( OFA_MAIN_PAGE_GET_CLASS( page )->setup_page ){
		OFA_MAIN_PAGE_GET_CLASS( page )->setup_page( page );

	} else {
		v_setup_page( page );
	}
}

static void
v_setup_page( ofaMainPage *page )
{
	GtkWidget *view;
	GtkWidget *buttons_box;

	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	view = do_setup_view( page );
	if( view ){
		gtk_grid_attach( page->private->grid, view, 0, 0, 1, 1 );
	}

	buttons_box = do_setup_buttons( page );
	if( buttons_box ){
		gtk_grid_attach( page->private->grid, buttons_box, 1, 0, 1, 1 );
	}

	gtk_widget_show_all( GTK_WIDGET( page->private->grid ));
}

static GtkWidget *
do_setup_view( ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_main_page_do_setup_view";
	GtkWidget *view;

	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	view = NULL;

	if( OFA_MAIN_PAGE_GET_CLASS( page )->setup_view ){
		view = OFA_MAIN_PAGE_GET_CLASS( page )->setup_view( page );

	} else {
		g_debug( "%s: page=%p", thisfn, ( void * ) page );
	}

	return( view );
}

static GtkWidget *
do_setup_buttons( ofaMainPage *page )
{
	GtkWidget *buttons_box;

	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	buttons_box = NULL;

	if( OFA_MAIN_PAGE_GET_CLASS( page )->setup_buttons ){
		buttons_box = OFA_MAIN_PAGE_GET_CLASS( page )->setup_buttons( page );

	} else {
		v_setup_buttons( page );
	}

	return( buttons_box );
}

static GtkWidget *
v_setup_buttons( ofaMainPage *page )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	buttons_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 30 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_New..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_new_clicked ), page );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	page->private->btn_new = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Update..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_update_clicked ), page );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	page->private->btn_update = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Delete..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_delete_clicked ), page );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	page->private->btn_delete = button;

	return( GTK_WIDGET( buttons_box ));
}

static void
do_init_view( ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_main_page_do_init_view";

	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( OFA_MAIN_PAGE_GET_CLASS( page )->init_view ){
		OFA_MAIN_PAGE_GET_CLASS( page )->init_view( page );

	} else {
		g_debug( "%s: page=%p", thisfn, ( void * ) page );
	}
}

static void
do_on_new_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_main_page_do_on_new_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( OFA_MAIN_PAGE_GET_CLASS( page )->on_new_clicked ){
		OFA_MAIN_PAGE_GET_CLASS( page )->on_new_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

static void
do_on_update_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_main_page_do_on_update_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( OFA_MAIN_PAGE_GET_CLASS( page )->on_update_clicked ){
		OFA_MAIN_PAGE_GET_CLASS( page )->on_update_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

static void
do_on_delete_clicked( GtkButton *button, ofaMainPage *page )
{
	static const gchar *thisfn = "ofa_main_page_do_on_delete_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( OFA_MAIN_PAGE_GET_CLASS( page )->on_delete_clicked ){
		OFA_MAIN_PAGE_GET_CLASS( page )->on_delete_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

/*
 * default class handler for OFA_SIGNAL_JOURNAL_UPDATED signal
 */
static void
on_journal_changed_class_handler( ofaMainPage *page, ofaMainPageUpdateType type, ofoBase *journal )
{
	static const gchar *thisfn = "ofa_main_page_on_journal_changed_class_handler";

	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));
	g_return_if_fail( journal && OFO_IS_BASE( journal ));

	if( !page->private->dispose_has_run ){

		g_debug( "%s: page=%p (%s), type=%d, journal=%p (%s)",
				thisfn,
				( void * ) page, G_OBJECT_TYPE_NAME( page ),
				type,
				( void * ) journal, G_OBJECT_TYPE_NAME( journal ));
	}
}

/**
 * ofa_main_page_get_main_window:
 */
ofaMainWindow *
ofa_main_page_get_main_window( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run ){

		return( page->private->main_window );
	}

	return( NULL );
}

/**
 * ofa_main_page_get_dossier:
 */
ofoDossier *
ofa_main_page_get_dossier( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run ){

		return( page->private->dossier );
	}

	return( NULL );
}

/**
 * ofa_main_page_get_theme:
 *
 * Returns -1 if theme is not set.
 * If theme is set, it is strictly greater than zero (start with 1).
 */
gint
ofa_main_page_get_theme( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run ){

		return( page->private->theme );
	}

	return( -1 );
}

/**
 * ofa_main_page_get_grid:
 */
GtkGrid *
ofa_main_page_get_grid( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run ){

		return( page->private->grid );
	}

	return( NULL );
}

static void
on_grid_finalized( ofaMainPage *self, GObject *grid )
{
	static const gchar *thisfn = "ofa_main_page_on_grid_finalized";

	g_debug( "%s: self=%p (%s), grid=%p",
			thisfn,
			( void * ) self, G_OBJECT_TYPE_NAME( self ),
			( void * ) grid );

	g_object_unref( self );
}

/**
 * ofa_main_page_get_treeview:
 *
 * Each page of the main notebook is built inside of a GtkGrid.
 * This GtkGrid is supposed to hold a GtkTreeView, more or less direcly,
 * maybe via another GtkNotebook (e.g. see #ofAccountsChart and
 * #ofaModelsSet).
 *
 * This function should not be called from a 'switch-page' notebook
 * signal handler, as the current page is not yet set at this time.
 */
GtkTreeView *
ofa_main_page_get_treeview( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );
	GtkNotebook *child_book;
	gint tab_num;
	GtkWidget *tab_widget;
	GtkTreeView *tview;

	tview = NULL;

	if( !page->private->dispose_has_run ){

		child_book =
				( GtkNotebook * ) my_utils_container_get_child_by_type(
													GTK_CONTAINER( page->private->grid ),
													GTK_TYPE_NOTEBOOK );
		if( child_book ){
			g_return_val_if_fail( GTK_IS_NOTEBOOK( child_book ), NULL );

			tab_num = gtk_notebook_get_current_page( GTK_NOTEBOOK( child_book ));
			if( tab_num < 0 ){
				return( NULL );
			}

			tab_widget = gtk_notebook_get_nth_page( GTK_NOTEBOOK( child_book ), tab_num );
			g_return_val_if_fail( tab_widget && GTK_IS_WIDGET( tab_widget ), NULL );

			tview = ( GtkTreeView * ) my_utils_container_get_child_by_type(
														GTK_CONTAINER( tab_widget ),
														GTK_TYPE_TREE_VIEW );
		}
		if( !tview ){
			tview = ( GtkTreeView * ) my_utils_container_get_child_by_type(
														GTK_CONTAINER( page->private->grid ),
														GTK_TYPE_TREE_VIEW );
		}
	}

	return( tview );
}

/**
 * ofa_main_page_get_new_btn:
 */
GtkWidget *
ofa_main_page_get_new_btn( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run &&
			page->private->btn_new ){

		return( GTK_WIDGET( page->private->btn_new ));
	}

	return( NULL );
}

/**
 * ofa_main_page_get_update_btn:
 */
GtkWidget *
ofa_main_page_get_update_btn( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run &&
			page->private->btn_update ){

		return( GTK_WIDGET( page->private->btn_update ));
	}

	return( NULL );
}

/**
 * ofa_main_page_get_delete_btn:
 */
GtkWidget *
ofa_main_page_get_delete_btn( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run &&
			page->private->btn_delete ){

		return( GTK_WIDGET( page->private->btn_delete ));
	}

	return( NULL );
}

/**
 * ofa_main_page_delete_confirmed:
 *
 * Returns: %TRUE if the deletion is confirmed by the user.
 */
gboolean
ofa_main_page_delete_confirmed( const ofaMainPage *page, const gchar *message )
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(
			GTK_WINDOW( page->private->main_window ),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
			"%s", message );

	gtk_dialog_add_buttons( GTK_DIALOG( dialog ),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_DELETE, GTK_RESPONSE_OK,
			NULL );

	response = gtk_dialog_run( GTK_DIALOG( dialog ));

	gtk_widget_destroy( dialog );

	return( response == GTK_RESPONSE_OK );
}
