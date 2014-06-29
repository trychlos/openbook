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

#include "ui/ofa-tab-label.h"

/* private instance data
 */
struct _ofaTabLabelPrivate {
	gboolean   dispose_has_run;

	GtkWidget *close_btn;
};

/* signals defined here
 */
enum {
	TAB_CLOSE_CLICKED = 0,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofaTabLabel, ofa_tab_label, GTK_TYPE_GRID )

static void setup_tab_content( ofaTabLabel *tab, GtkImage *image, const gchar *text );
static void setup_tab_style( ofaTabLabel *tab );
static void on_close_button_clicked( GtkButton *button, ofaTabLabel *tab );
static void on_tab_close_clicked_class_handler( ofaTabLabel *tab );

static void
tab_label_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_tab_label_finalize";
	ofaTabLabelPrivate *priv;

	g_return_if_fail( instance && OFA_IS_TAB_LABEL( instance ));

	priv = OFA_TAB_LABEL( instance )->private;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tab_label_parent_class )->finalize( instance );
}

static void
tab_label_dispose( GObject *instance )
{
	ofaTabLabel *self;

	g_return_if_fail( instance && OFA_IS_TAB_LABEL( instance ));

	self = OFA_TAB_LABEL( instance );

	if( !self->private->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_tab_label_parent_class )->dispose( instance );
}

static void
ofa_tab_label_init( ofaTabLabel *self )
{
	static const gchar *thisfn = "ofa_tab_label_init";

	g_return_if_fail( OFA_IS_TAB_LABEL( self ));

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->private = g_new0( ofaTabLabelPrivate, 1 );
}

static void
ofa_tab_label_class_init( ofaTabLabelClass *klass )
{
	static const gchar *thisfn = "ofa_tab_label_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = tab_label_dispose;
	G_OBJECT_CLASS( klass )->finalize = tab_label_finalize;

	/**
	 * ofaTabLabel::tab-close-clicked:
	 *
	 * This signal is emitted when the user clicks on the close button,
	 * on the right of the tab label
	 *
	 * Handler is of type:
	 * 		void user_handler ( ofaTabLabel *tab,
	 * 								gpointer user_data );
	 */
	st_signals[ TAB_CLOSE_CLICKED ] = g_signal_new_class_handler(
				OFA_SIGNAL_TAB_CLOSE_CLICKED,
				OFA_TYPE_TAB_LABEL,
				G_SIGNAL_RUN_LAST,
				G_CALLBACK( on_tab_close_clicked_class_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				0 );
}

/**
 * ofa_tab_label_new:
 * @image: [allow-none]: should be of GTK_ICON_SIZE_MENU size
 * @text:
 */
ofaTabLabel *
ofa_tab_label_new( GtkImage *image, const gchar *text )
{
	ofaTabLabel *self;

	self = g_object_new( OFA_TYPE_TAB_LABEL, NULL );

	setup_tab_content( self, image, text );
	setup_tab_style( self );

	gtk_widget_show_all( GTK_WIDGET( self ));

	return( self );
}

static void
setup_tab_content( ofaTabLabel *tab, GtkImage *image, const gchar *text )
{
	ofaTabLabelPrivate *priv;
	GtkWidget *label;

	priv = tab->private;

	gtk_grid_set_column_spacing( GTK_GRID( tab ), 5 );

	if( image ){
		gtk_grid_attach( GTK_GRID( tab ), GTK_WIDGET( image ), 0, 0, 1, 1 );
	}

	label = gtk_label_new_with_mnemonic( text );
	gtk_grid_attach( GTK_GRID( tab ), label, 1, 0, 1, 1 );

	priv->close_btn = gtk_button_new();
	gtk_button_set_relief( GTK_BUTTON( priv->close_btn ), GTK_RELIEF_NONE );
	gtk_button_set_focus_on_click( GTK_BUTTON( priv->close_btn ), FALSE );
	gtk_button_set_image( GTK_BUTTON( priv->close_btn ),
			gtk_image_new_from_stock( GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU ));
	g_signal_connect( G_OBJECT( priv->close_btn ), "clicked", G_CALLBACK( on_close_button_clicked ), tab );

	gtk_grid_attach( GTK_GRID( tab ), priv->close_btn, 2, 0, 1, 1 );
}

static void
setup_tab_style( ofaTabLabel *tab )
{
	static const gchar *thisfn = "ofa_tab_label_setup_tab_style";
	static GtkCssProvider *css_provider = NULL;
	ofaTabLabelPrivate *priv;
	GError *error;
	GtkStyleContext *style;

	priv = tab->private;

	if( !css_provider ){
		css_provider = gtk_css_provider_new();
		error = NULL;
		/*g_debug( "%s: css=%s", thisfn, PKGUIDIR "/ofa.css" );*/
		if( !gtk_css_provider_load_from_path( css_provider, PKGUIDIR "/ofa.css", &error )){
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
on_close_button_clicked( GtkButton *button, ofaTabLabel *tab )
{
	g_signal_emit_by_name( G_OBJECT( tab ), OFA_SIGNAL_TAB_CLOSE_CLICKED );
}

static void
on_tab_close_clicked_class_handler( ofaTabLabel *tab )
{
	static const gchar *thisfn = "ofa_tab_label_on_tab_close_clicked_class_handler";

	g_debug( "%s: tab=%p", thisfn, ( void * ) tab );
}
