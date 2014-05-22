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

#include "ui/ofa-main-page.h"

/* private class data
 */
struct _ofaMainPageClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

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

	/* are set when run for the first time (page is new)
	 */
	GList         *dataset;				/* a g_list_copy of the dataset */
};

/* class properties
 */
enum {
	PROP_0,

	PROP_WINDOW_ID,
	PROP_DOSSIER_ID,
	PROP_GRID_ID,
	PROP_THEME_ID,

	PROP_N_PROPERTIES
};

/* signals defined
 */
enum {
	JOURNAL_CHANGED = 0,
	LAST_SIGNAL
};
static GObjectClass *st_parent_class           = NULL;
static gint          st_signals[ LAST_SIGNAL ] = { 0 };

static GType register_type( void );
static void  class_init( ofaMainPageClass *klass );
static void  instance_init( GTypeInstance *instance, gpointer klass );
static void  instance_constructed( GObject *instance );
static void  instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec );
static void  instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec );
static void  instance_dispose( GObject *instance );
static void  instance_finalize( GObject *instance );
static void  on_grid_finalized( ofaMainPage *self, GObject *grid );
static void  on_journal_changed_class_handler( ofaMainPage *page, ofaMainPageUpdateType type, ofoBase *journal );
static void  main_page_free_dataset( const ofaMainPage *page );

GType
ofa_main_page_get_type( void )
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
	static const gchar *thisfn = "ofa_main_page_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofaMainPageClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) class_init,
		NULL,
		NULL,
		sizeof( ofaMainPage ),
		0,
		( GInstanceInitFunc ) instance_init
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_OBJECT, "ofaMainPage", &info, 0 );

	return( type );
}

static void
class_init( ofaMainPageClass *klass )
{
	static const gchar *thisfn = "ofa_main_page_class_init";
	GObjectClass *object_class;

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	st_parent_class = g_type_class_peek_parent( klass );

	object_class = G_OBJECT_CLASS( klass );
	object_class->constructed = instance_constructed;
	object_class->get_property = instance_get_property;
	object_class->set_property = instance_set_property;
	object_class->dispose = instance_dispose;
	object_class->finalize = instance_finalize;

	g_object_class_install_property( object_class, PROP_WINDOW_ID,
			g_param_spec_pointer(
					MAIN_PAGE_PROP_WINDOW,
					"Main window",
					"The main window (ofaMainWindow *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, PROP_DOSSIER_ID,
			g_param_spec_pointer(
					MAIN_PAGE_PROP_DOSSIER,
					"Current dossier",
					"The currently opened dossier (ofoDossier *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, PROP_GRID_ID,
			g_param_spec_pointer(
					MAIN_PAGE_PROP_GRID,
					"Page grid",
					"The top child of the page (GtkGrid *)",
					G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE ));

	g_object_class_install_property( object_class, PROP_THEME_ID,
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
					MAIN_PAGE_SIGNAL_JOURNAL_UPDATED,
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

	klass->private = g_new0( ofaMainPageClassPrivate, 1 );
}

static void
instance_init( GTypeInstance *instance, gpointer klass )
{
	static const gchar *thisfn = "ofa_main_page_instance_init";
	ofaMainPage *self;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	g_debug( "%s: instance=%p (%s), klass=%p",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), ( void * ) klass );

	self = OFA_MAIN_PAGE( instance );

	self->private = g_new0( ofaMainPagePrivate, 1 );

	self->private->dispose_has_run = FALSE;
	self->private->theme = -1;
}

static void
instance_constructed( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_page_instance_constructed";
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	priv = ( OFA_MAIN_PAGE( instance ))->private;

	/* first, chain up to the parent class */
	if( G_OBJECT_CLASS( st_parent_class )->constructed ){
		G_OBJECT_CLASS( st_parent_class )->constructed( instance );
	}

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

#if 0
	g_debug( "%s: main_window=%p, dossier=%p, theme=%d, grid=%p",
			thisfn,
			( void * ) priv->main_window,
			( void * ) priv->dossier,
			priv->theme,
			( void * ) priv->grid );
#endif

	/* attach a weak reference to the grid widget to unref this object */
	g_return_if_fail( priv->grid && GTK_IS_GRID( priv->grid ));
	g_object_weak_ref(
			G_OBJECT( priv->grid ),
			( GWeakNotify ) on_grid_finalized,
			OFA_MAIN_PAGE( instance ));
}

/*
 * user asks for a property
 * we have so to put the corresponding data into the provided GValue
 */
static void
instance_get_property( GObject *object, guint property_id, GValue *value, GParamSpec *spec )
{
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( object ));
	priv = OFA_MAIN_PAGE( object )->private;

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
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

/*
 * the user asks to set a property and provides it into a GValue
 * read the content of the provided GValue and set our object datas
 */
static void
instance_set_property( GObject *object, guint property_id, const GValue *value, GParamSpec *spec )
{
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( object ));
	priv = OFA_MAIN_PAGE( object )->private;

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
				G_OBJECT_WARN_INVALID_PROPERTY_ID( object, property_id, spec );
				break;
		}
	}
}

static void
instance_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_page_instance_dispose";
	ofaMainPagePrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	priv = ( OFA_MAIN_PAGE( instance ))->private;

	if( !priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		priv->dispose_has_run = TRUE;

		main_page_free_dataset( OFA_MAIN_PAGE( instance ));

		/* chain up to the parent class */
		if( G_OBJECT_CLASS( st_parent_class )->dispose ){
			G_OBJECT_CLASS( st_parent_class )->dispose( instance );
		}
	}
}

static void
instance_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_main_page_instance_finalize";
	ofaMainPage *self;

	g_return_if_fail( OFA_IS_MAIN_PAGE( instance ));

	g_debug( "%s: instance=%p (%s)", thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFA_MAIN_PAGE( instance );

	g_free( self->private );

	/* chain call to parent class */
	if( G_OBJECT_CLASS( st_parent_class )->finalize ){
		G_OBJECT_CLASS( st_parent_class )->finalize( instance );
	}
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

/*
 * default class handler for MAIN_PAGE_SIGNAL_JOURNAL_UPDATED signal
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
 * ofa_main_page_set_main_window:
 */
void
ofa_main_page_set_main_window( ofaMainPage *page, ofaMainWindow *window )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));
	/* let the user set a null main window, but not anything */
	g_return_if_fail( !window || OFA_IS_MAIN_WINDOW( window ));

	if( !page->private->dispose_has_run ){

		page->private->main_window = window;
	}
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
 * ofa_main_page_set_dossier:
 */
void
ofa_main_page_set_dossier( ofaMainPage *page, ofoDossier *dossier )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));
	/* let the user set a null dossier, but not anything */
	g_return_if_fail( !dossier || OFO_IS_DOSSIER( dossier ));

	if( !page->private->dispose_has_run ){

		page->private->dossier = dossier;
	}
}

/*
 * ofa_main_page_free_dataset:
 */
static void
main_page_free_dataset( const ofaMainPage *page )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( page->private->dataset ){

		g_list_free( page->private->dataset );
		page->private->dataset = NULL;
	}
}

/**
 * ofa_main_page_get_dataset:
 */
GList *
ofa_main_page_get_dataset( const ofaMainPage *page )
{
	g_return_val_if_fail( page && OFA_IS_MAIN_PAGE( page ), NULL );

	if( !page->private->dispose_has_run ){

		return( page->private->dataset );
	}

	return( NULL );
}

/**
 * ofa_main_page_set_dataset:
 */
void
ofa_main_page_set_dataset( ofaMainPage *page, GList *dataset )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));

	if( !page->private->dispose_has_run ){

		main_page_free_dataset( page );
		page->private->dataset = g_list_copy( dataset );
	}
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

/**
 * ofa_main_page_set_grid:
 */
void
ofa_main_page_set_grid( ofaMainPage *page, GtkGrid *grid )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));
	/* let the user set a null widget, but not anything */
	g_return_if_fail( !grid || GTK_IS_GRID( grid ));

	if( !page->private->dispose_has_run ){

		page->private->grid = grid;
	}
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
 * ofa_main_page_set_theme:
 *
 * Valid theme identifier is -1 (unset), or greater or equal to 1.
 */
void
ofa_main_page_set_theme( ofaMainPage *page, gint theme )
{
	g_return_if_fail( page && OFA_IS_MAIN_PAGE( page ));
	/* let the user unset the theme */
	g_return_if_fail( theme == -1 || theme >= 1 );

	if( !page->private->dispose_has_run ){

		page->private->theme = theme;
	}
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
