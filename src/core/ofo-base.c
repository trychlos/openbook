/*
 * Open Freelance Accounting * A double-entry accounting application for freelances.
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

#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"

/* private instance data
 */
struct _ofoBasePrivate {
	void *empty;						/* so that gcc -pedantic is empty */
};

G_DEFINE_TYPE( ofoBase, ofo_base, G_TYPE_OBJECT )

static void
ofo_base_finalize( GObject *instance )
{
	ofoBase *self = OFO_BASE( instance );

	/* free data members here */
	g_free( self->private );
	g_free( self->prot );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->finalize( instance );
}

static void
ofo_base_dispose( GObject *instance )
{
	ofoBase *self = OFO_BASE( instance );

	if( !self->prot->dispose_has_run ){

		self->prot->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->dispose( instance );
}

static void
ofo_base_init( ofoBase *self )
{
	self->private = g_new0( ofoBasePrivate, 1 );

	self->prot = g_new0( ofoBaseProtected, 1 );

	self->prot->dispose_has_run = FALSE;
}

static void
ofo_base_class_init( ofoBaseClass *klass )
{
	static const gchar *thisfn = "ofo_base_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

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
		new_ptr->send_signal_new = TRUE;

		/* be advertise when the main object disappears */
		g_object_weak_ref( G_OBJECT( dossier ), fn, user_data );
	}

	return( new_ptr );
}
