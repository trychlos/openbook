/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-paned-page.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"

/* private instance data
 */
typedef struct {
	GtkWidget     *paned;
	GtkOrientation orientation;
	gint           position;
}
	ofaPanedPagePrivate;

/* class properties
 */
enum {
	PROP_ORIENTATION_ID = 1,
	PROP_POSITION_ID,
};

static void       page_v_setup_page( ofaPage *page );
static void       do_setup_view( ofaPanedPage *self );
static void       do_init_view( ofaPanedPage *self );

G_DEFINE_TYPE_EXTENDED( ofaPanedPage, ofa_paned_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaPanedPage ))

static void
paned_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_paned_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PANED_PAGE( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paned_page_parent_class )->finalize( instance );
}

static void
paned_page_dispose( GObject *instance )
{
	ofaPageProtected *prot;

	g_return_if_fail( instance && OFA_IS_PANED_PAGE( instance ));

	prot = OFA_PAGE( instance )->prot;

	if( !prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_paned_page_parent_class )->dispose( instance );
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
paned_page_get_property( GObject *instance, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaPageProtected *prot;
	ofaPanedPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PANED_PAGE( instance ));

	prot = OFA_PAGE( instance )->prot;

	if( !prot->dispose_has_run ){

		priv = ofa_paned_page_get_instance_private( OFA_PANED_PAGE( instance ));

		switch( property_id ){
			case PROP_ORIENTATION_ID:
				g_value_set_int( value, priv->orientation );
				break;

			case PROP_POSITION_ID:
				g_value_set_int( value, priv->position );
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
paned_page_set_property( GObject *instance, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaPageProtected *prot;
	ofaPanedPagePrivate *priv;

	g_return_if_fail( instance && OFA_IS_PANED_PAGE( instance ));

	prot = OFA_PAGE( instance )->prot;

	if( !prot->dispose_has_run ){

		priv = ofa_paned_page_get_instance_private( OFA_PANED_PAGE( instance ));

		switch( property_id ){
			case PROP_ORIENTATION_ID:
				priv->orientation = g_value_get_int( value );
				break;

			case PROP_POSITION_ID:
				priv->position = g_value_get_int( value );
				break;

			default:
				G_OBJECT_WARN_INVALID_PROPERTY_ID( instance, property_id, spec );
				break;
		}
	}
}

static void
ofa_paned_page_init( ofaPanedPage *self )
{
	static const gchar *thisfn = "ofa_paned_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PANED_PAGE( self ));
}

static void
ofa_paned_page_class_init( ofaPanedPageClass *klass )
{
	static const gchar *thisfn = "ofa_paned_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->get_property = paned_page_get_property;
	G_OBJECT_CLASS( klass )->set_property = paned_page_set_property;
	G_OBJECT_CLASS( klass )->dispose = paned_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = paned_page_finalize;

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_ORIENTATION_ID,
			g_param_spec_int(
					"ofa-paned-page-orientation",
					"Paned orientation",
					"The orientation of the paned",
					0, 9, GTK_ORIENTATION_HORIZONTAL,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property(
			G_OBJECT_CLASS( klass ),
			PROP_POSITION_ID,
			g_param_spec_int(
					"ofa-paned-page-position",
					"Paned position",
					"The initial position of the eparator",
					-1, G_MAXINT, 150,
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	OFA_PAGE_CLASS( klass )->setup_page = page_v_setup_page;
}

/*
 * Called from page_constructed()
 */
static void
page_v_setup_page( ofaPage *page )
{
	ofaPanedPagePrivate *priv;
	GtkWidget *view;

	g_return_if_fail( page && OFA_IS_PANED_PAGE( page ));

	priv = ofa_paned_page_get_instance_private( OFA_PANED_PAGE( page ));

	priv->paned = gtk_paned_new( priv->orientation );
	gtk_grid_attach( GTK_GRID( page ), priv->paned, 0, 0, 1, 1 );
	gtk_paned_set_position( GTK_PANED( priv->paned ), priv->position );

	my_utils_widget_set_margins( GTK_WIDGET( page ), 2, 2, 2, 2 );

	do_setup_view( OFA_PANED_PAGE( page ));

	view = gtk_paned_get_child1( GTK_PANED( priv->paned ));
	if( view ){
		my_utils_widget_set_margins( view, 0, 0, 0, 2 );
	}
	view = gtk_paned_get_child2( GTK_PANED( priv->paned ));
	if( view ){
		my_utils_widget_set_margins( view, 0, 0, 2, 0 );
	}

	/* initialize the view at end of setup */
	do_init_view( OFA_PANED_PAGE( page ));
}

static void
do_setup_view( ofaPanedPage *self )
{
	ofaPanedPagePrivate *priv;

	priv = ofa_paned_page_get_instance_private( self );

	if( OFA_PANED_PAGE_GET_CLASS( self )->setup_view ){
		OFA_PANED_PAGE_GET_CLASS( self )->setup_view( self, GTK_PANED( priv->paned ));
	}
}

static void
do_init_view( ofaPanedPage *self )
{
	if( OFA_PANED_PAGE_GET_CLASS( self )->init_view ){
		OFA_PANED_PAGE_GET_CLASS( self )->init_view( self );
	}
}
