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

#include "ui/my-tab-label.h"

/* priv instance data
 */
struct _myTabLabelPrivate {
	gboolean   dispose_has_run;

	gchar     *label;

	GtkWidget *pin_btn;
	GtkWidget *close_btn;
};

/* signals defined here
 */
enum {
	TAB_CLOSE_CLICKED = 0,
	TAB_PIN_CLICKED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( myTabLabel, my_tab_label, GTK_TYPE_GRID )

static void setup_tab_content( myTabLabel *tab, GtkImage *image, const gchar *text );
static void setup_tab_style( myTabLabel *tab );
static void on_pin_button_clicked( GtkButton *button, myTabLabel *tab );
static void on_tab_pin_clicked_class_handler( myTabLabel *tab );
static void on_close_button_clicked( GtkButton *button, myTabLabel *tab );
static void on_tab_close_clicked_class_handler( myTabLabel *tab );

static void
tab_label_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_tab_label_finalize";
	myTabLabelPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_TAB_LABEL( instance ));

	/* free data members here */
	priv = MY_TAB_LABEL( instance )->priv;

	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_tab_label_parent_class )->finalize( instance );
}

static void
tab_label_dispose( GObject *instance )
{
	myTabLabel *self;

	g_return_if_fail( instance && MY_IS_TAB_LABEL( instance ));

	self = MY_TAB_LABEL( instance );

	if( !self->priv->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_tab_label_parent_class )->dispose( instance );
}

static void
my_tab_label_init( myTabLabel *self )
{
	static const gchar *thisfn = "my_tab_label_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( MY_IS_TAB_LABEL( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_TAB_LABEL, myTabLabelPrivate );
}

static void
my_tab_label_class_init( myTabLabelClass *klass )
{
	static const gchar *thisfn = "my_tab_label_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tab_label_dispose;
	G_OBJECT_CLASS( klass )->finalize = tab_label_finalize;

	g_type_class_add_private( klass, sizeof( myTabLabelPrivate ));

	/**
	 * myTabLabel::tab-close-clicked:
	 *
	 * This signal is emitted when the user clicks on the close button,
	 * on the right of the tab label
	 *
	 * Handler is of type:
	 * 		void user_handler ( myTabLabel *tab,
	 * 								gpointer user_data );
	 */
	st_signals[ TAB_CLOSE_CLICKED ] = g_signal_new_class_handler(
				MY_SIGNAL_TAB_CLOSE_CLICKED,
				MY_TYPE_TAB_LABEL,
				G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_tab_close_clicked_class_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );

	/**
	 * myTabLabel::tab-pin-clicked:
	 *
	 * This signal is emitted when the user clicks on the pin button,
	 * on the right of the tab label
	 *
	 * Handler is of type:
	 * 		void user_handler ( myTabLabel *tab,
	 * 								gpointer user_data );
	 */
	st_signals[ TAB_PIN_CLICKED ] = g_signal_new_class_handler(
				MY_SIGNAL_TAB_PIN_CLICKED,
				MY_TYPE_TAB_LABEL,
				G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_tab_pin_clicked_class_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );
}

/**
 * my_tab_label_new:
 * @image: [allow-none]: should be of GTK_ICON_SIZE_MENU size
 * @text:
 */
myTabLabel *
my_tab_label_new( GtkImage *image, const gchar *text )
{
	myTabLabel *self;

	self = g_object_new( MY_TYPE_TAB_LABEL, NULL );

	setup_tab_content( self, image, text );
	setup_tab_style( self );

	gtk_widget_show_all( GTK_WIDGET( self ));

	return( self );
}

static void
setup_tab_content( myTabLabel *tab, GtkImage *image, const gchar *text )
{
	myTabLabelPrivate *priv;
	GtkWidget *label;

	priv = tab->priv;

	gtk_grid_set_column_spacing( GTK_GRID( tab ), 5 );

	if( image ){
		gtk_grid_attach( GTK_GRID( tab ), GTK_WIDGET( image ), 0, 0, 1, 1 );
	}

	label = gtk_label_new_with_mnemonic( text );
	gtk_grid_attach( GTK_GRID( tab ), label, 1, 0, 1, 1 );
	priv->label = g_strdup( text );

	priv->pin_btn = gtk_button_new();
	gtk_button_set_relief( GTK_BUTTON( priv->pin_btn ), GTK_RELIEF_NONE );
	gtk_button_set_focus_on_click( GTK_BUTTON( priv->pin_btn ), FALSE );
	gtk_button_set_image( GTK_BUTTON( priv->pin_btn ),
			gtk_image_new_from_icon_name( "view-fullscreen", GTK_ICON_SIZE_MENU ));
	gtk_grid_attach( GTK_GRID( tab ), priv->pin_btn, 2, 0, 1, 1 );

	g_signal_connect( priv->pin_btn, "clicked", G_CALLBACK( on_pin_button_clicked ), tab );

	priv->close_btn = gtk_button_new();
	gtk_button_set_relief( GTK_BUTTON( priv->close_btn ), GTK_RELIEF_NONE );
	gtk_button_set_focus_on_click( GTK_BUTTON( priv->close_btn ), FALSE );
	gtk_button_set_image( GTK_BUTTON( priv->close_btn ),
			gtk_image_new_from_icon_name( "window-close", GTK_ICON_SIZE_MENU ));
	gtk_grid_attach( GTK_GRID( tab ), priv->close_btn, 3, 0, 1, 1 );

	g_signal_connect( priv->close_btn, "clicked", G_CALLBACK( on_close_button_clicked ), tab );
}

static void
setup_tab_style( myTabLabel *tab )
{
	static const gchar *thisfn = "my_tab_label_setup_tab_style";
	static GtkCssProvider *css_provider = NULL;
	myTabLabelPrivate *priv;
	GError *error;
	GtkStyleContext *style;

	priv = tab->priv;

	if( !css_provider ){
		css_provider = gtk_css_provider_new();
		error = NULL;
		/*g_debug( "%s: css=%s", thisfn, PKGUIDIR "/ofa.css" );*/
		if( !gtk_css_provider_load_from_path( css_provider, PKGCSSDIR "/ofa.css", &error )){
			g_warning( "%s: %s", thisfn, error->message );
			g_error_free( error );
			g_clear_object( &css_provider );
		}
	}

	if( css_provider ){
		style = gtk_widget_get_style_context( priv->close_btn );
		gtk_style_context_add_provider(
				style,
				GTK_STYLE_PROVIDER( css_provider ),
				GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
	}


}

static void
on_pin_button_clicked( GtkButton *button, myTabLabel *tab )
{
	g_signal_emit_by_name( G_OBJECT( tab ), MY_SIGNAL_TAB_PIN_CLICKED );
}

static void
on_tab_pin_clicked_class_handler( myTabLabel *tab )
{
	static const gchar *thisfn = "my_tab_label_on_tab_pin_clicked_class_handler";

	g_debug( "%s: tab=%p", thisfn, ( void * ) tab );
}

static void
on_close_button_clicked( GtkButton *button, myTabLabel *tab )
{
	g_signal_emit_by_name( G_OBJECT( tab ), MY_SIGNAL_TAB_CLOSE_CLICKED );
}

static void
on_tab_close_clicked_class_handler( myTabLabel *tab )
{
	static const gchar *thisfn = "my_tab_label_on_tab_close_clicked_class_handler";

	g_debug( "%s: tab=%p", thisfn, ( void * ) tab );
}

/**
 * my_tab_label_get_label:
 * @tab:: this #myTabLabel instance.
 *
 * Returns: the attached label as a new string which should be #g_free()
 *  by the caller.
 */
gchar *
my_tab_label_get_label( const myTabLabel *tab )
{
	myTabLabelPrivate *priv;

	g_return_val_if_fail( tab && MY_IS_TAB_LABEL( tab ), NULL );

	priv = tab->priv;

	if( priv->dispose_has_run ){
		g_return_val_if_reached( NULL );
	}

	return( g_strdup( priv->label ));
}
