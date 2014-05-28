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

#include "ui/ofo-base.h"
#include "ui/ofo-base-prot.h"

/* private instance data
 */
struct _ofoBasePrivate {
	void *empty;						/* so that gcc -pedantic is empty */
};

/* signals defined here
 */
enum {
	NEW_ENTRY,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ] = { 0 };

G_DEFINE_TYPE( ofoBase, ofo_base, G_TYPE_OBJECT )

#define OFO_BASE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_BASE, ofoBasePrivate))

static void
ofo_base_finalize( GObject *instance )
{
	ofoBase *self = OFO_BASE( instance );

	/* free data here */
	g_free( self->prot );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->finalize( instance );
}

static void
ofo_base_dispose( GObject *instance )
{
	ofoBase *self = OFO_BASE( instance );

	if( self->prot->dispose_has_run ){

		self->prot->dispose_has_run = TRUE;

		/* unref member objects here */
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_base_parent_class )->dispose( instance );
}

static void
ofo_base_init( ofoBase *self )
{
	self->priv = OFO_BASE_GET_PRIVATE( self );

	self->prot = g_new0( ofoBaseProtected, 1 );

	self->prot->dispose_has_run = FALSE;
}

static void
ofo_base_class_init( ofoBaseClass *klass )
{
	static const gchar *thisfn = "ofo_base_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoBasePrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_base_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_base_finalize;

	/*
	 * ofoBase::ofa-signal-new-entry:
	 *
	 * This signal is sent by the entry being just inserted.
	 * Other objects are suggested to connect to this signal in order
	 * to update themselves.
	 *
	 * The #ofoEntry entry object is passed as an argument. The emitter
	 * of the signal - usually the #ofoEntry class itself - takes care
	 * of getting a reference on the object, so that consumers are sure
	 * that the object is alive during signal processing.
	 *
	 * The default signal handler will decrement the reference count,
	 * thus releasing the object for application purposes.
	 *
	 * Handler is of type:
	 * 		void ( *handler )( ofoDossier *dossier,
	 * 							ofoEntry *entry,
	 * 							gpointer user_data );
	 */
	st_signals[ NEW_ENTRY ] = g_signal_new_class_handler(
				OFA_SIGNAL_NEW_ENTRY,
				OFO_TYPE_BASE,
				G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
				G_CALLBACK( on_new_entry_cleanup_handler ),
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_POINTER );
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

static void
on_new_entry_cleanup_handler( ofoDossier *dossier, ofoEntry *entry, gpointer user_data )
{
	static const gchar *thisfn = "ofo_base_on_new_entry_cleanup_handler";

	g_debug( "%s: dossier=%p, entry=%p, user_data=%p",
			thisfn, ( void * ) dossier, ( void * ) entry, ( void * ) user_data );

	g_object_unref( entry );
}
