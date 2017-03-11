/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-igetter.h"
#include "api/ofa-ope-template-editable.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-ope-template-select.h"

/* a data structure attached to each managed GtkEntry
 */
typedef struct {
	ofaIGetter *getter;
}
	sOpeTemplate;

#define OPE_TEMPLATE_EDITABLE_DATA             "ofa-ope-template-editable-data"

static const gchar *st_resource_ope_template = "/org/trychlos/openbook/core/ofa-ope-template-editable-icon-16.png";

static void          on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, void *empty );
static sOpeTemplate *get_editable_data( GtkEditable *editable );
static void          on_editable_finalized( sOpeTemplate *data, GObject *finalized_editable );

/**
 * ofa_ope_template_editable_init:
 * @editable: the #GtkEditable to be set.
 * @main_window: the #ofaMainWindow main window of the application.
 *
 * Initialize the GtkEntry to setup an icon.
 * When this icon is pressed, an account selection dialog is triggered.
 */
void
ofa_ope_template_editable_init( GtkEditable *editable, ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_ope_template_editable_init";
	sOpeTemplate *sdata;
	GdkPixbuf *pixbuf;
	GError *error;

	g_return_if_fail( editable && GTK_IS_EDITABLE( editable ));
	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));

	sdata = get_editable_data( editable );
	sdata->getter = getter;

	if( GTK_IS_ENTRY( editable )){
		gtk_widget_set_halign( GTK_WIDGET( editable ), GTK_ALIGN_START );
		gtk_entry_set_alignment( GTK_ENTRY( editable ), 0 );
		gtk_entry_set_max_width_chars( GTK_ENTRY( editable ), OTE_MNEMO_MAX_LENGTH );
		gtk_entry_set_max_length( GTK_ENTRY( editable ), OTE_MNEMO_MAX_LENGTH );

		error = NULL;
		pixbuf = gdk_pixbuf_new_from_resource( st_resource_ope_template, &error );
		if( pixbuf ){
			gtk_entry_set_icon_from_pixbuf( GTK_ENTRY( editable ), GTK_ENTRY_ICON_SECONDARY, pixbuf );
		} else {
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
		}

		g_signal_connect( editable, "icon-press", G_CALLBACK( on_icon_pressed ), NULL );
	}
}

static void
on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, void *empty )
{
	sOpeTemplate *sdata;
	gchar *initial_selection, *ope_template_id;
	GtkWindow *toplevel;

	sdata = get_editable_data( GTK_EDITABLE( entry ));

	initial_selection = g_strdup( gtk_entry_get_text( entry ));
	toplevel = my_utils_widget_get_toplevel( GTK_WIDGET( entry ));
	ope_template_id = ofa_ope_template_select_run( sdata->getter, toplevel, initial_selection );

	if( ope_template_id ){
		gtk_entry_set_text( entry, ope_template_id );
	}

	g_free( ope_template_id );
	g_free( initial_selection );
}

static sOpeTemplate *
get_editable_data( GtkEditable *editable )
{
	sOpeTemplate *sdata;

	sdata = ( sOpeTemplate * ) g_object_get_data( G_OBJECT( editable ), OPE_TEMPLATE_EDITABLE_DATA );

	if( !sdata ){
		sdata = g_new0( sOpeTemplate, 1 );
		g_object_set_data( G_OBJECT( editable ), OPE_TEMPLATE_EDITABLE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( editable ), ( GWeakNotify ) on_editable_finalized, sdata );
	}

	return( sdata );
}

static void
on_editable_finalized( sOpeTemplate *sdata, GObject *finalized_editable )
{
	static const gchar *thisfn = "ofa_ope_template_editable_on_editable_finalized";

	g_debug( "%s: sdata=%p, finalized_editable=%p", thisfn, ( void * ) sdata, ( void * ) finalized_editable );

	g_free( sdata );
}
