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

#include "api/ofa-ientry-ope-template.h"
#include "api/ofo-ope-template.h"

#include "core/ofa-main-window.h"
#include "core/ofa-ope-template-select.h"

/* a data structure attached to each managed GtkEntry
 */
typedef struct {
	ofaIEntryOpeTemplate *instance;
	ofaMainWindow        *main_window;
}
	sIEntryOpeTemplate;

static guint st_initializations              = 0;	/* interface initialization count */

#define IENTRY_OPE_TEMPLATE_LAST_VERSION       1
#define IENTRY_OPE_TEMPLATE_DATA               "ofa-ientry-ope-template-data"

static const gchar *st_resource_ope_template = "/org/trychlos/openbook/core/ofa-ientry-ope-template-icon-16.png";

static GType               register_type( void );
static void                interface_base_init( ofaIEntryOpeTemplateInterface *klass );
static void                interface_base_finalize( ofaIEntryOpeTemplateInterface *klass );
static void                on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, ofaIEntryOpeTemplate *instance );
static sIEntryOpeTemplate *get_ientry_ope_template_data( const ofaIEntryOpeTemplate *instance, GtkEntry *entry );
static void                on_entry_finalized( sIEntryOpeTemplate *data, GObject *finalized_entry );

/**
 * ofa_ientry_ope_template_get_type:
 *
 * Returns: the #GType type of this interface.
 */
GType
ofa_ientry_ope_template_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

/*
 * ofa_ientry_ope_template_register_type:
 *
 * Registers this interface.
 */
static GType
register_type( void )
{
	static const gchar *thisfn = "ofa_ientry_ope_template_register_type";
	GType type;

	static const GTypeInfo info = {
		sizeof( ofaIEntryOpeTemplateInterface ),
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

	type = g_type_register_static( G_TYPE_INTERFACE, "ofaIEntryOpeTemplate", &info, 0 );

	g_type_interface_add_prerequisite( type, G_TYPE_OBJECT );

	return( type );
}

static void
interface_base_init( ofaIEntryOpeTemplateInterface *klass )
{
	static const gchar *thisfn = "ofa_ientry_ope_template_interface_base_init";

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p (%s)", thisfn, ( void * ) klass, G_OBJECT_CLASS_NAME( klass ));

		/* declare here the default implementations */
	}

	st_initializations += 1;
}

static void
interface_base_finalize( ofaIEntryOpeTemplateInterface *klass )
{
	static const gchar *thisfn = "ofa_ientry_ope_template_interface_base_finalize";

	st_initializations -= 1;

	if( st_initializations == 0 ){
		g_debug( "%s: klass=%p", thisfn, ( void * ) klass );
	}
}

/**
 * ofa_ientry_ope_template_get_interface_last_version:
 *
 * Returns: the last version number of this interface.
 */
guint
ofa_ientry_ope_template_get_interface_last_version( void )
{
	return( IENTRY_OPE_TEMPLATE_LAST_VERSION );
}

/**
 * ofa_ientry_ope_template_get_interface_version:
 * @instance: this #ofaIEntryOpeTemplate instance.
 *
 * Returns: the version number implemented by the object.
 *
 * Defaults to 1.
 */
guint
ofa_ientry_ope_template_get_interface_version( const ofaIEntryOpeTemplate *instance )
{
	static const gchar *thisfn = "ofa_ientry_ope_template_get_interface_version";

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	g_return_val_if_fail( instance && OFA_IS_IENTRY_OPE_TEMPLATE( instance ), 0 );

	if( OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance )->get_interface_version ){
		return( OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance )->get_interface_version( instance ));
	}

	g_info( "%s: ofaIEntryOpeTemplate instance %p does not provide 'get_interface_version()' method",
			thisfn, ( void * ) instance );
	return( 1 );
}

/**
 * ofa_ientry_ope_template_init:
 * @instance: the #ofaIEntryOpeTemplate instance.
 * @main_window: the #ofaMainWindow main window of the application.
 * @entry: the #GtkEntry entry to be set.
 *
 * Initialize the GtkEntry to setup an icon.
 * When this icon is pressed, an account selection dialog is triggered.
 */
void
ofa_ientry_ope_template_init( ofaIEntryOpeTemplate *instance, ofaMainWindow *main_window, GtkEntry *entry )
{
	sIEntryOpeTemplate *sdata;
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	g_return_if_fail( instance && OFA_IS_IENTRY_OPE_TEMPLATE( instance ));
	g_return_if_fail( main_window && OFA_IS_MAIN_WINDOW( main_window ));
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	gtk_widget_set_halign( GTK_WIDGET( entry ), GTK_ALIGN_START );
	gtk_entry_set_alignment( entry, 0 );
	gtk_entry_set_max_width_chars( entry, OTE_MNEMO_MAX_LENGTH );
	gtk_entry_set_max_length( entry, OTE_MNEMO_MAX_LENGTH );

	sdata = get_ientry_ope_template_data( instance, entry );
	sdata->instance = instance;
	sdata->main_window = main_window;

	image = gtk_image_new_from_resource( st_resource_ope_template );

	pixbuf = gtk_image_get_pixbuf( GTK_IMAGE( image ));
	gtk_entry_set_icon_from_pixbuf( entry, GTK_ENTRY_ICON_SECONDARY, pixbuf );

	g_signal_connect( entry, "icon-press", G_CALLBACK( on_icon_pressed ), instance );
}

static void
on_icon_pressed( GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, ofaIEntryOpeTemplate *instance )
{
	static const gchar *thisfn = "ofa_ientry_ope_template_on_icon_pressed";
	sIEntryOpeTemplate *sdata;
	gchar *initial_selection, *ope_template_id, *tmp;
	GtkWidget *toplevel;

	sdata = get_ientry_ope_template_data( instance, entry );

	if( OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance )->on_pre_select ){
		initial_selection = OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance )->on_pre_select( instance, entry, icon_pos );
	} else {
		g_info( "%s: ofaIEntryOpeTemplate instance %p does not provide 'on_icon_pressed()' method",
				thisfn, ( void * ) instance );
		initial_selection = g_strdup( gtk_entry_get_text( entry ));
	}

	toplevel = gtk_widget_get_toplevel( GTK_WIDGET( entry ));
	ope_template_id = ofa_ope_template_select_run(
			sdata->main_window, toplevel ? GTK_WINDOW( toplevel ) : NULL, initial_selection );

	if( ope_template_id ){
		if( OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance )->on_post_select ){
			tmp = OFA_IENTRY_OPE_TEMPLATE_GET_INTERFACE( instance )->on_post_select( instance, entry, icon_pos, ope_template_id );
			if( tmp ){
				g_free( ope_template_id );
				ope_template_id = tmp;
			}
		} else {
			g_info( "%s: ofaIEntryOpeTemplate instance %p does not provide 'on_post_select()' method",
					thisfn, ( void * ) instance );
		}
		gtk_entry_set_text( entry, ope_template_id );
	}

	g_free( ope_template_id );
	g_free( initial_selection );
}

static sIEntryOpeTemplate *
get_ientry_ope_template_data( const ofaIEntryOpeTemplate *instance, GtkEntry *entry )
{
	sIEntryOpeTemplate *sdata;

	sdata = ( sIEntryOpeTemplate * ) g_object_get_data( G_OBJECT( entry ), IENTRY_OPE_TEMPLATE_DATA );

	if( !sdata ){
		sdata = g_new0( sIEntryOpeTemplate, 1 );
		g_object_set_data( G_OBJECT( entry ), IENTRY_OPE_TEMPLATE_DATA, sdata );
		g_object_weak_ref( G_OBJECT( entry ), ( GWeakNotify ) on_entry_finalized, sdata );
	}

	return( sdata );
}

static void
on_entry_finalized( sIEntryOpeTemplate *sdata, GObject *finalized_entry )
{
	static const gchar *thisfn = "ofa_ientry_ope_template_on_entry_finalized";

	g_debug( "%s: sdata=%p, finalized_entry=%p", thisfn, ( void * ) sdata, ( void * ) finalized_entry );

	g_free( sdata );
}
