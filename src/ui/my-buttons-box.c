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

#include "ui/my-buttons-box.h"

/* private instance data
 */
struct _myButtonsBoxPrivate {
	gboolean dispose_has_run;
};

G_DEFINE_TYPE( myButtonsBox, my_buttons_box, GTK_TYPE_BOX )

static void
buttons_box_finalize( GObject *instance )
{
	static const gchar *thisfn = "my_buttons_box_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && MY_IS_BUTTONS_BOX( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_buttons_box_parent_class )->finalize( instance );
}

static void
buttons_box_dispose( GObject *instance )
{
	myButtonsBoxPrivate *priv;

	g_return_if_fail( instance && MY_IS_BUTTONS_BOX( instance ));

	priv = ( MY_BUTTONS_BOX( instance ))->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( my_buttons_box_parent_class )->dispose( instance );
}

static void
my_buttons_box_init( myButtonsBox *self )
{
	static const gchar *thisfn = "my_buttons_box_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && MY_IS_BUTTONS_BOX( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, MY_TYPE_BUTTONS_BOX, myButtonsBoxPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
my_buttons_box_class_init( myButtonsBoxClass *klass )
{
	static const gchar *thisfn = "my_buttons_box_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = buttons_box_dispose;
	G_OBJECT_CLASS( klass )->finalize = buttons_box_finalize;

	g_type_class_add_private( klass, sizeof( myButtonsBoxPrivate ));
}

/**
 * my_buttons_box_new:
 */
myButtonsBox *
my_buttons_box_new( void )
{
	return( g_object_new( MY_TYPE_BUTTONS_BOX, NULL ));
}
