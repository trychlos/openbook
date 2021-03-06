/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

static void  do_setup_page( ofaPage *page );
static void  iactionable_iface_init( ofaIActionableInterface *iface );
static guint iactionable_get_interface_version( void );

G_DEFINE_TYPE_EXTENDED( ofaPage, ofa_page, GTK_TYPE_GRID, 0,
		G_ADD_PRIVATE( ofaPage )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IACTIONABLE, iactionable_iface_init ))

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
					"ofa-page-getter",
					"Getter",
					"A ofaIGetter instance, to be provided by the instantiator",
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

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

static void
do_setup_page( ofaPage *page )
{
	g_return_if_fail( page && OFA_IS_PAGE( page ));

	if( OFA_PAGE_GET_CLASS( page )->setup_page ){
		OFA_PAGE_GET_CLASS( page )->setup_page( page );
	}
}

/**
 * ofa_page_get_top_focusable_widget:
 *
 * This virtual function should return the top focusable widget of
 * the page. The default implementation just returns %NULL. The main
 * window typically calls this virtual when activating a page in
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

/**
 * ofa_page_get_getter:
 * @page: this #ofaPage instance.
 *
 * Returns: the #ofaIGetter instance which has been set by the #ofaMainWindow
 *  at instanciation time.
 */
ofaIGetter *
ofa_page_get_getter( ofaPage *page )
{
	ofaPagePrivate *priv;

	g_return_val_if_fail( page && OFA_IS_PAGE( page ), NULL );
	g_return_val_if_fail( !page->prot->dispose_has_run, NULL );

	priv = ofa_page_get_instance_private( page );

	return( priv->getter );
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
