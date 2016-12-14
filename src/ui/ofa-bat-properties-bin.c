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

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-counter.h"
#include "api/ofa-hub.h"
#include "api/ofa-iactionable.h"
#include "api/ofa-icontext.h"
#include "api/ofa-itvcolumnable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-iconcil.h"

#include "ui/ofa-batline-treeview.h"
#include "ui/ofa-bat-properties-bin.h"

/* private instance data
 */
typedef struct {
	gboolean            dispose_has_run;

	/* UI
	 */
	GtkWidget          *bat_id;
	GtkWidget          *bat_format;
	GtkWidget          *bat_count;
	GtkWidget          *bat_unused;
	GtkWidget          *bat_begin;
	GtkWidget          *bat_end;
	GtkWidget          *bat_rib;
	GtkWidget          *bat_currency;
	GtkWidget          *bat_solde_begin;
	GtkWidget          *bat_solde_end;
	GtkWidget          *bat_account;

	ofaBatlineTreeview *tview;

	/* just to make the my_utils_init_..._ex macros happy
	 */
	ofoBat             *bat;
	ofaHub             *hub;
	gboolean            is_new;

	/* currency is taken from BAT
	 */
	ofoCurrency        *currency;
}
	ofaBatPropertiesBinPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-bat-properties-bin.ui";

static void  setup_bin( ofaBatPropertiesBin *self );
static void  setup_treeview( ofaBatPropertiesBin *self );
static void  display_bat_properties( ofaBatPropertiesBin *self, ofoBat *bat );

G_DEFINE_TYPE_EXTENDED( ofaBatPropertiesBin, ofa_bat_properties_bin, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaBatPropertiesBin ))

static void
bat_properties_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_bat_properties_bin_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BAT_PROPERTIES_BIN( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_bin_parent_class )->finalize( instance );
}

static void
bat_properties_bin_dispose( GObject *instance )
{
	ofaBatPropertiesBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BAT_PROPERTIES_BIN( instance ));

	priv = ofa_bat_properties_bin_get_instance_private( OFA_BAT_PROPERTIES_BIN( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_bat_properties_bin_parent_class )->dispose( instance );
}

static void
ofa_bat_properties_bin_init( ofaBatPropertiesBin *self )
{
	static const gchar *thisfn = "ofa_bat_properties_bin_init";
	ofaBatPropertiesBinPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_PROPERTIES_BIN( self ));

	priv = ofa_bat_properties_bin_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_bat_properties_bin_class_init( ofaBatPropertiesBinClass *klass )
{
	static const gchar *thisfn = "ofa_bat_properties_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_properties_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_properties_bin_finalize;
}

/**
 * ofa_bat_properties_bin_new:
 */
ofaBatPropertiesBin *
ofa_bat_properties_bin_new( void )
{
	ofaBatPropertiesBin *self;

	self = g_object_new( OFA_TYPE_BAT_PROPERTIES_BIN, NULL );

	setup_bin( self );
	setup_treeview( self );

	return( self );
}

static void
setup_bin( ofaBatPropertiesBin *self )
{
	ofaBatPropertiesBinPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	priv = ofa_bat_properties_bin_get_instance_private( self );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "bpb-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( self ), GTK_WINDOW( toplevel ), "top" );

	/* identify the widgets for the properties */
	priv->bat_id = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-id" );
	g_return_if_fail( priv->bat_id && GTK_IS_ENTRY( priv->bat_id ));

	priv->bat_format = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-format" );
	g_return_if_fail( priv->bat_format && GTK_IS_ENTRY( priv->bat_format ));

	priv->bat_count = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-count" );
	g_return_if_fail( priv->bat_count && GTK_IS_ENTRY( priv->bat_count ));

	priv->bat_unused = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-unused" );
	g_return_if_fail( priv->bat_unused && GTK_IS_ENTRY( priv->bat_unused ));

	priv->bat_begin = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-begin" );
	g_return_if_fail( priv->bat_begin && GTK_IS_ENTRY( priv->bat_begin ));

	priv->bat_end = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-end" );
	g_return_if_fail( priv->bat_end && GTK_IS_ENTRY( priv->bat_end ));

	priv->bat_rib = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-rib" );
	g_return_if_fail( priv->bat_rib && GTK_IS_ENTRY( priv->bat_rib ));

	priv->bat_currency = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-currency" );
	g_return_if_fail( priv->bat_currency && GTK_IS_ENTRY( priv->bat_currency ));

	priv->bat_solde_begin = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-solde-begin" );
	g_return_if_fail( priv->bat_solde_begin && GTK_IS_ENTRY( priv->bat_solde_begin ));

	priv->bat_solde_end = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-solde-end" );
	g_return_if_fail( priv->bat_solde_end && GTK_IS_ENTRY( priv->bat_solde_end ));

	priv->bat_account = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p1-account" );
	g_return_if_fail( priv->bat_account && GTK_IS_ENTRY( priv->bat_account ));

	my_utils_container_set_editable( GTK_CONTAINER( self ), FALSE );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
}

static void
setup_treeview( ofaBatPropertiesBin *self )
{
	ofaBatPropertiesBinPrivate *priv;
	GtkWidget *box;

	priv = ofa_bat_properties_bin_get_instance_private( self );

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-boxview" );
	g_return_if_fail( box && GTK_IS_CONTAINER( box ));

	priv->tview = ofa_batline_treeview_new( priv->hub );
	gtk_container_add( GTK_CONTAINER( box ), GTK_WIDGET( priv->tview ));
}

/**
 * ofa_bat_properties_bin_set_settings_key:
 * @view: this #ofaBatlineTreeview instance.
 * @key: [allow-none]: the prefix of the settings key.
 *
 * Setup the setting key, or reset it to its default if %NULL.
 */
void
ofa_bat_properties_bin_set_settings_key( ofaBatPropertiesBin *bin, const gchar *key )
{
	ofaBatPropertiesBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_BAT_PROPERTIES_BIN( bin ));

	priv = ofa_bat_properties_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	/* we do not manage any settings here, so directly pass it to
	 * embedded classes */
	ofa_batline_treeview_set_settings_key( priv->tview, key );
}

static void
display_bat_properties( ofaBatPropertiesBin *self, ofoBat *bat )
{
	ofaBatPropertiesBinPrivate *priv;
	ofxCounter bat_id;
	const gchar *cstr;
	gchar *str;
	gint total, used;

	priv = ofa_bat_properties_bin_get_instance_private( self );

	bat_id = ofo_bat_get_id( bat );
	str = ofa_counter_to_str( bat_id );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_id ), str );
	g_free( str );

	cstr = ofo_bat_get_format( bat );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->bat_format ), cstr );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_format ), "" );
	}

	total = ofo_bat_get_lines_count( bat );
	str = g_strdup_printf( "%u", total );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_count ), str );
	g_free( str );

	used = ofo_bat_get_used_count( bat );
	str = g_strdup_printf( "%u", total-used );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_unused ), str );
	g_free( str );

	str = my_date_to_str( ofo_bat_get_begin_date( bat ), ofa_prefs_date_display());
	gtk_entry_set_text( GTK_ENTRY( priv->bat_begin ), str );
	g_free( str );

	str = my_date_to_str( ofo_bat_get_end_date( bat ), ofa_prefs_date_display());
	gtk_entry_set_text( GTK_ENTRY( priv->bat_end ), str );
	g_free( str );

	cstr = ofo_bat_get_rib( bat );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->bat_rib ), cstr );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_rib ), "" );
	}

	cstr = ofo_bat_get_currency( bat );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_currency ), cstr ? cstr : "" );
	priv->currency = cstr ? ofo_currency_get_by_code( priv->hub, cstr ) : NULL;
	g_return_if_fail( !priv->currency || OFO_IS_CURRENCY( priv->currency ));

	if( ofo_bat_get_begin_solde_set( bat )){
		str = ofa_amount_to_str( ofo_bat_get_begin_solde( bat ), priv->currency );
		gtk_entry_set_text( GTK_ENTRY( priv->bat_solde_begin ), str );
		g_free( str );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_solde_begin ), "" );
	}

	if( ofo_bat_get_end_solde_set( bat )){
		str = ofa_amount_to_str( ofo_bat_get_end_solde( bat ), priv->currency );
		gtk_entry_set_text( GTK_ENTRY( priv->bat_solde_end ), str );
		g_free( str );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_solde_end ), "" );
	}

	cstr = ofo_bat_get_account( bat );
	if( my_strlen( cstr )){
		gtk_entry_set_text( GTK_ENTRY( priv->bat_account ), cstr );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_account ), "" );
	}

	my_utils_container_notes_setup_full(
				GTK_CONTAINER( self ),
				"pn-notes", ofo_bat_get_notes( bat ), ofa_hub_dossier_is_writable( priv->hub ));
	my_utils_container_updstamp_init( self, bat );
}

/**
 * ofa_bat_properties_bin_set_bat:
 */
void
ofa_bat_properties_bin_set_bat( ofaBatPropertiesBin *bin, ofoBat *bat )
{
	static const gchar *thisfn = "ofa_bat_properties_bin_set_bat";
	ofaBatPropertiesBinPrivate *priv;

	g_debug( "%s: bin=%p, bat=%p", thisfn, ( void * ) bin, ( void * ) bat );

	g_return_if_fail( bin && OFA_IS_BAT_PROPERTIES_BIN( bin ));
	g_return_if_fail( bat && OFO_IS_BAT( bat ));

	priv = ofa_bat_properties_bin_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	priv->bat = bat;
	priv->hub = ofo_base_get_hub( OFO_BASE( bat ));
	priv->currency = NULL;

	display_bat_properties( bin, bat );
	ofa_batline_treeview_set_bat( priv->tview, bat );
}

/**
 * ofa_bat_properties_bin_get_batline_treeview:
 * @view: this #ofaBatlineTreeview instance.
 *
 * Returns: the #ofaBatlineTreeview embedded widget.
 *
 * The returned reference is owned by the @view, and should not be
 * unreffed by the caller.
 */
ofaBatlineTreeview *
ofa_bat_properties_bin_get_batline_treeview( ofaBatPropertiesBin *bin )
{
	ofaBatPropertiesBinPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_BAT_PROPERTIES_BIN( bin ), NULL );

	priv = ofa_bat_properties_bin_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->tview );
}
