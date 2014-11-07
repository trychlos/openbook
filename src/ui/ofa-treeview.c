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

#include "ui/ofa-treeview.h"
#include "ui/ofa-treeview-prot.h"

/* private instance data
 */
struct _ofaTreeviewPrivate {
	void *empty;						/* so that 'gcc -pedantic' is happy */
};

G_DEFINE_TYPE( ofaTreeview, ofa_treeview, G_TYPE_OBJECT )

static void
treeview_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_treeview_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_TREEVIEW( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_treeview_parent_class )->finalize( instance );
}

static void
treeview_dispose( GObject *instance )
{
	ofaTreeviewProtected *prot;

	g_return_if_fail( instance && OFA_IS_TREEVIEW( instance ));

	prot = ( OFA_TREEVIEW( instance ))->prot;

	if( !prot->dispose_has_run ){

		prot->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_treeview_parent_class )->dispose( instance );
}

static void
ofa_treeview_init( ofaTreeview *self )
{
	static const gchar *thisfn = "ofa_treeview_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_TREEVIEW( self ));

	self->prot = g_new0( ofaTreeviewProtected, 1 );
	self->prot->dispose_has_run = FALSE;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_TREEVIEW, ofaTreeviewPrivate );
}

static void
ofa_treeview_class_init( ofaTreeviewClass *klass )
{
	static const gchar *thisfn = "ofa_treeview_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = treeview_dispose;
	G_OBJECT_CLASS( klass )->finalize = treeview_finalize;

	g_type_class_add_private( klass, sizeof( ofaTreeviewPrivate ));
}
