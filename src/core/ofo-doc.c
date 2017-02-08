/*
 * A double-entry accounting application for professional services.
 *
 * Open Firm Accounting
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-doc.h"

/* priv instance data
 */
typedef struct {
	void *empty;
}
	ofoDocPrivate;

G_DEFINE_TYPE_EXTENDED( ofoDoc, ofo_doc, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoDoc ))

static void
doc_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_doc_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_DOC( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_doc_parent_class )->finalize( instance );
}

static void
doc_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_doc_dispose";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_doc_parent_class )->dispose( instance );
}

static void
ofo_doc_init( ofoDoc *self )
{
	static const gchar *thisfn = "ofo_doc_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));
}

static void
ofo_doc_class_init( ofoDocClass *klass )
{
	static const gchar *thisfn = "ofo_doc_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = doc_dispose;
	G_OBJECT_CLASS( klass )->finalize = doc_finalize;
}
