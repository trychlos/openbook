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

#include "core/ofa-collection.h"
#include "core/ofa-collection-prot.h"

/* priv instance data
 */
struct _ofaCollectionPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

G_DEFINE_TYPE( ofaCollection, ofa_collection, G_TYPE_OBJECT )

static void
collection_finalize( GObject *instance )
{
	g_return_if_fail( instance && OFA_IS_COLLECTION( instance ));

	/* free data members here */
	g_free( OFA_COLLECTION( instance )->prot );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_collection_parent_class )->finalize( instance );
}

static void
collection_dispose( GObject *instance )
{
	ofaCollectionProtected *prot;

	prot = OFA_COLLECTION( instance )->prot;

	if( !prot->dispose_has_run ){
		prot->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_collection_parent_class )->dispose( instance );
}

static void
ofa_collection_init( ofaCollection *self )
{
	self->prot = g_new0( ofaCollectionProtected, 1 );
	self->prot->dispose_has_run = FALSE;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_COLLECTION, ofaCollectionPrivate );
}

static void
ofa_collection_class_init( ofaCollectionClass *klass )
{
	static const gchar *thisfn = "ofa_collection_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = collection_dispose;
	G_OBJECT_CLASS( klass )->finalize = collection_finalize;

	g_type_class_add_private( klass, sizeof( ofaCollectionPrivate ));
}
