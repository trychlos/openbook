/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-settings.h"

#include "ui/my-editable-date.h"
#include "ui/ofa-date-filter-bin.h"

/* private instance data
 */
struct _ofaDateFilterBinPrivate {
	gboolean dispose_has_run;

	gchar   *pref_name;					/* settings key name */

	GDate    from_date;
	GDate    to_date;
};

/* signals defined here
 */
enum {
	CHANGED = 0,
	N_SIGNALS
};

static guint st_signals[ N_SIGNALS ]    = { 0 };

static const gchar *st_ui_xml           = PKGUIDIR "/ofa-date-filter-bin.ui";
static const gchar *st_ui_id            = "DateFilterBin";

G_DEFINE_TYPE( ofaDateFilterBin, ofa_date_filter_bin, GTK_TYPE_BIN )

static void     load_dialog( ofaDateFilterBin *bin );
static void     setup_dialog( ofaDateFilterBin *bin );
static gboolean on_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaDateFilterBin *bin );
static gboolean on_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaDateFilterBin *bin );
static gboolean on_date_focus_out( ofaDateFilterBin *bin, gint who, GtkEntry *entry, GDate *date );
static void     get_settings( ofaDateFilterBin *bin );
static void     set_settings( ofaDateFilterBin *bin );

static void
check_integrity_bin_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_date_filter_bin_finalize";
	ofaDateFilterBinPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DATE_FILTER_BIN( instance ));

	/* free data members here */
	priv = OFA_DATE_FILTER_BIN( instance )->priv;

	g_free( priv->pref_name );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_date_filter_bin_parent_class )->finalize( instance );
}

static void
check_integrity_bin_dispose( GObject *instance )
{
	ofaDateFilterBinPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DATE_FILTER_BIN( instance ));

	priv = OFA_DATE_FILTER_BIN( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_date_filter_bin_parent_class )->dispose( instance );
}

static void
ofa_date_filter_bin_init( ofaDateFilterBin *self )
{
	static const gchar *thisfn = "ofa_date_filter_bin_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DATE_FILTER_BIN( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(
						self, OFA_TYPE_DATE_FILTER_BIN, ofaDateFilterBinPrivate );
}

static void
ofa_date_filter_bin_class_init( ofaDateFilterBinClass *klass )
{
	static const gchar *thisfn = "ofa_date_filter_bin_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = check_integrity_bin_dispose;
	G_OBJECT_CLASS( klass )->finalize = check_integrity_bin_finalize;

	g_type_class_add_private( klass, sizeof( ofaDateFilterBinPrivate ));

	/**
	 * ofaDateFilterBin::changed:
	 *
	 * This signal is sent when one of the from or to dates is changed.
	 *
	 * Handler is of type:
	 * void ( *handler )( ofaDateFilterBin *bin,
	 * 						gint            who,
	 * 						const GDate    *date,
	 * 						gpointer        user_data );
	 */
	st_signals[ CHANGED ] = g_signal_new_class_handler(
				"changed",
				OFA_TYPE_DATE_FILTER_BIN,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_INT, G_TYPE_POINTER );
}

/**
 * ofa_date_filter_bin_new:
 */
ofaDateFilterBin *
ofa_date_filter_bin_new( const gchar *pref_name )
{
	ofaDateFilterBin *self;

	self = g_object_new( OFA_TYPE_DATE_FILTER_BIN, NULL );

	self->priv->pref_name = g_strdup( pref_name );
	get_settings( self );
	load_dialog( self );
	setup_dialog( self );

	return( self );
}

static void
load_dialog( ofaDateFilterBin *bin )
{
	GtkWidget *window;
	GtkWidget *top_widget;

	window = my_utils_builder_load_from_path( st_ui_xml, st_ui_id );
	g_return_if_fail( window && GTK_IS_WINDOW( window ));

	top_widget = my_utils_container_get_child_by_name( GTK_CONTAINER( window ), "top" );
	g_return_if_fail( top_widget && GTK_IS_CONTAINER( top_widget ));

	gtk_widget_reparent( top_widget, GTK_WIDGET( bin ));
}

static void
setup_dialog( ofaDateFilterBin *bin )
{
	ofaDateFilterBinPrivate *priv;
	GtkWidget *entry, *label;

	priv = bin->priv;

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "from-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "from-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), MY_DATE_DMYY );
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, MY_DATE_DMMM );
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );

	g_signal_connect( entry, "focus-out-event", G_CALLBACK( on_from_focus_out ), bin );

	if( my_date_is_valid( &priv->from_date )){
		my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->from_date );
	}

	entry = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "to-entry" );
	g_return_if_fail( entry && GTK_IS_ENTRY( entry ));

	label = my_utils_container_get_child_by_name( GTK_CONTAINER( bin ), "to-label" );
	g_return_if_fail( label && GTK_IS_LABEL( label ));

	my_editable_date_init( GTK_EDITABLE( entry ));
	my_editable_date_set_format( GTK_EDITABLE( entry ), MY_DATE_DMYY );
	my_editable_date_set_label( GTK_EDITABLE( entry ), label, MY_DATE_DMMM );
	my_editable_date_set_mandatory( GTK_EDITABLE( entry ), FALSE );

	g_signal_connect( entry, "focus-out-event", G_CALLBACK( on_to_focus_out ), bin );

	if( my_date_is_valid( &priv->to_date )){
		my_editable_date_set_date( GTK_EDITABLE( entry ), &priv->to_date );
	}
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_from_focus_out( GtkEntry *entry, GdkEvent *event, ofaDateFilterBin *bin )
{
	return( on_date_focus_out( bin, OFA_DATE_FILTER_FROM, entry, &bin->priv->from_date ));
}

/*
 * Returns :
 * TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
on_to_focus_out( GtkEntry *entry, GdkEvent *event, ofaDateFilterBin *bin )
{
	return( on_date_focus_out( bin, OFA_DATE_FILTER_TO, entry, &bin->priv->to_date ));
}

static gboolean
on_date_focus_out( ofaDateFilterBin *bin, gint who, GtkEntry *entry, GDate *date )
{
	my_date_set_from_date( date, my_editable_date_get_date( GTK_EDITABLE( entry ), NULL ));

	if( my_editable_date_is_empty( GTK_EDITABLE( entry )) || my_date_is_valid( date )){
		set_settings( bin );
	}

	g_signal_emit_by_name( bin, "changed", who, date );

	return( FALSE );
}

/**
 * ofa_date_filter_bin_get_from:
 */
const GDate *
ofa_date_filter_bin_get_from( const ofaDateFilterBin *bin )
{
	ofaDateFilterBinPrivate *priv;
	const GDate *date;

	g_return_val_if_fail( bin && OFA_IS_DATE_FILTER_BIN( bin ), NULL );

	priv = bin->priv;
	date = NULL;

	if( !priv->dispose_has_run ){
		date = ( const GDate * ) &priv->from_date;
	}

	return( date );
}

/**
 * ofa_date_filter_bin_set_from:
 */
void
ofa_date_filter_bin_set_from( ofaDateFilterBin *bin, const GDate *from)
{
	ofaDateFilterBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DATE_FILTER_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( &priv->from_date, from );
	}
}

/**
 * ofa_date_filter_bin_get_to:
 */
const GDate *
ofa_date_filter_bin_get_to( const ofaDateFilterBin *bin )
{
	ofaDateFilterBinPrivate *priv;
	const GDate *date;

	g_return_val_if_fail( bin && OFA_IS_DATE_FILTER_BIN( bin ), NULL );

	priv = bin->priv;
	date = NULL;

	if( !priv->dispose_has_run ){
		date = ( const GDate * ) &priv->to_date;
	}

	return( date );
}

/**
 * ofa_date_filter_bin_set_to:
 */
void
ofa_date_filter_bin_set_to( ofaDateFilterBin *bin, const GDate *to)
{
	ofaDateFilterBinPrivate *priv;

	g_return_if_fail( bin && OFA_IS_DATE_FILTER_BIN( bin ));

	priv = bin->priv;

	if( !priv->dispose_has_run ){
		my_date_set_from_date( &priv->to_date, to );
	}
}

/*
 * settings are: from;to; as SQL date
 */
static void
get_settings( ofaDateFilterBin *bin )
{
	ofaDateFilterBinPrivate *priv;
	GList *slist, *it;
	const gchar *cstr;

	priv = bin->priv;

	slist = ofa_settings_get_string_list( priv->pref_name );
	if( slist ){
		it = slist ? slist : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			my_date_set_from_sql( &priv->from_date, cstr );
		}

		it = it ? it->next : NULL;
		cstr = it ? it->data : NULL;
		if( my_strlen( cstr )){
			my_date_set_from_sql( &priv->to_date, cstr );
		}

		ofa_settings_free_string_list( slist );
	}
}

static void
set_settings( ofaDateFilterBin *bin )
{
	ofaDateFilterBinPrivate *priv;
	gchar *sfrom, *sto, *str;

	priv = bin->priv;

	sfrom = my_date_to_str( &priv->from_date, MY_DATE_SQL );
	sto = my_date_to_str( &priv->to_date, MY_DATE_SQL );

	str = g_strdup_printf( "%s;%s;", sfrom, sto );

	ofa_settings_set_string( priv->pref_name, str );

	g_free( str );
	g_free( sfrom );
	g_free( sto );
}
