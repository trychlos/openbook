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

#include "ui/ofo-base.h"

/* private instance data
 */
struct _ofoBasePrivate {
	gboolean dispose_has_run;
};

G_DEFINE_TYPE( ofoBase, ofo_base, G_TYPE_OBJECT )

#define OFO_BASE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_BASE, ofoBasePrivate))

static void
ofo_base_finalize( GObject *instance )
{
	/*ofoBase *self = OFO_BASE( instance );*/

	/* free data here */

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->finalize( instance );
}

static void
ofo_base_dispose( GObject *instance )
{
	/*ofoBase *self = OFO_BASE( instance );*/

	/* unref member objects here */

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->dispose( instance );
}

static void
ofo_base_init( ofoBase *self )
{
	self->priv = OFO_BASE_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;
}

static void
ofo_base_class_init( ofoBaseClass *klass )
{
	static const gchar *thisfn = "ofo_base_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoBasePrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_base_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_base_finalize;
}

/**
 * ofo_base_get_global:
 * @ptr: a pointer to the static variable which is supposed to handle
 *  the global data of the child class.
 * @dossier: the currently opened #ofoDossier dossier.
 * @fn: the #GWeakNotify function which will be called when the @dossier
 *  will be disposed. The #OFO_BASE_DEFINE_GLOBAL macro defines a
 *  <class>_clear_global static function which clears the #GList dataset.
 * @user_data: user data to be passed to @fn. The #OFO_BASE_DEFINE_GLOBAL
 *  macro sets this value to %NULL.
 *
 * Check that the global #ofoBaseGlobal structure is allocated.
 * Allocate it if it is not yet allocated.
 * Install a weak reference of the @dossier, in order to be able to free
 * the objects allocated in #ofoBase Global->#GList dataset list.
 *
 * Returns: the same pointer that @ptr if the structure was already
 * allocated, or a pointer to a newly allocated structure.
 */
ofoBaseGlobal *
ofo_base_get_global( ofoBaseGlobal *ptr, ofoBase *dossier, GWeakNotify fn, gpointer user_data )
{
	ofoBaseGlobal *new_ptr;

	new_ptr = ptr;

	if( !ptr ){

		/* allocate a new global structure */
		new_ptr = g_new0( ofoBaseGlobal, 1 );
		new_ptr->dossier = dossier;

		/* be advertise when the main object disappears */
		g_object_weak_ref( G_OBJECT( dossier ), fn, user_data );
	}

	return( new_ptr );
}
