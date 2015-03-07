/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-utils.h"
#include "api/ofo-base.h"

#include "ui/ofa-main-window.h"
#include "ui/ofa-page.h"
#include "ui/ofa-page-prot.h"

/* private instance data
 */
struct _ofaPagePrivate {
	gboolean       from_widget_finalized;

	/* properties set at instanciation time
	 */
	ofaMainWindow *main_window;
	GtkGrid       *top_grid;
	gint           theme;

	/* UI
	 */
};

/* class properties
 */
enum {
	PROP_MAIN_WINDOW_ID = 1,
	PROP_TOP_GRID_ID,
	PROP_THEME_ID,
};

/* signals defined here
 */
enum {
	PAGE_REMOVED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

G_DEFINE_TYPE( ofaPage, ofa_page, G_TYPE_OBJECT )

static void       do_setup_page( ofaPage *page );
static void       v_setup_page_default( ofaPage *page );
static GtkWidget *do_setup_view( ofaPage *page );
static GtkWidget *do_setup_buttons( ofaPage *page );
static void       do_init_view( ofaPage *page );
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
	ofaPagePrivate *priv;
	ofaPageProtected *prot;

	g_return_if_fail( instance && OFA_IS_PAGE( instance ));

	prot = ( OFA_PAGE( instance ))->prot;

	if( !prot->dispose_has_run ){

		prot->dispose_has_run = TRUE;

		/* unref object members here */
		priv = ( OFA_PAGE( instance ))->priv;

		if( !priv->from_widget_finalized ){
			g_object_weak_unref(
					G_OBJECT( priv->top_grid ), ( GWeakNotify ) on_grid_finalized, instance );
		}
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

	if( !prot->dispose_has_run ){

		priv = OFA_PAGE( instance )->priv;

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				g_value_set_pointer( value, priv->main_window );
				break;

			case PROP_TOP_GRID_ID:
				g_value_set_pointer( value, priv->top_grid );
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
page_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaPageProtected *prot;
	ofaPagePrivate *priv;

	g_return_if_fail( OFA_IS_PAGE( instance ));

	prot = OFA_PAGE( instance )->prot;

	if( !prot->dispose_has_run ){

		priv = OFA_PAGE( instance )->priv;

		switch( property_id ){
			case PROP_MAIN_WINDOW_ID:
				priv->main_window = g_value_get_pointer( value );
				break;

			case PROP_TOP_GRID_ID:
				priv->top_grid = g_value_get_pointer( value );
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

/*
 * called during instance initialization, after properties have been set
 */
static void
page_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_page_constructed";
	ofaPage *self;
	ofaPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAGE( instance ));

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( ofa_page_parent_class )->constructed ){
		G_OBJECT_CLASS( ofa_page_parent_class )->constructed( instance );
	}

	self = OFA_PAGE( instance );
	priv = self->priv;

	g_debug( "%s: instance=%p (%s), main_window=%p, top_grid=%p, theme=%d",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) priv->main_window,
			( void * ) priv->top_grid,
			priv->theme );

	/* attach a weak reference to the grid widget to unref this object
	 * and (more useful) the derived class which handles it */
	g_return_if_fail( priv->top_grid && GTK_IS_GRID( priv->top_grid ));
	g_object_weak_ref(
			G_OBJECT( priv->top_grid ), ( GWeakNotify ) on_grid_finalized, instance );

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
	self->priv->main_window = NULL;
	self->priv->top_grid = NULL;
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
			PROP_MAIN_WINDOW_ID,
			g_param_spec_pointer(
					PAGE_PROP_MAIN_WINDOW,
					"Main window",
					"The main window (ofaMainWindow *)",
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_TOP_GRID_ID,
			g_param_spec_pointer(
					PAGE_PROP_TOP_GRID,
					"Page grid",
					"The top child of the page (GtkGrid *)",
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

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
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	klass->setup_page = v_setup_page_default;
	klass->setup_view = NULL;
	klass->init_view = NULL;
	klass->on_button_clicked = NULL;
	klass->get_top_focusable_widget = NULL;

	/**
	 * ofaPage::page-removed:
	 *
	 * This signal is proxied by the main window after the page has
	 * been removed from the main notebook.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaPage     *page,
	 * 						GtkWidget *page_widget,
	 * 						guint      page_num,
	 * 						gpointer   user_data );
	 */
	st_signals[ PAGE_REMOVED ] = g_signal_new_class_handler(
				"page-removed",
				OFA_TYPE_PAGE,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_UINT );
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
		gtk_grid_attach( page->priv->top_grid, view, 0, 0, 1, 1 );
	}

	buttons_box = do_setup_buttons( page );
	if( buttons_box ){
		gtk_grid_attach( page->priv->top_grid, buttons_box, 1, 0, 1, 1 );
	}

	gtk_widget_show_all( GTK_WIDGET( page->priv->top_grid ));
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

/**
 * ofa_page_get_main_window:
 */
ofaMainWindow *
ofa_page_get_main_window( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( page->priv->main_window );
	}

	return( NULL );
}

/**
 * ofa_page_get_top_grid:
 */
GtkGrid *
ofa_page_get_top_grid( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( page->priv->top_grid );
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
 * ofa_page_get_dossier:
 */
ofoDossier *
ofa_page_get_dossier( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		return( ofa_main_window_get_dossier( page->priv->main_window ));
	}

	return( NULL );
}

static void
on_grid_finalized( ofaPage *self, GObject *finalized_grid )
{
	static const gchar *thisfn = "ofa_page_on_grid_finalized";
	ofaPagePrivate *priv;

	g_debug( "%s: self=%p (%s), finalized_grid=%p (%s)",
			thisfn,
			( void * ) self, G_OBJECT_TYPE_NAME( self ),
			( void * ) finalized_grid, G_OBJECT_TYPE_NAME( finalized_grid ));

	g_return_if_fail( self && OFA_IS_PAGE( self ));

	priv = self->priv;
	priv->from_widget_finalized = TRUE;

	g_object_unref( self );
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
ofa_page_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	if( !page->prot->dispose_has_run ){

		if( OFA_PAGE_GET_CLASS( page )->get_top_focusable_widget ){
			return( OFA_PAGE_GET_CLASS( page )->get_top_focusable_widget( page ));
		}
	}

	return( NULL );
}
