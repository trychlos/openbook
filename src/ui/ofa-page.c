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
#include "api/ofo-base.h"

#include "ui/my-buttons-box.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaPagePrivate {

	/* properties set at instanciation time
	 */
	GtkGrid       *grid;
	gint           theme;
	gboolean       has_import;
	gboolean       has_export;

	/* UI
	 */
	myButtonsBox  *buttons_box;
	GtkButton     *btn_new;
	GtkButton     *btn_update;
	GtkButton     *btn_delete;
	GtkButton     *btn_import;
	GtkButton     *btn_export;
};

/* class properties
 */
enum {
	PROP_WINDOW_ID = 1,
	PROP_DOSSIER_ID,
	PROP_GRID_ID,
	PROP_THEME_ID,
	PROP_HAS_IMPORT_ID,
	PROP_HAS_EXPORT_ID
};

G_DEFINE_TYPE( ofaPage, ofa_page, G_TYPE_OBJECT )

static void       do_setup_page( ofaPage *page );
static void       v_setup_page_default( ofaPage *page );
static GtkWidget *do_setup_view( ofaPage *page );
static GtkWidget *do_setup_buttons( ofaPage *page );
static GtkWidget *v_setup_buttons_default( ofaPage *page );
static GtkWidget *create_buttons( gboolean has_import, gboolean has_export );
static void       do_init_view( ofaPage *page );
static void       do_on_new_clicked( GtkButton *button, ofaPage *page );
static void       do_on_update_clicked( GtkButton *button, ofaPage *page );
static void       do_on_delete_clicked( GtkButton *button, ofaPage *page );
static void       do_on_import_clicked( GtkButton *button, ofaPage *page );
static void       do_on_export_clicked( GtkButton *button, ofaPage *page );
static void       on_grid_finalized( ofaPage *self, GObject *grid );

static void
page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_page_finalize";
	ofaPageProtected *prot;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAGE( instance ));

	/* free data members here */
	prot = OFA_PAGE( instance )->prot;
	g_free( prot );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_page_parent_class )->finalize( instance );
}

static void
page_dispose( GObject *instance )
{
	ofaPageProtected *prot;

	g_return_if_fail( instance && OFA_IS_PAGE( instance ));

	prot = ( OFA_PAGE( instance ))->prot;

	if( !prot->dispose_has_run ){

		prot->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_page_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
page_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaPageProtected *prot;
	ofaPagePrivate *priv;

	g_return_if_fail( OFA_IS_PAGE( instance ));

	prot = OFA_PAGE( instance )->prot;
	priv = OFA_PAGE( instance )->priv;

	if( !prot->dispose_has_run ){

		switch( property_id ){
			case PROP_WINDOW_ID:
				g_value_set_pointer( value, prot->main_window );
				break;

			case PROP_DOSSIER_ID:
				g_value_set_pointer( value, prot->dossier );
				break;

			case PROP_GRID_ID:
				g_value_set_pointer( value, priv->grid );
				break;

			case PROP_THEME_ID:
				g_value_set_int( value, priv->theme );
				break;

			case PROP_HAS_IMPORT_ID:
				g_value_set_boolean( value, priv->has_import );
				break;

			case PROP_HAS_EXPORT_ID:
				g_value_set_boolean( value, priv->has_export );
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
page_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaPageProtected *prot;
	ofaPagePrivate *priv;

	g_return_if_fail( OFA_IS_PAGE( instance ));

	prot = OFA_PAGE( instance )->prot;
	priv = OFA_PAGE( instance )->priv;

	if( !prot->dispose_has_run ){

		switch( property_id ){
			case PROP_WINDOW_ID:
				prot->main_window = g_value_get_pointer( value );
				break;

			case PROP_DOSSIER_ID:
				prot->dossier = g_value_get_pointer( value );
				break;

			case PROP_GRID_ID:
				priv->grid = g_value_get_pointer( value );
				break;

			case PROP_THEME_ID:
				priv->theme = g_value_get_int( value );
				break;

			case PROP_HAS_IMPORT_ID:
				priv->has_import = g_value_get_boolean( value );
				break;

			case PROP_HAS_EXPORT_ID:
				priv->has_export = g_value_get_boolean( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

/*
 * called during instance initialization, after properties have been set
 */
static void
page_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_page_constructed";
	ofaPage *self;
	ofaPageProtected *prot;
	ofaPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAGE( instance ));

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( ofa_page_parent_class )->constructed ){
		G_OBJECT_CLASS( ofa_page_parent_class )->constructed( instance );
	}

	self = OFA_PAGE( instance );
	prot = self->prot;
	priv = self->priv;

	g_debug( "%s: instance=%p (%s), main_window=%p, dossier=%p, theme=%d, grid=%p",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) prot->main_window,
			( void * ) prot->dossier,
			priv->theme,
			( void * ) priv->grid );

	/* attach a weak reference to the grid widget to unref this object
	 * and (more useful) the derived class which handles it */
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));
	g_object_weak_ref(
			G_OBJECT( priv->grid ),
			( GWeakNotify ) on_grid_finalized,
			OFA_PAGE( instance ));

	/* let the child class setup its page */
	do_setup_page( self );
	do_init_view( self );
}

static void
ofa_page_init( ofaPage *self )
{
	static const gchar *thisfn = "ofa_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PAGE( self ));

	self->prot = g_new0( ofaPageProtected, 1 );
	self->prot->dispose_has_run = FALSE;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_PAGE, ofaPagePrivate );
	self->priv->theme = -1;
}

static void
ofa_page_class_init( ofaPageClass *klass )
{
	static const gchar *thisfn = "ofa_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->constructed = page_constructed;
	G_OBJECT_CLASS( klass )->get_property = page_get_property;
	G_OBJECT_CLASS( klass )->set_property = page_set_property;
	G_OBJECT_CLASS( klass )->dispose = page_dispose;
	G_OBJECT_CLASS( klass )->finalize = page_finalize;

	g_type_class_add_private( klass, sizeof( ofaPagePrivate ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_WINDOW_ID,
			g_param_spec_pointer(
					PAGE_PROP_WINDOW,
					"Main window",
					"The main window (ofaMainWindow *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_DOSSIER_ID,
			g_param_spec_pointer(
					PAGE_PROP_DOSSIER,
					"Current dossier",
					"The currently opened dossier (ofoDossier *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_GRID_ID,
			g_param_spec_pointer(
					PAGE_PROP_GRID,
					"Page grid",
					"The top child of the page (GtkGrid *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_THEME_ID,
			g_param_spec_int(
					PAGE_PROP_THEME,
					"Theme",
					"The theme handled by this class (gint)",
					-1,
					INT_MAX,
					-1,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_HAS_IMPORT_ID,
			g_param_spec_boolean(
					PAGE_PROP_HAS_IMPORT,
					"Has import button",
					"Whether the page will display the 'Import...' button",
					FALSE,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_HAS_EXPORT_ID,
			g_param_spec_boolean(
					PAGE_PROP_HAS_EXPORT,
					"Has export button",
					"Whether the page will display the 'Export...' button",
					FALSE,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	klass->setup_page = v_setup_page_default;
	klass->setup_view = NULL;
	klass->setup_buttons = v_setup_buttons_default;
	klass->init_view = NULL;
	klass->on_new_clicked = NULL;
	klass->on_update_clicked = NULL;
	klass->on_delete_clicked = NULL;
	klass->on_import_clicked = NULL;
	klass->on_export_clicked = NULL;
	klass->pre_remove = NULL;
}

static void
do_setup_page( ofaPage *page )
{
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->setup_page ){
		OFA_PAGE_GET_CLASS( page )->setup_page( page );
	}
}

/*
 * this is only called if the derived class doesn't provide its own
 * version of the 'setup_page' virtual method
 */
static void
v_setup_page_default( ofaPage *page )
{
	GtkWidget *view;
	GtkWidget *buttons_box;

	g_return_if_fail( page && OFA_IS_PAGE( page ));

	view = do_setup_view( page );
	if( view ){
		gtk_grid_attach( page->priv->grid, view, 0, 0, 1, 1 );
	}

	buttons_box = do_setup_buttons( page );
	if( buttons_box ){
		gtk_grid_attach( page->priv->grid, buttons_box, 1, 0, 1, 1 );
	}

	gtk_widget_show_all( GTK_WIDGET( page->priv->grid ));
}

static GtkWidget *
do_setup_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_setup_view";
	GtkWidget *view;

	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	view = NULL;

	if( OFA_PAGE_GET_CLASS( page )->setup_view ){
		view = OFA_PAGE_GET_CLASS( page )->setup_view( page );

	} else {
		g_debug( "%s: page=%p", thisfn, ( void * ) page );
	}

	return( view );
}

static GtkWidget *
do_setup_buttons( ofaPage *page )
{
	GtkWidget *buttons_box;

	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	buttons_box = NULL;

	if( OFA_PAGE_GET_CLASS( page )->setup_buttons ){
		buttons_box = OFA_PAGE_GET_CLASS( page )->setup_buttons( page );
	}

	return( buttons_box );
}

static GtkWidget *
v_setup_buttons_default( ofaPage *page )
{
	ofaPagePrivate *priv;
	GtkBox *buttons_box;
	GtkWidget *button;

	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	priv = page->priv;
	buttons_box = GTK_BOX( create_buttons( priv->has_import, priv->has_export ));

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_NEW );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_new_clicked ), page );
	priv->btn_new = GTK_BUTTON( button );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_UPDATE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_update_clicked ), page );
	priv->btn_update = GTK_BUTTON( button );

	button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_DELETE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_delete_clicked ), page );
	priv->btn_delete = GTK_BUTTON( button );

	if( priv->has_import ){
		button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_IMPORT );
		g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_import_clicked ), page );
		priv->btn_import = GTK_BUTTON( button );
	}
	if( priv->has_export ){
		button = my_utils_container_get_child_by_name( GTK_CONTAINER( buttons_box ), PAGE_BUTTON_EXPORT );
		g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( do_on_export_clicked ), page );
		priv->btn_export = GTK_BUTTON( button );
	}

	return( GTK_WIDGET( buttons_box ));
}

static GtkWidget *
create_buttons( gboolean has_import, gboolean has_export )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkWidget *button;

	buttons_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 4 ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 15 );

	button = gtk_button_new_with_mnemonic( _( "_New..." ));
	gtk_box_pack_start( buttons_box, button, FALSE, FALSE, 0 );
	gtk_buildable_set_name( GTK_BUILDABLE( button ), PAGE_BUTTON_NEW );

	button = gtk_button_new_with_mnemonic( _( "_Update..." ));
	gtk_widget_set_sensitive( button, FALSE );
	gtk_box_pack_start( buttons_box, button, FALSE, FALSE, 0 );
	gtk_buildable_set_name( GTK_BUILDABLE( button ), PAGE_BUTTON_UPDATE );

	button = gtk_button_new_with_mnemonic( _( "_Delete..." ));
	gtk_widget_set_sensitive( button, FALSE );
	gtk_box_pack_start( buttons_box, button, FALSE, FALSE, 0 );
	gtk_buildable_set_name( GTK_BUILDABLE( button ), PAGE_BUTTON_DELETE );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 8 );

	if( has_import ){
		button = gtk_button_new_with_mnemonic( _( "_Import..." ));
		gtk_box_pack_start( buttons_box, button, FALSE, FALSE, 0 );
		gtk_buildable_set_name( GTK_BUILDABLE( button ), PAGE_BUTTON_IMPORT );
	}

	if( has_export ){
		button = gtk_button_new_with_mnemonic( _( "_Export..." ));
		gtk_box_pack_start( buttons_box, button, FALSE, FALSE, 0 );
		gtk_buildable_set_name( GTK_BUILDABLE( button ), PAGE_BUTTON_EXPORT );
	}

	return( GTK_WIDGET( buttons_box ));
}

/**
 * ofa_page_get_buttons_box_new:
 *
 * This is a convenience function which provides to others the same
 * buttons box than that of the standard main page (first used by
 * ofaAccountsBook)
 *
 * Returns a new box, with its attached buttons.
 */
GtkBox *
ofa_page_get_buttons_box_new( gboolean has_import, gboolean has_export )
{
	return( GTK_BOX( create_buttons( has_import, has_export )));
}

static void
do_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_init_view";

	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->init_view ){
		OFA_PAGE_GET_CLASS( page )->init_view( page );

	} else {
		g_debug( "%s: page=%p", thisfn, ( void * ) page );
	}
}

static void
do_on_new_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_on_new_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->on_new_clicked ){
		OFA_PAGE_GET_CLASS( page )->on_new_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

static void
do_on_update_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_on_update_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->on_update_clicked ){
		OFA_PAGE_GET_CLASS( page )->on_update_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

static void
do_on_delete_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_on_delete_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->on_delete_clicked ){
		OFA_PAGE_GET_CLASS( page )->on_delete_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

static void
do_on_import_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_on_import_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->on_import_clicked ){
		OFA_PAGE_GET_CLASS( page )->on_import_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

static void
do_on_export_clicked( GtkButton *button, ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_do_on_export_clicked";

	g_return_if_fail( button && GTK_IS_BUTTON( button ));
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->on_export_clicked ){
		OFA_PAGE_GET_CLASS( page )->on_export_clicked( button, page );

	} else {
		g_debug( "%s: button=%p, page=%p (%s)",
				thisfn, ( void * ) button, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
	}
}

/**
 * ofa_page_get_main_window:
 */
ofaMainWindow *
ofa_page_get_main_window( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( page->prot->main_window );
	}

	return( NULL );
}

/**
 * ofa_page_get_dossier:
 */
ofoDossier *
ofa_page_get_dossier( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( page->prot->dossier );
	}

	return( NULL );
}

/**
 * ofa_page_get_theme:
 *
 * Returns -1 if theme is not set.
 * If theme is set, it is strictly greater than zero (start with 1).
 */
gint
ofa_page_get_theme( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( page->priv->theme );
	}

	return( -1 );
}

/**
 * ofa_page_get_grid:
 */
GtkGrid *
ofa_page_get_grid( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( page->priv->grid );
	}

	return( NULL );
}

static void
on_grid_finalized( ofaPage *self, GObject *grid )
{
	static const gchar *thisfn = "ofa_page_on_grid_finalized";

	g_debug( "%s: self=%p (%s), grid=%p",
			thisfn,
			( void * ) self, G_OBJECT_TYPE_NAME( self ),
			( void * ) grid );

	g_object_unref( self );
}

/**
 * ofa_page_get_new_btn:
 */
GtkWidget *
ofa_page_get_new_btn( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( GTK_WIDGET( page->priv->btn_new ));
	}

	return( NULL );
}

/**
 * ofa_page_get_update_btn:
 */
GtkWidget *
ofa_page_get_update_btn( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( GTK_WIDGET( page->priv->btn_update ));
	}

	return( NULL );
}

/**
 * ofa_page_get_delete_btn:
 */
GtkWidget *
ofa_page_get_delete_btn( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( GTK_WIDGET( page->priv->btn_delete ));
	}

	return( NULL );
}

/**
 * ofa_page_get_import_btn:
 */
GtkWidget *
ofa_page_get_import_btn( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( GTK_WIDGET( page->priv->btn_import ));
	}

	return( NULL );
}

/**
 * ofa_page_get_export_btn:
 */
GtkWidget *
ofa_page_get_export_btn( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( GTK_WIDGET( page->priv->btn_export ));
	}

	return( NULL );
}

/**
 * ofa_page_get_top_focusable_widget:
 *
 * This virtual function should return the top focusable widget of
 * the page. The default implementation just returns NULL. The main
 * window typically call this virtual when activating a page in
 * order the focus to be correctly set.
 */
GtkWidget *
ofa_page_get_top_focusable_widget( ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		if( OFA_PAGE_GET_CLASS( page )->get_top_focusable_widget ){
			return( OFA_PAGE_GET_CLASS( page )->get_top_focusable_widget( page ));
		}
	}

	return( NULL );
}

/**
 * ofa_page_pre_remove:
 *
 * This function is called by the #ofaMainWindow main window when it is
 * about to remove an #ofaPage from the main notebook. It is time for
 * the #ofaPage-derived class to handle widgets before they are
 * destroyed.
 */
void
ofa_page_pre_remove( ofaPage *page )
{
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( !page->prot->dispose_has_run ){

		if( OFA_PAGE_GET_CLASS( page )->pre_remove ){
			OFA_PAGE_GET_CLASS( page )->pre_remove( page );
		}
	}
}
