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

#include "my/my-utils.h"

#include "api/ofa-account-editable.h"
#include "api/ofa-igetter.h"

#include "core/ofa-account-select.h"

/* a data structure attached to each managed GtkEditable
 */
typedef struct {
	ofaIGetter          *getter;
	ofeAccountAllowed    allowed;
	AccountPreSelectCb   preselect_cb;
	void                *preselect_user_data;
	AccountPostSelectCb  postselect_cb;
	void                *postselect_user_data;
}
	sAccount;

#define ACCOUNT_EDITABLE_DATA             "ofa-account-editable-data"

static const gchar *st_resource_account = "/org/trychlos/openbook/core/ofa-account-editable-icon-16.png";

static void      on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, void *empty );
static sAccount *get_editable_data( GtkEditable *editable );
static void      on_editable_finalized( sAccount *sdata, GObject *finalized_editable );

/**
 * ofa_account_editable_init:
 * @editable: the #GtkEditable entry.
 * @getter: a #ofaIGetter instance.
 * @allowed: the allowed selection.
 *
 * Initialize the GtkEntry to setup an icon.
 * When this icon is pressed, an account selection dialog is triggered.
 */
void
ofa_account_editable_init( GtkEditable *editable, ofaIGetter *getter, ofeAccountAllowed allowed )
{
	sAccount *sdata;
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = get_editable_data( editable );
	sdata->getter = getter;
	sdata->allowed = allowed;

	if( GTK_IS_ENTRY( editable )){
		gtk_widget_set_halign( GTK_WIDGET( editable ), GTK_ALIGN_START );
		gtk_entry_set_alignment( GTK_ENTRY( editable ), 0 );
		gtk_entry_set_max_width_chars( GTK_ENTRY( editable ), ACC_NUMBER_MAX_LENGTH );
		gtk_entry_set_max_length( GTK_ENTRY( editable ), ACC_NUMBER_MAX_LENGTH );

		image = gtk_image_new_from_resource( st_resource_account );

#if 0
		GtkImageType type = gtk_image_get_storage_type( GTK_IMAGE( image ));
		g_debug( "ofa_account_editable_init: storage_type=%s",
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
		gtk_entry_set_icon_from_pixbuf( GTK_ENTRY( editable ), GTK_ENTRY_ICON_SECONDARY, pixbuf );

#if 0
		if( type == GTK_IMAGE_ICON_NAME ){
			const gchar *icon_name;
			gtk_image_get_icon_name( GTK_IMAGE( image ), &icon_name, NULL );
			g_debug( "icon_name=%s", icon_name );
			gtk_entry_set_icon_from_icon_name( entry, GTK_ENTRY_ICON_SECONDARY, icon_name );
		}
#endif

		g_signal_connect( GTK_ENTRY( editable ), "icon-press", G_CALLBACK( on_icon_pressed ), NULL );
	}
}

static void
on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, void *empty )
{
	sAccount *sdata;
	gchar *initial_selection, *account_id, *tmp;
	GtkWindow *toplevel;

	sdata = get_editable_data( GTK_EDITABLE( entry ));

	if( sdata->preselect_cb ){
		initial_selection = ( *sdata->preselect_cb )( GTK_EDITABLE( entry ), sdata->allowed, sdata->preselect_user_data );
	} else {
		initial_selection = g_strdup( gtk_entry_get_text( entry ));
	}

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( entry ));
	account_id = ofa_account_select_run( sdata->getter, toplevel, initial_selection, sdata->allowed );

	if( account_id ){
		if( sdata->postselect_cb ){
			tmp = ( *sdata->postselect_cb )( GTK_EDITABLE( entry ), sdata->allowed, account_id, sdata->postselect_user_data );
			if( tmp ){
				g_free( account_id );
				account_id = tmp;
			}
		}
		gtk_entry_set_text( entry, account_id );
	}

	g_free( account_id );
	g_free( initial_selection );
}

/**
 * ofa_account_editable_set_preselect_cb:
 * @editable:
 * @cb: [allow-none]:
 * @user_data:
 *
 * Define a callback function which will be called when the user click
 * on the icon selector.
 *
 * It is expected that the callback returns an account identifier to be
 * used as the initial selection of the selection dialog, as a newly
 * allocated string which will be g_free() by #ofa_account_editable.
 */
void
ofa_account_editable_set_preselect_cb ( GtkEditable *editable, AccountPreSelectCb cb, void *user_data )
{
	sAccount *sdata;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	sdata = get_editable_data( editable );

	sdata->preselect_cb = cb;
	sdata->preselect_user_data = user_data;
}

/**
 * ofa_account_editable_set_postselect_cb:
 * @editable:
 * @cb: [allow-none]:
 * @user_data:
 *
 * Define a callback function which will be called when the user returns
 * from a selection.
 *
 * It is expected that the callback returns the finally selected account
 * identifier to be used by the application, as a newly allocated string
 * which will be g_free() by #ofa_account_editable.
 */
void
ofa_account_editable_set_postselect_cb( GtkEditable *editable, AccountPostSelectCb cb, void *user_data )
{
	sAccount *sdata;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	sdata = get_editable_data( editable );

	sdata->postselect_cb = cb;
	sdata->postselect_user_data = user_data;
}

static sAccount *
get_editable_data( GtkEditable *editable )
{
	sAccount *sdata;

	sdata = ( sAccount * ) g_object_get_data( G_OBJECT( editable ), ACCOUNT_EDITABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sAccount, 1 );
		g_object_set_data( G_OBJECT( editable ), ACCOUNT_EDITABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_finalized, sdata );
	}

	return( sdata );
}

static void
on_editable_finalized( sAccount *sdata, GObject *finalized_editable )
{
	static const gchar *thisfn = "ofa_account_editable_on_editable_finalized";

	g_debug( "%s: sdata=%p, finalized_editable=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_editable );

	g_free( sdata );
}
