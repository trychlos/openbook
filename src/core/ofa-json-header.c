/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
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

#include <json-glib/json-glib.h>

#include "api/ofa-json-header.h"

/* private instance data
 */
typedef struct {
	gboolean               dispose_has_run;
}
	ofaJsonHeaderPrivate;

G_DEFINE_TYPE_EXTENDED( ofaJsonHeader, ofa_json_header, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaJsonHeader ))

static void
json_header_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_json_header_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_JSON_HEADER( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_json_header_parent_class )->finalize( instance );
}

static void
json_header_dispose( GObject *instance )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_JSON_HEADER( instance ));

	priv = ofa_json_header_get_instance_private( OFA_JSON_HEADER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_json_header_parent_class )->dispose( instance );
}

static void
ofa_json_header_init( ofaJsonHeader *self )
{
	static const gchar *thisfn = "ofa_json_header_init";
	ofaJsonHeaderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_JSON_HEADER( self ));

	priv = ofa_json_header_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_json_header_class_init( ofaJsonHeaderClass *klass )
{
	static const gchar *thisfn = "ofa_json_header_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = json_header_dispose;
	G_OBJECT_CLASS( klass )->finalize = json_header_finalize;
}

/**
 * ofa_json_header_new:
 *
 * Allocates and initializes a #ofaJsonHeader object.
 *
 * Returns: a new #ofaJsonHeader object.
 */
ofaJsonHeader *
ofa_json_header_new( void )
{
	ofaJsonHeader *header;

	header = g_object_new( OFA_TYPE_JSON_HEADER, NULL );

	return( header );
}
