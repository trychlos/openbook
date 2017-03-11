/*
 * Open Firm Paimeaning
 * A double-entry paimeaning application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Paimeaning is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Paimeaning is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Paimeaning; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "my/my-utils.h"

#include "api/ofa-paimean-editable.h"
#include "api/ofa-igetter.h"
#include "api/ofo-paimean.h"

#include "core/ofa-paimean-select.h"

/* a data structure attached to each managed GtkEditable
 */
typedef struct {
	ofaIGetter          *getter;
	PaimeanPreSelectCb   preselect_cb;
	void                *preselect_user_data;
	PaimeanPostSelectCb  postselect_cb;
	void                *postselect_user_data;
}
	sPaimean;

#define PAIMEAN_EDITABLE_DATA             "ofa-paimean-editable-data"

static const gchar *st_resource_paimean = "/org/trychlos/openbook/core/ofa-paimean-editable-icon-16.png";

static void      on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, void *empty );
static sPaimean *get_editable_data( GtkEditable *editable );
static void      on_editable_finalized( sPaimean *sdata, GObject *finalized_editable );

/**
 * ofa_paimean_editable_init:
 * @editable: the #GtkEditable entry.
 * @getter: a #ofaIGetter instance.
 *
 * Initialize the GtkEntry to setup an icon.
 * When this icon is pressed, an paimean selection dialog is triggered.
 */
void
ofa_paimean_editable_init( GtkEditable *editable, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_paimean_editable_init";
	sPaimean *sdata;
	GdkPixbuf *pixbuf;
	GError *error;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = get_editable_data( editable );
	sdata->getter = getter;

	if( GTK_IS_ENTRY( editable )){
		gtk_widget_set_halign( GTK_WIDGET( editable ), GTK_ALIGN_START );
		gtk_entry_set_alignment( GTK_ENTRY( editable ), 0 );
		gtk_entry_set_width_chars( GTK_ENTRY( editable ), PAM_NUMBER_WIDTH );
		gtk_entry_set_max_width_chars( GTK_ENTRY( editable ), PAM_NUMBER_MAX_LENGTH );
		gtk_entry_set_max_length( GTK_ENTRY( editable ), PAM_NUMBER_MAX_LENGTH );

		error = NULL;
		pixbuf = gdk_pixbuf_new_from_resource( st_resource_paimean, &error );
		if( pixbuf ){
			gtk_entry_set_icon_from_pixbuf( GTK_ENTRY( editable ), GTK_ENTRY_ICON_SECONDARY, pixbuf );
		} else {
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}

		g_signal_connect( GTK_ENTRY( editable ), "icon-press", G_CALLBACK( on_icon_pressed ), NULL );
	}
}

static void
on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, void *empty )
{
	sPaimean *sdata;
	gchar *initial_selection, *paimean_id, *tmp;
	GtkWindow *toplevel;

	sdata = get_editable_data( GTK_EDITABLE( entry ));

	if( sdata->preselect_cb ){
		initial_selection = ( *sdata->preselect_cb )( GTK_EDITABLE( entry ), sdata->preselect_user_data );
	} else {
		initial_selection = g_strdup( gtk_entry_get_text( entry ));
	}

	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( entry ));
	paimean_id = ofa_paimean_select_run( sdata->getter, toplevel, initial_selection );

	if( paimean_id ){
		if( sdata->postselect_cb ){
			tmp = ( *sdata->postselect_cb )( GTK_EDITABLE( entry ), paimean_id, sdata->postselect_user_data );
			if( tmp ){
				g_free( paimean_id );
				paimean_id = tmp;
			}
		}
		gtk_entry_set_text( entry, paimean_id );
	}

	g_free( paimean_id );
	g_free( initial_selection );
}

/**
 * ofa_paimean_editable_set_preselect_cb:
 * @editable:
 * @cb: [allow-none]:
 * @user_data:
 *
 * Define a callback function which will be called when the user click
 * on the icon selector.
 *
 * It is expected that the callback returns an paimean identifier to be
 * used as the initial selection of the selection dialog, as a newly
 * allocated string which will be g_free() by #ofa_paimean_editable.
 */
void
ofa_paimean_editable_set_preselect_cb ( GtkEditable *editable, PaimeanPreSelectCb cb, void *user_data )
{
	sPaimean *sdata;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	sdata = get_editable_data( editable );

	sdata->preselect_cb = cb;
	sdata->preselect_user_data = user_data;
}

/**
 * ofa_paimean_editable_set_postselect_cb:
 * @editable:
 * @cb: [allow-none]:
 * @user_data:
 *
 * Define a callback function which will be called when the user returns
 * from a selection.
 *
 * It is expected that the callback returns the finally selected paimean
 * identifier to be used by the application, as a newly allocated string
 * which will be g_free() by #ofa_paimean_editable.
 */
void
ofa_paimean_editable_set_postselect_cb( GtkEditable *editable, PaimeanPostSelectCb cb, void *user_data )
{
	sPaimean *sdata;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));

	sdata = get_editable_data( editable );

	sdata->postselect_cb = cb;
	sdata->postselect_user_data = user_data;
}

static sPaimean *
get_editable_data( GtkEditable *editable )
{
	sPaimean *sdata;

	sdata = ( sPaimean * ) g_object_get_data( G_OBJECT( editable ), PAIMEAN_EDITABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sPaimean, 1 );
		g_object_set_data( G_OBJECT( editable ), PAIMEAN_EDITABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_finalized, sdata );
	}

	return( sdata );
}

static void
on_editable_finalized( sPaimean *sdata, GObject *finalized_editable )
{
	static const gchar *thisfn = "ofa_paimean_editable_on_editable_finalized";

	g_debug( "%s: sdata=%p, finalized_editable=%p",
			thisfn, ( void * ) sdata, ( void * ) finalized_editable );

	g_free( sdata );
}
