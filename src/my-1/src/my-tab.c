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

#include "my/my-tab.h"
#include "my/my-utils.h"

/* priv instance data
 */
typedef struct {
	gboolean   dispose_has_run;

	GtkWidget *grid;
	gchar     *label;

	GtkWidget *pin_btn;
	GtkWidget *close_btn;
}
	myTabPrivate;

/* signals defined here
 */
enum {
	TAB_CLOSE_CLICKED = 0,
	TAB_PIN_CLICKED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

static void setup_tab_content( myTab *tab, GtkImage *image, const gchar *text );
static void setup_tab_style( myTab *tab );
static void on_pin_button_clicked( GtkButton *button, myTab *tab );
static void on_tab_pin_clicked_class_handler( myTab *tab );
static void on_close_button_clicked( GtkButton *button, myTab *tab );
static void on_tab_close_clicked_class_handler( myTab *tab );

G_DEFINE_TYPE_EXTENDED( myTab, my_tab, GTK_TYPE_EVENT_BOX, 0,
		G_ADD_PRIVATE( myTab ))

static void
tab_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_tab_finalize";
	myTabPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_TAB( instance ));

	/* free data members here */
	priv = my_tab_get_instance_private( MY_TAB( instance ));

	g_free( priv->label );

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_tab_parent_class )->finalize( instance );
}

static void
tab_dispose( GObject *instance )
{
	myTabPrivate *priv;

	g_return_if_fail( instance && MY_IS_TAB( instance ));

	priv = my_tab_get_instance_private( MY_TAB( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_tab_parent_class )->dispose( instance );
}

static void
my_tab_init( myTab *self )
{
	static const gchar *thisfn = "my_tab_init";
	myTabPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_TAB( self ));

	priv = my_tab_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
my_tab_class_init( myTabClass *klass )
{
	static const gchar *thisfn = "my_tab_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tab_dispose;
	G_OBJECT_CLASS( klass )->finalize = tab_finalize;

	/**
	 * myTab::tab-close-clicked:
	 *
	 * This signal is emitted when the user clicks on the close button,
	 * on the right of the tab label
	 *
	 * Handler is of type:
	 * 		void user_handler ( myTab *tab,
	 * 								gpointer user_data );
	 */
	st_signals[ TAB_CLOSE_CLICKED ] = g_signal_new_class_handler(
				MY_SIGNAL_TAB_CLOSE_CLICKED,
				MY_TYPE_TAB,
				G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_tab_close_clicked_class_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );

	/**
	 * myTab::tab-pin-clicked:
	 *
	 * This signal is emitted when the user clicks on the pin button,
	 * on the right of the tab label
	 *
	 * Handler is of type:
	 * 		void user_handler ( myTab *tab,
	 * 								gpointer user_data );
	 */
	st_signals[ TAB_PIN_CLICKED ] = g_signal_new_class_handler(
				MY_SIGNAL_TAB_PIN_CLICKED,
				MY_TYPE_TAB,
				G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_tab_pin_clicked_class_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );
}

/**
 * my_tab_new:
 * @image: [allow-none]: should be of GTK_ICON_SIZE_MENU size
 * @text:
 */
myTab *
my_tab_new( GtkImage *image, const gchar *text )
{
	myTab *self;

	self = g_object_new( MY_TYPE_TAB, NULL );

	setup_tab_content( self, image, text );
	setup_tab_style( self );

	gtk_widget_show_all( GTK_WIDGET( self ));

	return( self );
}

static void
setup_tab_content( myTab *tab, GtkImage *image, const gchar *text )
{
	myTabPrivate *priv;
	GtkWidget *label;

	priv = my_tab_get_instance_private( tab );

	priv->grid = gtk_grid_new();
	gtk_container_add( GTK_CONTAINER( tab ), priv->grid );

	if( image ){
		gtk_grid_attach( GTK_GRID( priv->grid ), GTK_WIDGET( image ), 0, 0, 1, 1 );
		my_utils_widget_set_margin_right( GTK_WIDGET( image ), 2 );
	}

	label = gtk_label_new_with_mnemonic( text );
	gtk_grid_attach( GTK_GRID( priv->grid ), label, 1, 0, 1, 1 );
	my_utils_widget_set_margin_right( label, 6 );
	priv->label = g_strdup( text );

	priv->pin_btn = gtk_button_new();
	gtk_widget_set_focus_on_click( priv->pin_btn, FALSE );
	gtk_button_set_relief( GTK_BUTTON( priv->pin_btn ), GTK_RELIEF_NONE );
	gtk_button_set_image( GTK_BUTTON( priv->pin_btn ),
			gtk_image_new_from_icon_name( "view-fullscreen", GTK_ICON_SIZE_MENU ));
	gtk_grid_attach( GTK_GRID( priv->grid ), priv->pin_btn, 2, 0, 1, 1 );
	my_utils_widget_set_margin_right( priv->pin_btn, 2 );

	g_signal_connect( priv->pin_btn, "clicked", G_CALLBACK( on_pin_button_clicked ), tab );

	priv->close_btn = gtk_button_new();
	gtk_widget_set_focus_on_click( priv->close_btn, FALSE );
	gtk_button_set_relief( GTK_BUTTON( priv->close_btn ), GTK_RELIEF_NONE );
	gtk_button_set_image( GTK_BUTTON( priv->close_btn ),
			gtk_image_new_from_icon_name( "window-close", GTK_ICON_SIZE_MENU ));
	gtk_grid_attach( GTK_GRID( priv->grid ), priv->close_btn, 3, 0, 1, 1 );

	g_signal_connect( priv->close_btn, "clicked", G_CALLBACK( on_close_button_clicked ), tab );
}

static void
setup_tab_style( myTab *tab )
{
	static const gchar *thisfn = "my_tab_setup_tab_style";
	static GtkCssProvider *css_provider = NULL;
	myTabPrivate *priv;
	GError *error;
	GtkStyleContext *style;

	priv = my_tab_get_instance_private( tab );

	if( !css_provider ){
		css_provider = gtk_css_provider_new();
		error = NULL;
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
on_pin_button_clicked( GtkButton *button, myTab *tab )
{
	g_signal_emit_by_name( G_OBJECT( tab ), MY_SIGNAL_TAB_PIN_CLICKED );
}

static void
on_tab_pin_clicked_class_handler( myTab *tab )
{
	static const gchar *thisfn = "my_tab_on_tab_pin_clicked_class_handler";

	g_debug( "%s: tab=%p", thisfn, ( void * ) tab );
}

static void
on_close_button_clicked( GtkButton *button, myTab *tab )
{
	g_signal_emit_by_name( G_OBJECT( tab ), MY_SIGNAL_TAB_CLOSE_CLICKED );
}

static void
on_tab_close_clicked_class_handler( myTab *tab )
{
	static const gchar *thisfn = "my_tab_on_tab_close_clicked_class_handler";

	g_debug( "%s: tab=%p", thisfn, ( void * ) tab );
}

/**
 * my_tab_get_label:
 * @tab:: this #myTab instance.
 *
 * Returns: the attached label as a new string which should be #g_free()
 *  by the caller.
 */
gchar *
my_tab_get_label( myTab *tab )
{
	myTabPrivate *priv;

	g_return_val_if_fail( tab && MY_IS_TAB( tab ), NULL );

	priv = my_tab_get_instance_private( tab );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( g_strdup( priv->label ));
}

/**
 * my_tab_set_show_close:
 * @tab:: this #myTab instance.
 * @show: whether the 'Close' button is visible.
 *
 * Hide or show the 'Close' button.
 */
void
my_tab_set_show_close( myTab *tab, gboolean show )
{
	myTabPrivate *priv;

	g_return_if_fail( tab && MY_IS_TAB( tab ));

	priv = my_tab_get_instance_private( tab );

	g_return_if_fail( !priv->dispose_has_run );

	if( show ){
		gtk_widget_show( priv->close_btn );
	} else {
		gtk_widget_hide( priv->close_btn );
	}
}

/**
 * my_tab_set_show_detach:
 * @tab:: this #myTab instance.
 * @show: whether the 'Detach' button is visible.
 *
 * Hide or show the 'Detach' button.
 */
void
my_tab_set_show_detach( myTab *tab, gboolean show )
{
	myTabPrivate *priv;

	g_return_if_fail( tab && MY_IS_TAB( tab ));

	priv = my_tab_get_instance_private( tab );

	g_return_if_fail( !priv->dispose_has_run );

	if( show ){
		gtk_widget_show( priv->pin_btn );
	} else {
		gtk_widget_hide( priv->pin_btn );
	}
}
