/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#include <string.h>

#include <core/ofa-main-window.h>

#include <ui/ofa-iaccount-entry.h>
#include <ui/ofa-account-select.h>

/* a data structure attached to each managed GtkEntry
 */
typedef struct {
	ofaIAccountEntry *instance;
	ofaMainWindow    *main_window;
	ofeAccountAllowed allowed;
}
	sIAccountEntry;

static guint st_initializations         = 0;	/* interface initialization count */

#define IACCOUNT_ENTRY_LAST_VERSION       1
#define IACCOUNT_ENTRY_DATA               "ofa-iaccount-entry-data"

static const gchar *st_resource_account   = "/org/trychlos/openbook/ui/ofa-iaccount-entry-icon-16.png";

static GType           register_type( void );
static void            interface_base_init( ofaIAccountEntryInterface *klass );
static void            interface_base_finalize( ofaIAccountEntryInterface *klass );
static void            on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, ofaIAccountEntry *instance );
static sIAccountEntry *get_iaccount_entry_data( const ofaIAccountEntry *instance, GtkEntry *entry );
static void            on_entry_finalized( sIAccountEntry *data, GObject *finalized_entry );

/**
 * ofa_iaccount_entry_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_iaccount_entry_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_iaccount_entry_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_iaccount_entry_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIAccountEntryInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIAccountEntry", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIAccountEntryInterface *klass )
{
	static const gchar *thisfn = "ofa_iaccount_entry_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIAccountEntryInterface *klass )
{
	static const gchar *thisfn = "ofa_iaccount_entry_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_iaccount_entry_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_iaccount_entry_get_interface_last_version( void )
{
	return( IACCOUNT_ENTRY_LAST_VERSION );
}

/**
 * ofa_iaccount_entry_get_interface_version:
 * @instance: this #ofaIAccountEntry instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_iaccount_entry_get_interface_version( const ofaIAccountEntry *instance )
{
	static const gchar *thisfn = "ofa_iaccount_entry_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IACCOUNT_ENTRY( instance ), 0 );

	if( OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIAccountEntry instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_iaccount_entry_init:
 * @instance: the #ofaIAccountEntry instance.
 * @entry: the #GtkEntry entry to be set.
 * @main_window: the #ofaMainWindow main window of the application.
 * @allowed: the allowed selection.
 *
 * Initialize the GtkEntry to setup an icon.
 * When this icon is pressed, an account selection dialog is triggered.
 */
void
ofa_iaccount_entry_init( ofaIAccountEntry *instance, GtkEntry *entry, ofaMainWindow *main_window, ofeAccountAllowed allowed )
{
	sIAccountEntry *sdata;
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	g_return_if_fail( instance && OFA_IS_IACCOUNT_ENTRY( instance ));
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));

	sdata = get_iaccount_entry_data( instance, entry );
	sdata->instance = instance;
	sdata->main_window = main_window;
	sdata->allowed = allowed;

	image = gtk_image_new_from_resource( st_resource_account );

#if 0
	GtkImageType type = gtk_image_get_storage_type( GTK_IMAGE( image ));
	g_debug( "ofa_iaccount_entry_init: storage_type=%s",
			type == GTK_IMAGE_EMPTY ? "GTK_IMAGE_EMPTY" : (
			type == GTK_IMAGE_PIXBUF ? "GTK_IMAGE_PIXBUF":(
			type == GTK_IMAGE_STOCK ? "GTK_IMAGE_STOCK" : (
			type == GTK_IMAGE_ICON_SET ? "GTK_IMAGE_ICON_SET" : (
			type == GTK_IMAGE_ANIMATION ? "GTK_IMAGE_ANIMATION" : (
			type == GTK_IMAGE_ICON_NAME ? "GTK_IMAGE_ICON_NAME" : (
			type == GTK_IMAGE_GICON ? "GTK_IMAGE_GICON" : (
			type == GTK_IMAGE_SURFACE ? "GTK_IMAGE_SURFACE" : "unknwon" ))))))));
#endif

	pixbuf = gtk_image_get_pixbuf( GTK_IMAGE( image ));
	gtk_entry_set_icon_from_pixbuf( entry, GTK_ENTRY_ICON_SECONDARY, pixbuf );

#if 0
	if( type == GTK_IMAGE_ICON_NAME ){
		const gchar *icon_name;
		gtk_image_get_icon_name( GTK_IMAGE( image ), &icon_name, NULL );
		g_debug( "icon_name=%s", icon_name );
		gtk_entry_set_icon_from_icon_name( entry, GTK_ENTRY_ICON_SECONDARY, icon_name );
	}
#endif

	g_signal_connect( entry, "icon-press", G_CALLBACK( on_icon_pressed ), instance );
}

static void
on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, ofaIAccountEntry *instance )
{
	static const gchar *thisfn = "ofa_iaccount_entry_on_icon_pressed";
	sIAccountEntry *sdata;
	gchar *initial_selection, *account_id, *tmp;

	sdata = get_iaccount_entry_data( instance, entry );

	if( OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance )->on_pre_select ){
		initial_selection = OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance )->on_pre_select( instance, entry, icon_pos, sdata->allowed );
	} else {
		g_info( "%s: ofaIAccountEntry instance %p does not provide 'on_icon_pressed()' method",
				thisfn, ( void * ) instance );
		initial_selection = g_strdup( gtk_entry_get_text( entry ));
	}

	account_id = ofa_account_select_run( sdata->main_window, initial_selection, sdata->allowed );

	if( account_id ){
		if( OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance )->on_post_select ){
			tmp = OFA_IACCOUNT_ENTRY_GET_INTERFACE( instance )->on_post_select( instance, entry, icon_pos, sdata->allowed, account_id );
			if( tmp ){
				g_free( account_id );
				account_id = tmp;
			}
		} else {
			g_info( "%s: ofaIAccountEntry instance %p does not provide 'on_post_select()' method",
					thisfn, ( void * ) instance );
		}
		gtk_entry_set_text( entry, account_id );
	}

	g_free( account_id );
	g_free( initial_selection );
}

static sIAccountEntry *
get_iaccount_entry_data( const ofaIAccountEntry *instance, GtkEntry *entry )
{
	sIAccountEntry *sdata;

	sdata = ( sIAccountEntry * ) g_object_get_data( G_OBJECT( entry ), IACCOUNT_ENTRY_DATA );

	if( !sdata ){
		sdata = g_new0( sIAccountEntry, 1 );
		g_object_set_data( G_OBJECT( entry ), IACCOUNT_ENTRY_DATA, sdata );
		g_object_weak_ref( G_OBJECT( entry ), ( GWeakNotify ) on_entry_finalized, sdata );
	}

	return( sdata );
}

static void
on_entry_finalized( sIAccountEntry *sdata, GObject *finalized_entry )
{
	static const gchar *thisfn = "ofa_iaccount_entry_on_entry_finalized";

	g_debug( "%s: sdata=%p, finalized_entry=%p", thisfn, ( void * ) sdata, ( void * ) finalized_entry );

	g_free( sdata );
}
