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

#include "my/my-utils.h"

#include "api/ofa-action-page.h"
#include "api/ofa-page.h"
#include "api/ofa-page-prot.h"

/* private instance data
 */
typedef struct {
	void *empty;						/* so that gcc -pedantic is happy */
}
	ofaActionPagePrivate;

static void       v_setup_page( ofaPage *page );
static GtkWidget *do_setup_view( ofaActionPage *page );
static void       do_setup_actions( ofaActionPage *page );
static void       do_init_view( ofaActionPage *page );

G_DEFINE_TYPE_EXTENDED( ofaActionPage, ofa_action_page, OFA_TYPE_PAGE, 0,
		G_ADD_PRIVATE( ofaActionPage ))

static void
action_page_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_action_page_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_ACTION_PAGE( instance ));

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_action_page_parent_class )->finalize( instance );
}

static void
action_page_dispose( GObject *instance )
{
	ofaPageProtected *prot;

	g_return_if_fail( instance && OFA_IS_ACTION_PAGE( instance ));

	prot = ( OFA_PAGE( instance ))->prot;

	if( !prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_action_page_parent_class )->dispose( instance );
}

static void
ofa_action_page_init( ofaActionPage *self )
{
	static const gchar *thisfn = "ofa_action_page_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_ACTION_PAGE( self ));
}

static void
ofa_action_page_class_init( ofaActionPageClass *klass )
{
	static const gchar *thisfn = "ofa_action_page_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = action_page_dispose;
	G_OBJECT_CLASS( klass )->finalize = action_page_finalize;

	OFA_PAGE_CLASS( klass )->setup_page = v_setup_page;
}

static void
v_setup_page( ofaPage *page )
{
	GtkWidget *view;

	g_return_if_fail( page && OFA_IS_ACTION_PAGE( page ));

	/* the grid will have two columns:
	 * - setup the spacing between these two columns
	 * - setup the margin around the grid */
	gtk_grid_set_column_spacing( GTK_GRID( page ), 2 );
	my_utils_widget_set_margins( GTK_WIDGET( page ), 2, 2, 2, 2 );

	/* setup the view at row=0, column=0 */
	view = do_setup_view( OFA_ACTION_PAGE( page ));
	if( view ){
		gtk_grid_attach( GTK_GRID( page ), view, 0, 0, 1, 1 );
	}

	/* setup the action buttons at row=0, column=1 */
	do_setup_actions( OFA_ACTION_PAGE( page ));

	/* initialize the view as and of setup */
	do_init_view( OFA_ACTION_PAGE( page ));
}

static GtkWidget *
do_setup_view( ofaActionPage *page )
{
	GtkWidget *view;

	view = NULL;

	if( OFA_ACTION_PAGE_GET_CLASS( page )->setup_view ){
		view = OFA_ACTION_PAGE_GET_CLASS( page )->setup_view( page );
	}

	return( view );
}

static void
do_setup_actions( ofaActionPage *page )
{
	ofaButtonsBox *buttons_box;

	if( OFA_ACTION_PAGE_GET_CLASS( page )->setup_actions ){
		buttons_box = ofa_buttons_box_new();
		gtk_grid_attach( GTK_GRID( page ), GTK_WIDGET( buttons_box ), 1, 0, 1, 1 );
		OFA_ACTION_PAGE_GET_CLASS( page )->setup_actions( page, buttons_box );
	}
}

static void
do_init_view( ofaActionPage *page )
{
	if( OFA_ACTION_PAGE_GET_CLASS( page )->init_view ){
		OFA_ACTION_PAGE_GET_CLASS( page )->init_view( page );
	}
}
