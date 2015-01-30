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

#include "api/my-date.h"
#include "api/my-double.h"
#include "api/my-utils.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-bat-properties-bin.h"

/* private instance data
 */
struct _ofaBatPropertiesBinPrivate {
	gboolean      dispose_has_run;

	/* UI
	 */
	GtkWidget    *bat_id;
	GtkWidget    *bat_format;
	GtkWidget    *bat_count;
	GtkWidget    *bat_begin;
	GtkWidget    *bat_end;
	GtkWidget    *bat_rib;
	GtkWidget    *bat_currency;
	GtkWidget    *bat_solde;

	/* just to make the my_utils_init_..._ex macros happy
	 */
	ofoBat       *bat;
	gboolean      is_new;
};

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-bat-properties-bin.ui";
static const gchar *st_ui_id            = "BatPropertiesBinWindow";

G_DEFINE_TYPE( ofaBatPropertiesBin, ofa_bat_properties_bin, GTK_TYPE_BIN )

static void          load_dialog( ofaBatPropertiesBin *bin );
static void          display_bat_properties( ofaBatPropertiesBin *bin, ofoBat *bat, ofoDossier *dossier, gboolean editable );

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

	priv = ( OFA_BAT_PROPERTIES_BIN( instance ))->priv;

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

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BAT_PROPERTIES_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_BAT_PROPERTIES_BIN, ofaBatPropertiesBinPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_bat_properties_bin_class_init( ofaBatPropertiesBinClass *klass )
{
	static const gchar *thisfn = "ofa_bat_properties_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_properties_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_properties_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaBatPropertiesBinPrivate ));
}

/**
 * ofa_bat_properties_bin_new:
 */
ofaBatPropertiesBin *
ofa_bat_properties_bin_new( void )
{
	ofaBatPropertiesBin *bin;

	bin = g_object_new( OFA_TYPE_BAT_PROPERTIES_BIN, NULL );

	load_dialog( bin );

	return( bin );
}

static void
load_dialog( ofaBatPropertiesBin *bin )
{
	ofaBatPropertiesBinPrivate *priv;
	GtkWidget *window;
	GtkWidget *top_widget;

	priv = bin->priv;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top-box" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));

	/* identify the widgets for the properties */
	priv->bat_id = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-id" );
	g_return_if_fail( priv->bat_id && GTK_IS_ENTRY( priv->bat_id ));

	priv->bat_format = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-format" );
	g_return_if_fail( priv->bat_format && GTK_IS_ENTRY( priv->bat_format ));

	priv->bat_count = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-count" );
	g_return_if_fail( priv->bat_count && GTK_IS_ENTRY( priv->bat_count ));

	priv->bat_begin = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-begin" );
	g_return_if_fail( priv->bat_begin && GTK_IS_ENTRY( priv->bat_begin ));

	priv->bat_end = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-end" );
	g_return_if_fail( priv->bat_end && GTK_IS_ENTRY( priv->bat_end ));

	priv->bat_rib = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-rib" );
	g_return_if_fail( priv->bat_rib && GTK_IS_ENTRY( priv->bat_rib ));

	priv->bat_currency = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-currency" );
	g_return_if_fail( priv->bat_currency && GTK_IS_ENTRY( priv->bat_currency ));

	priv->bat_solde = my_utils_container_get_child_by_name( GTK_CONTAINER( top_widget ), "p1-solde" );
	g_return_if_fail( priv->bat_solde && GTK_IS_ENTRY( priv->bat_solde ));
}

static void
display_bat_properties( ofaBatPropertiesBin *bin, ofoBat *bat, ofoDossier *dossier, gboolean editable )
{
	ofaBatPropertiesBinPrivate *priv;
	const gchar *cstr;
	gchar *str;

	priv = bin->priv;

	str = g_strdup_printf( "%lu", ofo_bat_get_id( bat ));
	gtk_entry_set_text( GTK_ENTRY( priv->bat_id ), str );
	g_free( str );

	cstr = ofo_bat_get_format( bat );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->bat_format ), cstr );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_format ), "" );
	}

	str = g_strdup_printf( "%u", ofo_bat_get_count( bat, dossier ));
	gtk_entry_set_text( GTK_ENTRY( priv->bat_count ), str );
	g_free( str );

	str = my_date_to_str( ofo_bat_get_begin( bat ), MY_DATE_DMYY );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_begin ), str );
	g_free( str );

	str = my_date_to_str( ofo_bat_get_end( bat ), MY_DATE_DMYY );
	gtk_entry_set_text( GTK_ENTRY( priv->bat_end ), str );
	g_free( str );

	cstr = ofo_bat_get_rib( bat );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->bat_rib ), cstr );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_rib ), "" );
	}

	cstr = ofo_bat_get_currency( bat );
	if( cstr ){
		gtk_entry_set_text( GTK_ENTRY( priv->bat_currency ), cstr );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_currency ), "" );
	}

	if( ofo_bat_get_solde_set( bat )){
		str = my_double_to_str( ofo_bat_get_solde( bat ));
		gtk_entry_set_text( GTK_ENTRY( priv->bat_solde ), str );
		g_free( str );
	} else {
		gtk_entry_set_text( GTK_ENTRY( priv->bat_solde ), "" );
	}

	priv->bat = bat;
	my_utils_init_notes_ex( bin, bat, editable );
	my_utils_init_upd_user_stamp_ex( bin, bat );
}

/**
 * ofa_bat_properties_bin_set_bat:
 */
void
ofa_bat_properties_bin_set_bat( ofaBatPropertiesBin *bin, ofoBat *bat, ofoDossier *dossier, gboolean editable )
{
	static const gchar *thisfn = "ofa_bat_properties_bin_set_bat";
	ofaBatPropertiesBinPrivate *priv;

	g_debug( "%s: bin=%p, bat=%p, dossier=%p, editable=%s",
			thisfn, ( void * ) bin, ( void * ) bat, ( void * ) dossier, editable ? "True":"False" );

	g_return_if_fail( OFA_IS_BAT_PROPERTIES_BIN( bin ));
	g_return_if_fail( OFO_IS_BAT( bat ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){

		display_bat_properties( bin, bat, dossier, editable );
	}
}
