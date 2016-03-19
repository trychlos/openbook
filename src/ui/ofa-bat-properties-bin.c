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

#include <glib/gi18n.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-counter.h"
#include "api/ofa-hub.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-iconcil.h"

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
	GtkWidget    *bat_unused;
	GtkWidget    *bat_begin;
	GtkWidget    *bat_end;
	GtkWidget    *bat_rib;
	GtkWidget    *bat_currency;
	GtkWidget    *bat_solde_begin;
	GtkWidget    *bat_solde_end;
	GtkWidget    *bat_account;

	GtkTreeView  *tview;

	/* just to make the my_utils_init_..._ex macros happy
	 */
	ofoBat       *bat;
	ofaHub       *hub;
	gboolean      is_new;

	/* currency: taken from bat or line
	 */
	ofoCurrency  *currency;
};

/* columns in the lines treeview
 */
enum {
	COL_ID = 0,
	COL_DOPE,
	COL_DEFFECT,
	COL_REF,
	COL_LABEL,
	COL_AMOUNT,
	COL_CURRENCY,
	COL_ENTRY,
	COL_USER,
	COL_STAMP,
	N_COLUMNS
};

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-bat-properties-bin.ui";

static void  setup_bin( ofaBatPropertiesBin *self );
static void  setup_treeview( ofaBatPropertiesBin *self );
static void  display_bat_properties( ofaBatPropertiesBin *self, ofoBat *bat );
static void  display_bat_lines( ofaBatPropertiesBin *self, ofoBat *bat );
static void  display_line( ofaBatPropertiesBin *self, GtkTreeModel *tstore, ofoBatLine *line );

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
	GtkWidget *tview;
	GtkTreeModel *tmodel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	priv = ofa_bat_properties_bin_get_instance_private( self );

	tview = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "p3-treeview" );
	g_return_if_fail( tview && GTK_IS_TREE_VIEW( tview ));
	priv->tview = GTK_TREE_VIEW( tview );

	tmodel = GTK_TREE_MODEL( gtk_list_store_new(
			N_COLUMNS,
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* line_id, dope, deffect */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* ref, label, amount */
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 	/* currency, entry, user */
			G_TYPE_STRING, 								 	/* stamp */
			G_TYPE_OBJECT ));								/* the ofoBatLine object itself */
	gtk_tree_view_set_model( priv->tview, tmodel );
	g_object_unref( tmodel );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Line id." ),
			cell, "text", COL_ID,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ope." ),
			cell, "text", COL_DOPE,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Effect" ),
			cell, "text", COL_DEFFECT,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Ref." ),
			cell, "text", COL_REF,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Label" ),
			cell, "text", COL_LABEL,
			NULL );
	gtk_tree_view_column_set_resizable( column, TRUE );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Amount" ),
			cell, "text", COL_AMOUNT,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Cur." ),
			cell, "text", COL_CURRENCY,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment( cell, 1, 0.5 );
	column = gtk_tree_view_column_new_with_attributes(
			_( "Entry" ),
			cell, "text", COL_ENTRY,
			NULL );
	gtk_tree_view_column_set_alignment( column, 1.0 );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "User" ),
			cell, "text", COL_USER,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			_( "Timestamp" ),
			cell, "text", COL_STAMP,
			NULL );
	gtk_tree_view_append_column( priv->tview, column );

	select = gtk_tree_view_get_selection( priv->tview );
	gtk_tree_selection_set_mode( select, GTK_SELECTION_BROWSE );
}

static void
display_bat_properties( ofaBatPropertiesBin *self, ofoBat *bat )
{
	ofaBatPropertiesBinPrivate *priv;
	ofxCounter bat_id;
	const gchar *cstr;
	gchar *str;
	gint total, used;
	ofoDossier *dossier;

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

	dossier = ofa_hub_get_dossier( priv->hub );

	my_utils_container_notes_setup_full(
				GTK_CONTAINER( self ),
				"pn-notes", ofo_bat_get_notes( bat ), ofo_dossier_is_current( dossier ));
	my_utils_container_updstamp_init( self, bat );
}

static void
display_bat_lines( ofaBatPropertiesBin *self, ofoBat *bat )
{
	ofaBatPropertiesBinPrivate *priv;
	GList *dataset, *it;
	GtkTreeModel *tmodel;

	priv = ofa_bat_properties_bin_get_instance_private( self );

	tmodel = gtk_tree_view_get_model( priv->tview );
	gtk_list_store_clear( GTK_LIST_STORE( tmodel ));

	dataset = ofo_bat_line_get_dataset( priv->hub, ofo_bat_get_id( bat ));
	for( it=dataset ; it ; it=it->next ){
		display_line( self, tmodel, OFO_BAT_LINE( it->data ));
	}
	ofo_bat_line_free_dataset( dataset );
}

static void
display_line( ofaBatPropertiesBin *self, GtkTreeModel *tstore, ofoBatLine *line )
{
	ofaBatPropertiesBinPrivate *priv;
	gchar *sid, *sdope, *sdeffect, *samount;
	ofxCounter bat_id;
	GtkTreeIter iter;
	ofoConcil *concil;
	const gchar *cuser, *cur_code;
	gchar *stamp;
	GString *snumbers;
	GList *ids, *it;
	ofsConcilId *scid;

	priv = ofa_bat_properties_bin_get_instance_private( self );

	bat_id = ofo_bat_line_get_line_id( line );
	sid = g_strdup_printf( "%lu", bat_id );
	sdope = my_date_to_str( ofo_bat_line_get_dope( line ), ofa_prefs_date_display());
	sdeffect = my_date_to_str( ofo_bat_line_get_deffect( line ), ofa_prefs_date_display());

	if( !priv->currency ){
		cur_code = ofo_bat_line_get_currency( line );
		priv->currency = cur_code ? ofo_currency_get_by_code( priv->hub, cur_code ) : NULL;
		g_return_if_fail( !priv->currency || OFO_IS_CURRENCY( priv->currency ));
	}

	samount = ofa_amount_to_str( ofo_bat_line_get_amount( line ), priv->currency );
	concil = ofa_iconcil_get_concil( OFA_ICONCIL( line ));
	snumbers = g_string_new( "" );
	if( concil ){
		cuser = ofo_concil_get_user( concil );
		stamp = my_utils_stamp_to_str( ofo_concil_get_stamp( concil ), MY_STAMP_YYMDHMS );
		ids = ofo_concil_get_ids( concil );
		for( it=ids ; it ; it=it->next ){
			scid = ( ofsConcilId * ) it->data;
			if( !g_utf8_collate( scid->type, CONCIL_TYPE_ENTRY )){
				if( snumbers->len ){
					snumbers = g_string_append( snumbers, "," );
				}
				g_string_append_printf( snumbers, "%ld", scid->other_id );
			}
		}
	} else {
		cuser = "";
		stamp = g_strdup( "" );
	}

	gtk_list_store_insert_with_values(
			GTK_LIST_STORE( tstore ),
			&iter,
			-1,
			COL_ID,       sid,
			COL_DOPE,     sdope,
			COL_DEFFECT,  sdeffect,
			COL_REF,      ofo_bat_line_get_ref( line ),
			COL_LABEL,    ofo_bat_line_get_label( line ),
			COL_AMOUNT,   samount,
			COL_CURRENCY, priv->currency ? ofo_currency_get_code( priv->currency ) : "",
			COL_ENTRY,    snumbers->str,
			COL_USER,     cuser,
			COL_STAMP,    stamp,
			-1 );

	g_string_free( snumbers, TRUE );
	g_free( stamp );
	g_free( samount );
	g_free( sdeffect );
	g_free( sdope );
	g_free( sid );
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
	display_bat_lines( bin, bat );
}
