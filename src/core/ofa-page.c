/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-utils.h"

#include "api/ofa-iactionable.h"
#include "api/ofa-igetter.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"
#include "api/ofo-base.h"

/* private instance data
 */
typedef struct {

	/* properties set at instanciation time
	 */
	ofaIGetter *getter;
}
	ofaPagePrivate;

/* class properties
 */
enum {
	PROP_GETTER_ID = 1,
};

/* signals defined here
 */
enum {
	PAGE_REMOVED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static void                  igetter_iface_init( ofaIGetterInterface *iface );
static ofaIGetter           *igetter_get_permanent_getter( const ofaIGetter *instance );
static GApplication         *igetter_get_application( const ofaIGetter *instance );
static ofaHub               *igetter_get_hub( const ofaIGetter *instance );
static GtkApplicationWindow *igetter_get_main_window( const ofaIGetter *instance );
static ofaIThemeManager     *igetter_get_theme_manager( const ofaIGetter *instance );
static void                  iactionable_iface_init( ofaIActionableInterface *iface );
static guint                 iactionable_get_interface_version( void );
static void                  do_setup_page( ofaPage *page );
static void                  v_setup_page( ofaPage *page );
static GtkWidget            *do_setup_view( ofaPage *page );
static GtkWidget            *do_setup_buttons( ofaPage *page );
static void                  do_init_view( ofaPage *page );
static void                  v_init_view( ofaPage *page );
static GtkWidget            *v_get_top_focusable_widget( const ofaPage *page );

G_DEFINE_TYPE_EXTENDED( ofaPage, ofa_page, GTK_TYPE_GRID, 0,
		G_ADD_PRIVATE( ofaPage )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IGETTER, igetter_iface_init ))

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

	if( !prot->dispose_has_run ){

		priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

		switch( property_id ){
			case PROP_GETTER_ID:
				g_value_set_pointer( value, priv->getter );
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

		priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

		switch( property_id ){
			case PROP_GETTER_ID:
				priv->getter = g_value_get_pointer( value );
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

	priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

	g_debug( "%s: instance=%p (%s), getter=%p (%s)",
			thisfn,
			( void * ) instance, G_OBJECT_TYPE_NAME( instance ),
			( void * ) priv->getter, G_OBJECT_TYPE_NAME( priv->getter ));

	/* let the child class setup its page before showing it */
	do_setup_page( self );
	do_init_view( self );
	gtk_widget_show_all( GTK_WIDGET( instance ));
}

static void
ofa_page_init( ofaPage *self )
{
	static const gchar *thisfn = "ofa_page_init";
	ofaPagePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PAGE( self ));

	self->prot = g_new0( ofaPageProtected, 1 );
	self->prot->dispose_has_run = FALSE;

	priv = ofa_page_get_instance_private( self );
	priv->getter = NULL;
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

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_GETTER_ID,
			g_param_spec_pointer(
					PAGE_PROP_GETTER,
					"Getter",
					"A ofaIGetter instance, to be provided by the instantiator",
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	klass->setup_page = v_setup_page;
	klass->setup_view = NULL;
	klass->setup_buttons = NULL;
	klass->init_view = v_init_view;
	klass->get_top_focusable_widget = v_get_top_focusable_widget;

	/**
	 * ofaPage::page-removed:
	 *
	 * This signal is proxied by the main window after the page has
	 * been removed from the main notebook, and before the top #GtkGrid
	 * be destroyed.
	 * This is mostly useful when the page needs to be informed of its
	 * next closing, while having yet all its widgets available.
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

/*
 * ofaIGetter interface management
 */
static void
igetter_iface_init( ofaIGetterInterface *iface )
{
	static const gchar *thisfn = "ofa_page_igetter_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_permanent = igetter_get_permanent_getter;
	iface->get_application = igetter_get_application;
	iface->get_hub = igetter_get_hub;
	iface->get_main_window = igetter_get_main_window;
	iface->get_theme_manager = igetter_get_theme_manager;
}

/*
 * #ofaPage's life is not expected to be as long as those of the
 * #ofoDossier, so asks to the main window.
 */
static ofaIGetter *
igetter_get_permanent_getter( const ofaIGetter *instance )
{
	GtkApplicationWindow *main_window;

	g_return_val_if_fail( !OFA_PAGE( instance )->prot->dispose_has_run, NULL );

	main_window = igetter_get_main_window( instance );
	g_return_val_if_fail( main_window && OFA_IS_IGETTER( main_window ), NULL );

	return( ofa_igetter_get_permanent_getter( OFA_IGETTER( main_window )));
}

static GApplication *
igetter_get_application( const ofaIGetter *instance )
{
	ofaPagePrivate *priv;

	g_return_val_if_fail( !OFA_PAGE( instance )->prot->dispose_has_run, NULL );

	priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

	return( ofa_igetter_get_application( priv->getter ));
}

static ofaHub *
igetter_get_hub( const ofaIGetter *instance )
{
	ofaPagePrivate *priv;

	g_return_val_if_fail( !OFA_PAGE( instance )->prot->dispose_has_run, NULL );

	priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

	return( ofa_igetter_get_hub( priv->getter ));
}

static GtkApplicationWindow *
igetter_get_main_window( const ofaIGetter *instance )
{
	ofaPagePrivate *priv;

	g_return_val_if_fail( !OFA_PAGE( instance )->prot->dispose_has_run, NULL );

	priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

	return( ofa_igetter_get_main_window( priv->getter ));
}

static ofaIThemeManager *
igetter_get_theme_manager( const ofaIGetter *instance )
{
	ofaPagePrivate *priv;

	g_return_val_if_fail( !OFA_PAGE( instance )->prot->dispose_has_run, NULL );

	priv = ofa_page_get_instance_private( OFA_PAGE( instance ));

	return( ofa_igetter_get_theme_manager( priv->getter ));
}

/*
 * ofaIActionable interface management
 */
static void
iactionable_iface_init( ofaIActionableInterface *iface )
{
	static const gchar *thisfn = "ofa_page_iactionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iactionable_get_interface_version;
}

static guint
iactionable_get_interface_version( void )
{
	return( 1 );
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
v_setup_page( ofaPage *page )
{
	GtkWidget *view, *buttons_box;

	g_return_if_fail( page && OFA_IS_PAGE( page ));

	view = do_setup_view( page );
	if( view ){
		gtk_grid_attach( GTK_GRID( page ), view, 0, 0, 1, 1 );
	}

	buttons_box = do_setup_buttons( page );
	if( buttons_box ){
		gtk_grid_attach( GTK_GRID( page ), buttons_box, 1, 0, 1, 1 );
	}
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
	static const gchar *thisfn = "ofa_page_do_setup_buttons";
	GtkWidget *buttons_box;

	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

	buttons_box = NULL;

	if( OFA_PAGE_GET_CLASS( page )->setup_buttons ){
		buttons_box = OFA_PAGE_GET_CLASS( page )->setup_buttons( page );
	} else {
		g_debug( "%s: page=%p", thisfn, ( void * ) page );
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

static void
v_init_view( ofaPage *page )
{
	static const gchar *thisfn = "ofa_page_v_init_view";

	g_debug( "%s: page=%p (%s)", thisfn, ( void * ) page, G_OBJECT_TYPE_NAME( page ));
}

/*
 * this is only called if the derived class doesn't provide its own
 * version of the 'get_top_focusable_widget' virtual method
 */
static GtkWidget *
v_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );

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
ofa_page_get_top_focusable_widget( const ofaPage *page )
{
	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );
	g_return_val_if_fail( !page->prot->dispose_has_run, NULL );

	if( OFA_PAGE_GET_CLASS( page )->get_top_focusable_widget ){
		return( OFA_PAGE_GET_CLASS( page )->get_top_focusable_widget( page ));
	}

	return( NULL );
}
