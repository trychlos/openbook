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

/* private class data
 */
struct _ofoBaseClassPrivate {
	void *empty;						/* so that gcc -pedantic is happy */
};

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

	G_OBJECT_CLASS( klass )->dispose = ofo_base_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_base_finalize;

	klass->priv = g_new0( ofoBaseClassPrivate, 1 );
}
