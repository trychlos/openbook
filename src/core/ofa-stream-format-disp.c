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
#include "my/my-decimal-combo.h"
#include "my/my-field-combo.h"
#include "my/my-utils.h"

#include "ofa-stream-format-disp.h"

/* private instance data
 */
typedef struct {
	gboolean         dispose_has_run;

	/* initialization data
	 */
	ofaStreamFormat *settings;

	/* UI
	 */
	GtkWidget    *charmap_label;
	GtkWidget    *date_label;
	GtkWidget    *thousand_label;
	GtkWidget    *decimal_label;
	GtkWidget    *field_label;
	GtkWidget    *strdelim_label;
	GtkWidget    *headers_label;

	GtkSizeGroup *group0;

	GtkWidget    *name_data;
	GtkWidget    *mode_data;
	GtkWidget    *charmap_data;
	GtkWidget    *date_data;
	GtkWidget    *thousand_data;
	GtkWidget    *decimal_data;
	GtkWidget    *field_data;
	GtkWidget    *strdelim_data;
	GtkWidget    *headers_data;
}
	ofaStreamFormatDispPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/core/ofa-stream-format-disp.ui";

static void setup_bin( ofaStreamFormatDisp *bin );
static void setup_labels( ofaStreamFormatDisp *bin );
static void setup_format( ofaStreamFormatDisp *bin );

G_DEFINE_TYPE_EXTENDED( ofaStreamFormatDisp, ofa_stream_format_disp, GTK_TYPE_BIN, 0,
		G_ADD_PRIVATE( ofaStreamFormatDisp ))

static void
stream_format_disp_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_stream_format_disp_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT_DISP( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_stream_format_disp_parent_class )->finalize( instance );
}

static void
stream_format_disp_dispose( GObject *instance )
{
	ofaStreamFormatDispPrivate *priv;

	g_return_if_fail( instance && OFA_IS_STREAM_FORMAT_DISP( instance ));

	priv = ofa_stream_format_disp_get_instance_private( OFA_STREAM_FORMAT_DISP( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->group0 );
		g_clear_object( &priv->settings );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_stream_format_disp_parent_class )->dispose( instance );
}

static void
ofa_stream_format_disp_init( ofaStreamFormatDisp *self )
{
	static const gchar *thisfn = "ofa_stream_format_disp_init";
	ofaStreamFormatDispPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_STREAM_FORMAT_DISP( self ));

	priv = ofa_stream_format_disp_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_stream_format_disp_class_init( ofaStreamFormatDispClass *klass )
{
	static const gchar *thisfn = "ofa_stream_format_disp_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = stream_format_disp_dispose;
	G_OBJECT_CLASS( klass )->finalize = stream_format_disp_finalize;
}

/**
 * ofa_stream_format_disp_new:
 *
 * Returns: a new #ofaStreamFormatDisp instance.
 */
ofaStreamFormatDisp *
ofa_stream_format_disp_new( void )
{
	ofaStreamFormatDisp *self;

	self = g_object_new( OFA_TYPE_STREAM_FORMAT_DISP, NULL );

	setup_bin( self );
	setup_labels( self );

	return( self );
}

/*
 */
static void
setup_bin( ofaStreamFormatDisp *bin )
{
	ofaStreamFormatDispPrivate *priv;
	GtkBuilder *builder;
	GObject *object;
	GtkWidget *toplevel;

	priv = ofa_stream_format_disp_get_instance_private( bin );

	builder = gtk_builder_new_from_resource( st_resource_ui );

	object = gtk_builder_get_object( builder, "sfd-col0-hsize" );
	g_return_if_fail( object && GTK_IS_SIZE_GROUP( object ));
	priv->group0 = GTK_SIZE_GROUP( g_object_ref( object ));

	object = gtk_builder_get_object( builder, "top-window" );
	g_return_if_fail( object && GTK_IS_WINDOW( object ));
	toplevel = GTK_WIDGET( g_object_ref( object ));

	my_utils_container_attach_from_window( GTK_CONTAINER( bin ), GTK_WINDOW( toplevel ), "top-grid" );

	gtk_widget_destroy( toplevel );
	g_object_unref( builder );
	gtk_widget_show_all( GTK_WIDGET( bin ));
}

static void
setup_labels( ofaStreamFormatDisp *self )
{
	ofaStreamFormatDispPrivate *priv;

	priv = ofa_stream_format_disp_get_instance_private( self );

	priv->name_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "name-data" );
	g_return_if_fail( priv->name_data && GTK_IS_LABEL( priv->name_data ));

	priv->mode_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "mode-data" );
	g_return_if_fail( priv->mode_data && GTK_IS_LABEL( priv->mode_data ));

	priv->charmap_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "charmap-label" );
	g_return_if_fail( priv->charmap_label && GTK_IS_LABEL( priv->charmap_label ));

	priv->charmap_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "charmap-data" );
	g_return_if_fail( priv->charmap_data && GTK_IS_LABEL( priv->charmap_data ));

	priv->date_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-label" );
	g_return_if_fail( priv->date_label && GTK_IS_LABEL( priv->date_label ));

	priv->date_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "date-data" );
	g_return_if_fail( priv->date_data && GTK_IS_LABEL( priv->date_data ));

	priv->thousand_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "thousand-label" );
	g_return_if_fail( priv->thousand_label && GTK_IS_LABEL( priv->thousand_label ));

	priv->thousand_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "thousand-data" );
	g_return_if_fail( priv->thousand_data && GTK_IS_LABEL( priv->thousand_data ));

	priv->decimal_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "decimal-label" );
	g_return_if_fail( priv->decimal_label && GTK_IS_LABEL( priv->decimal_label ));

	priv->decimal_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "decimal-data" );
	g_return_if_fail( priv->decimal_data && GTK_IS_LABEL( priv->decimal_data ));

	priv->field_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "field-label" );
	g_return_if_fail( priv->field_label && GTK_IS_LABEL( priv->field_label ));

	priv->field_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "field-data" );
	g_return_if_fail( priv->field_data && GTK_IS_LABEL( priv->field_data ));

	priv->strdelim_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "str-label" );
	g_return_if_fail( priv->strdelim_label && GTK_IS_LABEL( priv->strdelim_label ));

	priv->strdelim_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "str-data" );
	g_return_if_fail( priv->strdelim_data && GTK_IS_LABEL( priv->strdelim_data ));

	priv->headers_label = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "headers-label" );
	g_return_if_fail( priv->headers_label && GTK_IS_LABEL( priv->headers_label ));

	priv->headers_data = my_utils_container_get_child_by_name( GTK_CONTAINER( self ), "headers-data" );
	g_return_if_fail( priv->headers_data && GTK_IS_LABEL( priv->headers_data ));
}

/**
 * ofa_stream_format_disp_get_size_group:
 * @bin: this #ofaStreamFormatDisp instance.
 * @column: the desired column number.
 *
 * Returns: the #GtkSizeGroup which managed the @column.
 */
GtkSizeGroup *
ofa_stream_format_disp_get_size_group( const ofaStreamFormatDisp *bin, guint column )
{
	static const gchar *thisfn = "ofa_stream_format_disp_get_size_group";
	ofaStreamFormatDispPrivate *priv;

	g_return_val_if_fail( bin && OFA_IS_STREAM_FORMAT_DISP( bin ), NULL );

	priv = ofa_stream_format_disp_get_instance_private( bin );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	if( column == 0 ){
		return( priv->group0 );
	}

	g_warning( "%s: unkknown column=%d", thisfn, column );
	return( NULL );
}

/**
 * ofa_stream_format_disp_set_format:
 * @bin: this #ofaStreamFormatDisp instance.
 * @format: the new #ofaStreamFormat object to be considered.
 *
 * Release the reference previously taken on the initial object, and
 * then take a new reference on @format.
 */
void
ofa_stream_format_disp_set_format( ofaStreamFormatDisp *bin, ofaStreamFormat *format )
{
	ofaStreamFormatDispPrivate *priv;

	g_return_if_fail( bin && OFA_IS_STREAM_FORMAT_DISP( bin ));
	g_return_if_fail( format && OFA_IS_STREAM_FORMAT( format ));

	priv = ofa_stream_format_disp_get_instance_private( bin );

	g_return_if_fail( !priv->dispose_has_run );

	g_clear_object( &priv->settings );
	priv->settings = g_object_ref( format );

	setup_format( bin );
}

static void
setup_format( ofaStreamFormatDisp *self )
{
	ofaStreamFormatDispPrivate *priv;
	const gchar *cstr;
	ofeSFMode mode;
	gboolean has, with;
	myDateFormat datefmt;
	gchar ch;
	gchar *str;
	gint count;

	priv = ofa_stream_format_disp_get_instance_private( self );

	/* name */
	cstr = ofa_stream_format_get_name( priv->settings );
	gtk_label_set_text( GTK_LABEL( priv->name_data ), cstr ? cstr : "" ),

	/* mode */
	mode = ofa_stream_format_get_mode( priv->settings );
	cstr = ofa_stream_format_get_mode_localestr( mode );
	gtk_label_set_text( GTK_LABEL( priv->mode_data ), cstr ? cstr : "" ),

	/* character encoding */
	has = ofa_stream_format_get_has_charmap( priv->settings );
	cstr = has ? ofa_stream_format_get_charmap( priv->settings ) : NULL;
	gtk_label_set_text( GTK_LABEL( priv->charmap_data ), cstr ? cstr : "" );
	gtk_widget_set_sensitive( priv->charmap_label, has );
	if( has ){
		my_utils_widget_set_style( priv->charmap_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->charmap_data, has );

	/* date format */
	has = ofa_stream_format_get_has_date( priv->settings );
	datefmt = has ? ofa_stream_format_get_date_format( priv->settings ) : 0;
	cstr = datefmt ? my_date_get_format_str( datefmt ) : "";
	gtk_label_set_text( GTK_LABEL( priv->date_data ), cstr ? cstr : "" );
	gtk_widget_set_sensitive( priv->date_label, has );
	if( has ){
		my_utils_widget_set_style( priv->date_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->date_data, has );

	/* thousand sep */
	has = ofa_stream_format_get_has_thousand( priv->settings );
	ch = has ? ofa_stream_format_get_thousand_sep( priv->settings ) : 0;
	str = ch ? g_strdup_printf( "%c (0x%2.2x)", ch, ch ) : g_strdup( _( "(none)" ));
	gtk_label_set_text( GTK_LABEL( priv->thousand_data ), str );
	g_free( str );
	gtk_widget_set_sensitive( priv->thousand_label, has );
	if( has ){
		my_utils_widget_set_style( priv->thousand_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->thousand_data, has );

	/* decimal sep */
	has = ofa_stream_format_get_has_decimal( priv->settings );
	ch = has ? ofa_stream_format_get_decimal_sep( priv->settings ) : 0;
	str = ch ? g_strdup_printf( "%c (0x%2.2x)", ch, ch ) : g_strdup( _( "(none)" ));
	gtk_label_set_text( GTK_LABEL( priv->decimal_data ), str );
	g_free( str );
	gtk_widget_set_sensitive( priv->decimal_label, has );
	if( has ){
		my_utils_widget_set_style( priv->decimal_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->decimal_data, has );

	/* field sep */
	has = ofa_stream_format_get_has_field( priv->settings );
	ch = has ? ofa_stream_format_get_field_sep( priv->settings ) : 0;
	str = ch ? g_strdup_printf( "%c (0x%2.2x)", ch, ch ) : g_strdup( _( "(none)" ));
	gtk_label_set_text( GTK_LABEL( priv->field_data ), str );
	g_free( str );
	gtk_widget_set_sensitive( priv->field_label, has );
	if( has ){
		my_utils_widget_set_style( priv->field_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->field_data, has );

	/* string delimiter */
	has = ofa_stream_format_get_has_strdelim( priv->settings );
	ch = has ? ofa_stream_format_get_string_delim( priv->settings ) : 0;
	str = ch ? g_strdup_printf( "%c (0x%2.2x)", ch, ch ) : g_strdup( _( "(none)" ));
	gtk_label_set_text( GTK_LABEL( priv->strdelim_data ), str );
	g_free( str );
	gtk_widget_set_sensitive( priv->strdelim_label, has );
	if( has ){
		my_utils_widget_set_style( priv->strdelim_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->strdelim_data, has );

	/* headers */
	has = ofa_stream_format_get_has_headers( priv->settings );
	switch( mode ){
		case OFA_SFMODE_EXPORT:
			with = ofa_stream_format_get_with_headers( priv->settings );
			str = g_strdup_printf( "%s", with ? _( "True" ):_( "False" ));
			break;

		case OFA_SFMODE_IMPORT:
			count = ofa_stream_format_get_headers_count( priv->settings );
			str = g_strdup_printf( "%d", count );
			break;

		default:
			str = g_strdup_printf( _( "(invalid mode: %d)" ), mode );
			break;
	}
	gtk_label_set_text( GTK_LABEL( priv->headers_data ), str );
	g_free( str );
	gtk_widget_set_sensitive( priv->headers_label, has );
	if( has ){
		my_utils_widget_set_style( priv->headers_data, "labelinfo" );
	} else {
		my_utils_widget_remove_style( priv->headers_data, "labelinfo" );
	}
	gtk_widget_set_sensitive( priv->headers_data, has );
}
