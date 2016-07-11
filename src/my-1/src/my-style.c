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

#include "my/my-style.h"
#include "my/my-utils.h"

/* private instance data
 */
typedef struct {
	gboolean        dispose_has_run;

	GtkCssProvider *provider;
}
	myStylePrivate;

static myStyle *st_style                = NULL;

static myStyle *style_new( void );
static myStyle *style_get_singleton( void );

G_DEFINE_TYPE_EXTENDED( myStyle, my_style, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( myStyle ))

static void
style_finalize( GObject *object )
{
	static const gchar *thisfn = "my_style_finalize";

	g_debug( "%s: object=%p (%s)",
			thisfn, ( void * ) object, G_OBJECT_TYPE_NAME( object ));

	g_return_if_fail( object && MY_IS_STYLE( object ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_style_parent_class )->finalize( object );
}

static void
style_dispose( GObject *object )
{
	myStylePrivate *priv;

	g_return_if_fail( object && MY_IS_STYLE( object ));

	priv = my_style_get_instance_private( MY_STYLE( object ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->provider );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_style_parent_class )->dispose( object );
}

static void
my_style_init( myStyle *self )
{
	static const gchar *thisfn = "my_style_init";
	myStylePrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_STYLE( self ));

	priv = my_style_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	priv->provider = gtk_css_provider_new();
}

static void
my_style_class_init( myStyleClass *klass )
{
	static const gchar *thisfn = "my_style_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = style_dispose;
	G_OBJECT_CLASS( klass )->finalize = style_finalize;
}

/*
 * my_style_new:
 *
 * Returns: a new reference to a #myStyle object.
 *
 * The returned reference should be #g_object_unref() by the caller.
 */
static myStyle *
style_new( void )
{
	myStyle *style;

	style = g_object_new( MY_TYPE_STYLE, NULL );

	return( style );
}

/*
 * style_get_singleton:
 *
 * Returns: the unique reference to the #myStyle singleton.
 */
static myStyle *
style_get_singleton( void )
{
	if( !st_style ){
		st_style = style_new();
	}

	return( st_style );
}

/**
 * my_style_set_css_resource:
 * @path: the full path to the resource.
 *
 * Initialize the internal singleton with the provided CSS resource.
 */
void
my_style_set_css_resource( const gchar *path )
{
	static const gchar *thisfn = "my_style_set_css_resource";
	myStyle *style;
	myStylePrivate *priv;

	g_debug( "%s: path=%s", thisfn, path );

	g_return_if_fail( my_strlen( path ));

	style = style_get_singleton();

	priv = my_style_get_instance_private( style );

	gtk_css_provider_load_from_resource( priv->provider, path );
}

/**
 * my_style_add:
 * @widget: the target #GtkWidget.
 * @class: the desired style class.
 *
 * Adds the specified @class to the current style classes of @widget.
 */
void
my_style_add( GtkWidget *widget, const gchar *class )
{
	static const gchar *thisfn = "my_style_add";
	myStyle *style;
	myStylePrivate *priv;
	GtkStyleContext *context;

	g_debug( "%s: widget=%p (%s), class=%s",
			thisfn, ( void * ) widget, G_OBJECT_TYPE_NAME( widget ), class );

	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));
	g_return_if_fail( my_strlen( class ));

	style = style_get_singleton();

	priv = my_style_get_instance_private( style );

	context = gtk_widget_get_style_context( widget );

	gtk_style_context_add_provider( context,
			GTK_STYLE_PROVIDER( priv->provider ),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

	gtk_style_context_add_class( context, class );
}

/**
 * my_style_remove:
 * @widget: the target #GtkWidget.
 * @class: the desired style class.
 *
 * Removes the specified @class from the current style classes of @widget.
 */
void
my_style_remove( GtkWidget *widget, const gchar *class )
{
	static const gchar *thisfn = "my_style_remove";
	myStyle *style;
	myStylePrivate *priv;
	GtkStyleContext *context;

	g_debug( "%s: widget=%p (%s), class=%s",
			thisfn, ( void * ) widget, G_OBJECT_TYPE_NAME( widget ), class );

	g_return_if_fail( widget && GTK_IS_WIDGET( widget ));
	g_return_if_fail( my_strlen( class ));

	style = style_get_singleton();

	priv = my_style_get_instance_private( style );

	context = gtk_widget_get_style_context( widget );

	gtk_style_context_add_provider( context,
			GTK_STYLE_PROVIDER( priv->provider ),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

	gtk_style_context_remove_class( context, class );
}

/**
 * my_style_free:
 *
 * Frees the #myStyle statically allocated resources.
 */
void
my_style_free( void )
{
	static const gchar *thisfn = "my_style_free";

	g_debug( "%s:", thisfn );

	g_clear_object( &st_style );
}
