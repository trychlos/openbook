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

#include "my/my-iassistant.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#define IASSISTANT_LAST_VERSION         1
#define IASSISTANT_INSTANCE_DATA        "my-iassistant-instance-data"
#define IASSISTANT_PAGE_DATA            "my-iassistant-page-data"

static guint    st_initializations      = 0;		/* interface initialization count */

/* a data structure attached to each myIAssistant instance
 */
typedef struct {

	/* initialization
	 */
	const ofsIAssistant *cbs;

	/* runtime
	 */
	GtkWidget           *prev_page_widget;
	gint                 prev_page_num;
	GtkWidget           *cur_page_widget;

	/* when quitting
	 */
	gboolean             escape_key_pressed;
}
	sIAssistantInstance;

/* a data structure set against each #GtkWidget page
 */
typedef struct {
	gint                 page_num;
	gboolean             initialized;
}
	sIAssistantPage;

static GType                register_type( void );
static void                 interface_base_init( myIAssistantInterface *klass );
static void                 interface_base_finalize( myIAssistantInterface *klass );
static void                 do_setup_once( myIAssistant *instance, sIAssistantInstance *sdata );
static void                 on_prepare( myIAssistant *instance, GtkWidget *page, void *empty );
static void                 do_page_init( myIAssistant *instance, GtkWidget *page, sIAssistantInstance *inst_data, sIAssistantPage *page_data );
static void                 do_page_display( myIAssistant *instance, GtkWidget *page, sIAssistantInstance *inst_data, sIAssistantPage *page_data );
static void                 do_page_forward( myIAssistant *instance, GtkWidget *page, sIAssistantInstance *inst_data );
static void                 on_cancel( myIAssistant *instance, void *empty );
static void                 on_close( myIAssistant *instance, void *empty );
static gboolean             on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, myIAssistant *instance );
static gboolean             is_willing_to_quit( myIAssistant *instance, guint keyval );
static void                 do_close( myIAssistant *instance );
static sIAssistantInstance *get_iassistant_instance_data( myIAssistant *instance );
static void                 on_iassistant_finalized( sIAssistantInstance *sdata, gpointer finalized_iassistant );
static sIAssistantPage     *get_iassistant_page_data( const myIAssistant *instance, GtkWidget *page );
static void                 on_page_finalized( sIAssistantPage *sdata, gpointer finalized_page );

/**
 * my_iassistant_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
my_iassistant_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * my_iassistant_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "my_iassistant_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( myIAssistantInterface ),
		( GBaseInitFunc ) interface_base_init,
		( GBaseFinalizeFunc ) interface_base_finalize,
		NULL,
		NULL,
		NULL,
		0,
		0,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( G_TYPE_INTERFACE, "myIAssistant", &info, 0 );

	g_type_interface_add_prerequisite( type, GTK_TYPE_ASSISTANT );

	return( type );
}

static void
interface_base_init( myIAssistantInterface *klass )
{
	static const gchar *thisfn = "my_iassistant_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( myIAssistantInterface *klass )
{
	static const gchar *thisfn = "my_iassistant_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * my_iassistant_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
my_iassistant_get_interface_last_version( void )
{
	return( IASSISTANT_LAST_VERSION );
}

/**
 * my_iassistant_get_interface_version:
 * @type: the implementation's GType.
 *
 * Returns: the version number of this interface which is managed by
 * the @type implementation.
 *
 * Defaults to 1.
 *
 * Since: version 1.
 */
guint
my_iassistant_get_interface_version( GType type )
{
	gpointer klass, iface;
	guint version;

	klass = g_type_class_ref( type );
	g_return_val_if_fail( klass, 1 );

	iface = g_type_interface_peek( klass, MY_TYPE_IASSISTANT );
	g_return_val_if_fail( iface, 1 );

	version = 1;

	if((( myIAssistantInterface * ) iface )->get_interface_version ){
		version = (( myIAssistantInterface * ) iface )->get_interface_version();

	} else {
		g_info( "%s implementation does not provide 'myIAssistant::get_interface_version()' method",
				g_type_name( type ));
	}

	g_type_class_unref( klass );

	return( version );
}

/**
 * my_iassistant_set_callbacks:
 * @instance: this #myIAssistant instance.
 * @callbacks: an array, -1 terminated, of callback init/display/forward
 *  definitions.
 *
 * Set the callbacks for the pages of the assistant.
 *
 * Note that the interface takes a copy of the provided pointer to the
 * callbacks array. The implementation must so take care to keep this
 * array safe and alive during the run.
 */
void
my_iassistant_set_callbacks( myIAssistant *instance, const ofsIAssistant *cbs )
{
	sIAssistantInstance *inst_data;

	g_return_if_fail( instance && MY_IS_IASSISTANT( instance ));

	inst_data = get_iassistant_instance_data( instance );

	inst_data->cbs = cbs;
}

/*
 * called when the sIAssistant is first initialized
 */
static void
do_setup_once( myIAssistant *instance, sIAssistantInstance *sdata )
{
	/* GtkAssistant signal */
	g_signal_connect( instance, "prepare", G_CALLBACK( on_prepare ), NULL );

	/* terminating the assistant */
	g_signal_connect( instance, "cancel", G_CALLBACK( on_cancel ), NULL );
	g_signal_connect( instance, "close", G_CALLBACK( on_close ), NULL );

	/* deals with 'Esc' key */
	g_signal_connect( instance, "key-press-event", G_CALLBACK( on_key_pressed_event ), instance );
}

/*
 * Preparing a page:
 * - if this is the first time, then call the 'init' cb
 * - only then, call the 'display' cb
 *
 * The "prepare" signal for the first page (usually Introduction) is
 * sent during GtkAssistant construction, so before the derived class
 * has any chance to connect to it.
 */
static void
on_prepare( myIAssistant *instance, GtkWidget *page, void *empty )
{
	static const gchar *thisfn = "my_iassistant_on_prepare";
	sIAssistantInstance *inst_data;
	sIAssistantPage *page_data;

	g_debug( "%s: instance=%p, page=%p, empty=%p",
			thisfn, ( void * ) instance, ( void * ) page, ( void * ) empty );

	g_return_if_fail( instance && GTK_IS_ASSISTANT( instance ));
	g_return_if_fail( page && GTK_IS_WIDGET( page ));

	inst_data = get_iassistant_instance_data( instance );
	g_return_if_fail( inst_data );

	page_data = get_iassistant_page_data( instance, page );
	g_return_if_fail( page_data );

	inst_data->cur_page_widget = page;

	if( inst_data->prev_page_num >= 0 && inst_data->prev_page_num < page_data->page_num ){
		do_page_forward( instance, inst_data->prev_page_widget, inst_data );
	}

	if( !my_iassistant_is_page_initialized( instance, page )){
		do_page_init( instance, page, inst_data, page_data );
		my_iassistant_set_page_initialized( instance, page, TRUE );
	}

	do_page_display( instance, page, inst_data, page_data );

	inst_data->prev_page_widget = page;
	inst_data->prev_page_num = page_data->page_num;
}

static void
do_page_init( myIAssistant *instance, GtkWidget *page, sIAssistantInstance *inst_data, sIAssistantPage *page_data )
{
	static const gchar *thisfn = "my_iassistant_do_page_init";
	gint i;

	g_debug( "%s: instance=%p, page=%p, inst_data=%p, page_data=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) page,
			( void * ) inst_data, ( void * ) page_data, page_data->page_num );

	if( inst_data->cbs ){
		for( i=0 ; inst_data->cbs[i].page_num >= 0 ; ++i ){
			if( inst_data->cbs[i].page_num == page_data->page_num ){
				if( inst_data->cbs[i].init_cb ){
					inst_data->cbs[i].init_cb( instance, page_data->page_num, page );
					break;
				}
			}
		}
	}
}

static void
do_page_display( myIAssistant *instance, GtkWidget *page, sIAssistantInstance *inst_data, sIAssistantPage *page_data )
{
	static const gchar *thisfn = "my_iassistant_do_page_display";
	gint i;

	g_debug( "%s: instance=%p, page=%p, inst_data=%p, page_data=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) page,
			( void * ) inst_data, ( void * ) page_data, page_data->page_num );

	gtk_widget_show_all( page );

	if( inst_data->cbs ){
		for( i=0 ; inst_data->cbs[i].page_num >= 0 ; ++i ){
			if( inst_data->cbs[i].page_num == page_data->page_num ){
				if( inst_data->cbs[i].display_cb ){
					inst_data->cbs[i].display_cb( instance, page_data->page_num, page );
					break;
				}
			}
		}
	}
}

static void
do_page_forward( myIAssistant *instance, GtkWidget *page, sIAssistantInstance *inst_data )
{
	static const gchar *thisfn = "my_iassistant_do_page_forward";
	sIAssistantPage *page_data;
	gint i;

	page_data = get_iassistant_page_data( instance, page );
	g_return_if_fail( page_data );

	g_debug( "%s: instance=%p, page=%p, inst_data=%p, page_data=%p, page_num=%d",
			thisfn, ( void * ) instance, ( void * ) page,
			( void * ) inst_data, ( void * ) page_data, page_data->page_num );

	if( inst_data->cbs ){
		for( i=0 ; inst_data->cbs[i].page_num >= 0 ; ++i ){
			if( inst_data->cbs[i].page_num == page_data->page_num ){
				if( inst_data->cbs[i].forward_cb ){
					inst_data->cbs[i].forward_cb( instance, page_data->page_num, page );
					break;
				}
			}
		}
	}
}

/*
 * the "cancel" message is sent when the user click on the "Cancel"
 * button, or if he hits the 'Escape' key and the 'Quit on escape'
 * preference is set
 *
 *  Key values come from /usr/include/gtk-3.0/gdk/gdkkeysyms.h.
 */
static void
on_cancel( myIAssistant *instance, void *empty )
{
	static const gchar *thisfn = "my_iassistant_on_cancel";

	g_debug( "%s: instance=%p, empty=%p",
			thisfn, ( void * ) instance, ( void * ) empty );

	if( is_willing_to_quit( instance, GDK_KEY_Cancel )){
		do_close( instance );
	}
}

/*
 * the "close" message is sent when the user clicks on "Close" button,
 * after the assistant is complete.
 */
static void
on_close( myIAssistant *instance, void *empty )
{
	static const gchar *thisfn = "my_iassistant_on_close";

	g_debug( "%s: instance=%p, empty=%p",
			thisfn, ( void * ) instance, ( void * ) empty );

	do_close( instance );
}

/*
 *  Key values come from /usr/include/gtk-3.0/gdk/gdkkeysyms.h.
 */
static gboolean
on_key_pressed_event( GtkWidget *widget, GdkEventKey *event, myIAssistant *instance )
{
	gboolean stop = FALSE;

	if( event->keyval == GDK_KEY_Escape && is_willing_to_quit( instance, event->keyval )){
		do_close( instance );
	}

	return( stop );
}

static gboolean
is_willing_to_quit( myIAssistant *instance, guint keyval )
{
	static const gchar *thisfn = "my_iassistant_is_willing_to_quit";

	if( MY_IASSISTANT_GET_INTERFACE( instance )->is_willing_to_quit ){
		return( MY_IASSISTANT_GET_INTERFACE( instance )->is_willing_to_quit( instance, keyval ));
	}

	g_info( "%s: myIAssistant's %s implementation does not provide 'is_willing_to_quit()' method",
			thisfn, G_OBJECT_TYPE_NAME( instance ));
	return( TRUE );
}

static void
do_close( myIAssistant *instance )
{
	if( MY_IS_IWINDOW(( instance ))){
		my_iwindow_close( MY_IWINDOW( instance ));

	} else {
		g_object_unref( instance );
	}
}

/**
 * my_iassistant_is_page_initialized:
 * @instance: this #myIAssistant instance.
 * @page: the #GtkWidget page.
 *
 * Returns: %TRUE if the page has been subject to one-time initialization.
 */
gboolean
my_iassistant_is_page_initialized( const myIAssistant *instance, GtkWidget *page )
{
	sIAssistantPage *page_data;

	g_return_val_if_fail( instance && MY_IS_IASSISTANT( instance ), FALSE );

	page_data = get_iassistant_page_data( instance, page );
	g_return_val_if_fail( page_data, FALSE );

	return( page_data->initialized );
}

/**
 * my_assistant_set_page_initialized:
 * @instance: this #myIAssistant instance.
 * @page: the #GtkWidget page.
 * @initialized: whether the @page has been initialized.
 *
 * Set the initialization status.
 */
void
my_iassistant_set_page_initialized( myIAssistant *instance, GtkWidget *page, gboolean initialized )
{
	sIAssistantPage *page_data;

	g_return_if_fail( instance && MY_IS_IASSISTANT( instance ));

	page_data = get_iassistant_page_data( instance, page );
	g_return_if_fail( page_data );

	page_data->initialized = initialized;
}

/**
 * my_assistant_set_current_page_complete:
 * @instance: this #myIAssistant instance.
 * @complete: whether the @page is completed.
 *
 * Set the completion status.
 */
void
my_iassistant_set_current_page_complete( myIAssistant *instance, gboolean complete )
{
	sIAssistantInstance *inst_data;

	g_return_if_fail( instance && MY_IS_IASSISTANT( instance ));

	inst_data = get_iassistant_instance_data( instance );
	g_return_if_fail( inst_data );

	if( inst_data->cur_page_widget ){
		g_return_if_fail( GTK_IS_WIDGET( inst_data->cur_page_widget ));
		gtk_assistant_set_page_complete( GTK_ASSISTANT( instance ), inst_data->cur_page_widget, complete );
	}
}

/**
 * my_assistant_set_current_page_type:
 * @instance: this #myIAssistant instance.
 * @type: the type to be set.
 *
 * Set the current page type.
 */
void
my_iassistant_set_current_page_type( myIAssistant *instance, GtkAssistantPageType type )
{
	sIAssistantInstance *inst_data;

	g_return_if_fail( instance && MY_IS_IASSISTANT( instance ));

	inst_data = get_iassistant_instance_data( instance );
	g_return_if_fail( inst_data );

	if( inst_data->cur_page_widget ){
		g_return_if_fail( GTK_IS_WIDGET( inst_data->cur_page_widget ));
		gtk_assistant_set_page_type( GTK_ASSISTANT( instance ), inst_data->cur_page_widget, type );
	}
}

static sIAssistantInstance *
get_iassistant_instance_data( myIAssistant *instance )
{
	sIAssistantInstance *sdata;

	sdata = ( sIAssistantInstance * ) g_object_get_data( G_OBJECT( instance ), IASSISTANT_INSTANCE_DATA );

	if( !sdata ){
		sdata = g_new0( sIAssistantInstance, 1 );
		g_object_set_data( G_OBJECT( instance ), IASSISTANT_INSTANCE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( instance ), ( GWeakNotify ) on_iassistant_finalized, sdata );
		do_setup_once( instance, sdata );
	}

	return( sdata );
}

static void
on_iassistant_finalized( sIAssistantInstance *sdata, gpointer finalized_iassistant )
{
	static const gchar *thisfn = "my_iassistant_on_iassistant_finalized";

	g_debug( "%s: sdata=%p, finalized_iassistant=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_iassistant );

	g_free( sdata );
}

static sIAssistantPage *
get_iassistant_page_data( const myIAssistant *instance, GtkWidget *page )
{
	sIAssistantPage *sdata;

	sdata = ( sIAssistantPage * ) g_object_get_data( G_OBJECT( page ), IASSISTANT_PAGE_DATA );

	if( !sdata ){
		sdata = g_new0( sIAssistantPage, 1 );
		sdata->initialized = FALSE;
		sdata->page_num = gtk_assistant_get_current_page( GTK_ASSISTANT( instance ));
		g_object_set_data( G_OBJECT( page ), IASSISTANT_PAGE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( page ), ( GWeakNotify ) on_page_finalized, sdata );
	}

	return( sdata );
}

static void
on_page_finalized( sIAssistantPage *sdata, gpointer finalized_page )
{
	static const gchar *thisfn = "my_iassistant_on_page_finalized";

	g_debug( "%s: sdata=%p, finalized_page=%p", thisfn, ( void * ) sdata, ( void * ) finalized_page );

	g_free( sdata );
}
