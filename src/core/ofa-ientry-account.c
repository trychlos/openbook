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

#include "api/ofa-ientry-account.h"

#include "core/ofa-account-select.h"
#include "core/ofa-main-window.h"

/* a data structure attached to each managed GtkEntry
 */
typedef struct {
	ofaIEntryAccount  *instance;
	ofaMainWindow     *main_window;
	ofeAccountAllowed  allowed;
}
	sIEntryAccount;

static guint st_initializations         = 0;	/* interface initialization count */

#define IENTRY_ACCOUNT_LAST_VERSION       1
#define IENTRY_ACCOUNT_DATA               "ofa-ientry-account-data"

static const gchar *st_resource_account = "/org/trychlos/openbook/core/ofa-ientry-account-icon-16.png";

static GType           register_type( void );
static void            interface_base_init( ofaIEntryAccountInterface *klass );
static void            interface_base_finalize( ofaIEntryAccountInterface *klass );
static void            on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, ofaIEntryAccount *instance );
static sIEntryAccount *get_iaccount_entry_data( const ofaIEntryAccount *instance, GtkEntry *entry );
static void            on_entry_finalized( sIEntryAccount *data, GObject *finalized_entry );

/**
 * ofa_ientry_account_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ientry_account_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ientry_account_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ientry_account_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIEntryAccountInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIEntryAccount", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIEntryAccountInterface *klass )
{
	static const gchar *thisfn = "ofa_ientry_account_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIEntryAccountInterface *klass )
{
	static const gchar *thisfn = "ofa_ientry_account_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ientry_account_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ientry_account_get_interface_last_version( void )
{
	return( IENTRY_ACCOUNT_LAST_VERSION );
}

/**
 * ofa_ientry_account_get_interface_version:
 * @instance: this #ofaIEntryAccount instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_ientry_account_get_interface_version( const ofaIEntryAccount *instance )
{
	static const gchar *thisfn = "ofa_ientry_account_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IENTRY_ACCOUNT( instance ), 0 );

	if( OFA_IENTRY_ACCOUNT_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IENTRY_ACCOUNT_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIEntryAccount instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_ientry_account_init:
 * @instance: the #ofaIEntryAccount instance.
 * @main_window: the #ofaMainWindow main window of the application.
 * @entry: the #GtkEntry entry to be set.
 * @allowed: the allowed selection.
 *
 * Initialize the GtkEntry to setup an icon.
 * When this icon is pressed, an account selection dialog is triggered.
 */
void
ofa_ientry_account_init( ofaIEntryAccount *instance, ofaMainWindow *main_window, GtkEntry *entry, ofeAccountAllowed allowed )
{
	sIEntryAccount *sdata;
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	g_return_if_fail( instance && OFA_IS_IENTRY_ACCOUNT( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	gtk_widget_set_halign( GTK_WIDGET( entry ), GTK_ALIGN_START );
	gtk_entry_set_alignment( entry, 0 );
	gtk_entry_set_max_width_chars( entry, ACC_NUMBER_MAX_LENGTH );
	gtk_entry_set_max_length( entry, ACC_NUMBER_MAX_LENGTH );

	sdata = get_iaccount_entry_data( instance, entry );
	sdata->instance = instance;
	sdata->main_window = main_window;
	sdata->allowed = allowed;

	image = gtk_image_new_from_resource( st_resource_account );

#if 0
	GtkImageType type = gtk_image_get_storage_type( GTK_IMAGE( image ));
	g_debug( "ofa_ientry_account_init: storage_type=%s",
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
on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, ofaIEntryAccount *instance )
{
	static const gchar *thisfn = "ofa_ientry_account_on_icon_pressed";
	sIEntryAccount *sdata;
	gchar *initial_selection, *account_id, *tmp;
	GtkWidget *toplevel;

	sdata = get_iaccount_entry_data( instance, entry );

	if( OFA_IENTRY_ACCOUNT_GET_INTERFACE( instance )->on_pre_select ){
		initial_selection = OFA_IENTRY_ACCOUNT_GET_INTERFACE( instance )->on_pre_select( instance, entry, icon_pos, sdata->allowed );
	} else {
		g_info( "%s: ofaIEntryAccount instance %p does not provide 'on_pre_select()' method",
				thisfn, ( void * ) instance );
		initial_selection = g_strdup( gtk_entry_get_text( entry ));
	}

	toplevel = gtk_widget_get_toplevel( GTK_WIDGET( entry ));
	account_id = ofa_account_select_run(
			sdata->main_window, toplevel ? GTK_WINDOW( toplevel ) : NULL, initial_selection, sdata->allowed );

	if( account_id ){
		if( OFA_IENTRY_ACCOUNT_GET_INTERFACE( instance )->on_post_select ){
			tmp = OFA_IENTRY_ACCOUNT_GET_INTERFACE( instance )->on_post_select( instance, entry, icon_pos, sdata->allowed, account_id );
			if( tmp ){
				g_free( account_id );
				account_id = tmp;
			}
		} else {
			g_info( "%s: ofaIEntryAccount instance %p does not provide 'on_post_select()' method",
					thisfn, ( void * ) instance );
		}
		gtk_entry_set_text( entry, account_id );
	}

	g_free( account_id );
	g_free( initial_selection );
}

static sIEntryAccount *
get_iaccount_entry_data( const ofaIEntryAccount *instance, GtkEntry *entry )
{
	sIEntryAccount *sdata;

	sdata = ( sIEntryAccount * ) g_object_get_data( G_OBJECT( entry ), IENTRY_ACCOUNT_DATA );

	if( !sdata ){
		sdata = g_new0( sIEntryAccount, 1 );
		g_object_set_data( G_OBJECT( entry ), IENTRY_ACCOUNT_DATA, sdata );
		g_object_weak_ref( G_OBJECT( entry ), ( GWeakNotify ) on_entry_finalized, sdata );
	}

	return( sdata );
}

static void
on_entry_finalized( sIEntryAccount *sdata, GObject *finalized_entry )
{
	static const gchar *thisfn = "ofa_ientry_account_on_entry_finalized";

	g_debug( "%s: sdata=%p, finalized_entry=%p", thisfn, ( void * ) sdata, ( void * ) finalized_entry );

	g_free( sdata );
}
