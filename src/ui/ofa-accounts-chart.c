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

#include "ui/ofa-main-page.h"
#include "ui/ofa-account-notebook.h"
#include "ui/ofa-account-properties.h"
#include "ui/ofa-accounts-chart.h"
#include "ui/ofo-account.h"

/* private class data
 */
struct _ofaAccountsChartClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

/* private instance data
 */
struct _ofaAccountsChartPrivate {
	gboolean       dispose_has_run;

	/* properties
	 */

	/* are set when run for the first time (page is new)
	 */

	/* UI
	 */
	ofaAccountNotebook *chart_child;
	GtkButton          *update_btn;
	GtkButton          *delete_btn;
	GtkButton          *consult_btn;
};

static GObjectClass *st_parent_class = NULL;

static GType      register_type( void );
static void       class_init( ofaAccountsChartClass *klass );
static void       instance_init( GTypeInstance *instance, gpointer klass );
static void       instance_constructed( GObject *instance );
static void       instance_dispose( GObject *instance );
static void       instance_finalize( GObject *instance );
static void       setup_page_chart( ofaAccountsChart *self );
static GtkWidget *setup_chart_book( ofaAccountsChart *self );
static GtkWidget *setup_buttons_box( ofaAccountsChart *self );
static void       on_account_selected( const gchar *number, ofaAccountsChart *self );
static void       on_new_account( GtkButton *button, ofaAccountsChart *self );
static void       on_update_account( GtkButton *button, ofaAccountsChart *self );
static void       on_delete_account( GtkButton *button, ofaAccountsChart *self );
static gboolean   delete_confirmed( ofaAccountsChart *self, ofoAccount *account );
static void       on_view_entries( GtkButton *button, ofaAccountsChart *self );

GType
ofa_accounts_chart_get_type( void )
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
	static const gchar *thisfn = "ofa_accounts_chart_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaAccountsChartClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaAccountsChart ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFA_TYPE_MAIN_PAGE, "ofaAccountsChart", &info, 0 );

	return( type );
}

static void
class_init( ofaAccountsChartClass *klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	klass->private = g_new0( ofaAccountsChartClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_init";
	ofaAccountsChart *self;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_ACCOUNTS_CHART( instance );

	self->private = g_new0( ofaAccountsChartPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
instance_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_constructed";

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	/* first chain up to the parent class */
	if( G_OBJECT_CLASS( st_parent_class )->constructed ){
		G_OBJECT_CLASS( st_parent_class )->constructed( instance );
	}

	/* then setup the page
	 */
	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	setup_page_chart( OFA_ACCOUNTS_CHART( instance ));
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_dispose";
	ofaAccountsChartPrivate *priv;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	priv = ( OFA_ACCOUNTS_CHART( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_accounts_chart_instance_finalize";
	ofaAccountsChart *self;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_ACCOUNTS_CHART( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
}

/**
 * ofa_accounts_chart_run:
 *
 * When called by the main_window, the page has been created, showed
 * and activated - there is nothing left to do here....
 */
void
ofa_accounts_chart_run( ofaMainPage *this )
{
	static const gchar *thisfn = "ofa_accounts_chart_run";

	g_return_if_fail( this && OFA_IS_ACCOUNTS_CHART( this ));

	g_debug( "%s: this=%p (%s)",
			thisfn, ( void * ) this, G_OBJECT_TYPE_NAME( this ));
}

/*
 * +-----------------------------------------------------------------------+
 * | grid1 (this is the notebook page)                                     |
 * | +-----------------------------------------------+-------------------+ |
 * | | book1                                         |                   | |
 * | | each page of the book contains accounts for   |                   | |
 * | | the corresponding class (if any)              |                   | |
 * | +-----------------------------------------------+-------------------+ |
 * +-----------------------------------------------------------------------+
 */
static void
setup_page_chart( ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_setup_page_chart";
	GtkGrid *grid;
	GtkWidget *chart_book;
	GtkWidget *buttons_box;

	g_debug( "%s: self=%p", thisfn, ( void * ) self );

	grid = ofa_main_page_get_grid( OFA_MAIN_PAGE( self ));

	chart_book = setup_chart_book( self );
	gtk_grid_attach( grid, chart_book, 0, 0, 1, 1 );

	buttons_box = setup_buttons_box( self );
	gtk_grid_attach( grid, buttons_box, 1, 0, 1, 1 );
}

static GtkWidget *
setup_chart_book( ofaAccountsChart *self )
{
	GtkNotebook *chart_book;
	ofaAccountNotebookParms parms;

	chart_book = GTK_NOTEBOOK( gtk_notebook_new());
	gtk_widget_set_margin_left( GTK_WIDGET( chart_book ), 4 );
	gtk_widget_set_margin_bottom( GTK_WIDGET( chart_book ), 4 );

	parms.book = chart_book;
	parms.dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
	parms.pfnSelect = ( ofaAccountNotebookCb ) on_account_selected;
	parms.user_data_select = self;
	parms.pfnDoubleClic = NULL;
	parms.user_data_double_clic = NULL;

	self->private->chart_child = ofa_account_notebook_init_dialog( &parms );

	return( GTK_WIDGET( chart_book ));
}

static GtkWidget *
setup_buttons_box( ofaAccountsChart *self )
{
	GtkBox *buttons_box;
	GtkFrame *frame;
	GtkButton *button;

	buttons_box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 6 ));
	gtk_widget_set_margin_right( GTK_WIDGET( buttons_box ), 4 );

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 30 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_New..." )));
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_new_account ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Update..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_update_account ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->update_btn = button;

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "_Delete..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_delete_account ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->delete_btn = button;

	frame = GTK_FRAME( gtk_frame_new( NULL ));
	gtk_widget_set_size_request( GTK_WIDGET( frame ), -1, 25 );
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_NONE );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( frame ), FALSE, FALSE, 0 );

	button = GTK_BUTTON( gtk_button_new_with_mnemonic( _( "View _entries..." )));
	gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE );
	g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( on_view_entries ), self );
	gtk_box_pack_start( buttons_box, GTK_WIDGET( button ), FALSE, FALSE, 0 );
	self->private->consult_btn = button;

	return( GTK_WIDGET( buttons_box ));
}

/*
 * ofaAccountNotebook callback:
 * first selection occurs during initialization of the chart notebook
 * thus at a moment where the buttons where not yet created
 */
static void
on_account_selected( const gchar *number, ofaAccountsChart *self )
{
	ofoAccount *account;

	account = NULL;

	if( self->private->chart_child ){
		account = ofa_account_notebook_get_selected( self->private->chart_child );
	}

	if( self->private->update_btn ){
		gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->update_btn ), account != NULL );
		gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->delete_btn ), account && ofo_account_is_deletable( account ));
		gtk_widget_set_sensitive(
				GTK_WIDGET( self->private->consult_btn ), account != NULL );
	}
}

static void
on_new_account( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_new_account";
	ofoAccount *account;
	ofaMainWindow *main_window;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	main_window = ofa_main_page_get_main_window( OFA_MAIN_PAGE( self ));

	account = ofo_account_new();
	if( ofa_account_properties_run( main_window, account )){

		/* update our chart of accounts */
		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));
		ofa_main_page_set_dataset(
				OFA_MAIN_PAGE( self ), ofo_account_get_dataset( dossier ));

		/* insert the account in its right place */
		ofa_account_notebook_insert( self->private->chart_child, account );
	}
}

static void
on_update_account( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_update_account";
	ofoAccount *account;
	gchar *prev_number;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	account = ofa_account_notebook_get_selected( self->private->chart_child );
	if( account ){
		prev_number = g_strdup( ofo_account_get_number( account ));

		if( ofa_account_properties_run(
				ofa_main_page_get_main_window( OFA_MAIN_PAGE( self )), account )){

			ofa_account_notebook_remove( self->private->chart_child, prev_number );
			ofa_account_notebook_insert( self->private->chart_child, account );
		}

		g_free( prev_number );
		g_object_unref( account );
	}
}

/*
 * un compte peut être supprimé si ses soldes sont nuls, et après
 * confirmation de l'utilisateur
 */
static void
on_delete_account( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_delete_account";
	ofoAccount *account;
	ofoDossier *dossier;

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );

	account = ofa_account_notebook_get_selected( self->private->chart_child );
	if( account ){
		g_object_unref( account );
		g_return_if_fail( ofo_account_is_deletable( account ));

		dossier = ofa_main_page_get_dossier( OFA_MAIN_PAGE( self ));

		if( delete_confirmed( self, account ) &&
				ofo_account_delete( account, dossier )){

			/* update our chart of accounts */
			ofa_main_page_set_dataset(
					OFA_MAIN_PAGE( self ), ofo_account_get_dataset( dossier ));

			/* remove the row from the model
			 * this will cause an automatic new selection */
			ofa_account_notebook_remove( self->private->chart_child, ofo_account_get_number( account ));
		}
	}
}

static gboolean
delete_confirmed( ofaAccountsChart *self, ofoAccount *account )
{
	gchar *msg;
	gboolean delete_ok;

	msg = g_strdup_printf( _( "Are you sure you want delete the '%s - %s' account ?" ),
			ofo_account_get_number( account ),
			ofo_account_get_label( account ));

	delete_ok = ofa_main_page_delete_confirmed( OFA_MAIN_PAGE( self ), msg );

	g_free( msg );

	return( delete_ok );
}

static void
on_view_entries( GtkButton *button, ofaAccountsChart *self )
{
	static const gchar *thisfn = "ofa_accounts_chart_on_view_entries";

	g_return_if_fail( OFA_IS_ACCOUNTS_CHART( self ));

	g_debug( "%s: button=%p, self=%p", thisfn, ( void * ) button, ( void * ) self );
}
