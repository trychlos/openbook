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

#include "ui/ofa-page-book.h"

/* private instance data
 */
struct _ofaPageBookPrivate {
	gboolean       dispose_has_run;

	/* UI
	 */
	GtkNotebook   *book;
};

G_DEFINE_TYPE( ofaPageBook, ofa_page_book, OFA_TYPE_PAGE )

static void
page_book_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_page_book_finalize";
	ofaPageBookPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_PAGE_BOOK( instance ));

	priv = OFA_PAGE_BOOK( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_page_book_parent_class )->finalize( instance );
}

static void
page_book_dispose( GObject *instance )
{
	ofaPageBookPrivate *priv;

	g_return_if_fail( instance && OFA_IS_PAGE_BOOK( instance ));

	priv = ( OFA_PAGE_BOOK( instance ))->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_page_book_parent_class )->dispose( instance );
}

static void
ofa_page_book_init( ofaPageBook *self )
{
	static const gchar *thisfn = "ofa_page_book_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_PAGE_BOOK( self ));

	self->private = g_new0( ofaPageBookPrivate, 1 );

	self->private->dispose_has_run = FALSE;
}

static void
ofa_page_book_class_init( ofaPageBookClass *klass )
{
	static const gchar *thisfn = "ofa_page_book_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = page_book_dispose;
	G_OBJECT_CLASS( klass )->finalize = page_book_finalize;
}
