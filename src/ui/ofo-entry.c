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

#include <string.h>

#include "ui/my-utils.h"
#include "ui/ofo-entry.h"

/* priv instance data
 */
struct _ofoEntryPrivate {
	gboolean dispose_has_run;

	/* properties
	 */

	/* internals
	 */

	/* sgbd data
	 */
	GDate          effect;
	gint           number;
	GDate          operation;
	gchar         *label;
	gchar         *ref;
	gint           dev_id;
	gint           jou_id;
	gdouble        amount;
	ofaEntrySens   sens;
	ofaEntryStatus status;
	gchar         *maj_user;
	GTimeVal       maj_stamp;
};

G_DEFINE_TYPE( ofoEntry, ofo_entry, OFO_TYPE_BASE )

#define OFO_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), OFO_TYPE_ENTRY, ofoEntryPrivate))

static void
ofo_entry_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_finalize";
	ofoEntry *self;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	self = OFO_ENTRY( instance );

	g_free( self->priv->label );
	g_free( self->priv->ref );
	g_free( self->priv->maj_user );

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->finalize( instance );
}

static void
ofo_entry_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_entry_dispose";
	ofoEntry *self;

	self = OFO_ENTRY( instance );

	if( !self->priv->dispose_has_run ){

		g_debug( "%s: instance=%p (%s)",
				thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

		self->priv->dispose_has_run = TRUE;
	}

	/* chain up to parent class */
	G_OBJECT_CLASS( ofo_entry_parent_class )->dispose( instance );
}

static void
ofo_entry_init( ofoEntry *self )
{
	static const gchar *thisfn = "ofo_entry_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = OFO_ENTRY_GET_PRIVATE( self );

	self->priv->dispose_has_run = FALSE;

	g_date_clear( &self->priv->effect, 1 );
	g_date_clear( &self->priv->operation, 1 );
}

static void
ofo_entry_class_init( ofoEntryClass *klass )
{
	static const gchar *thisfn = "ofo_entry_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	g_type_class_add_private( klass, sizeof( ofoEntryPrivate ));

	G_OBJECT_CLASS( klass )->dispose = ofo_entry_dispose;
	G_OBJECT_CLASS( klass )->finalize = ofo_entry_finalize;
}

/**
 * ofo_entry_new:
 */
ofoEntry *
ofo_entry_new( const GDate *effet, const GDate *ope, const gchar *label, const gchar *ref,
					gint dev_id, gint jou_id, gdouble amount, ofaEntrySens sens )
{
	ofoEntry *entry;

	entry = g_object_new( OFO_TYPE_ENTRY, NULL );

	if( effet && g_date_valid( effet )){
		memcpy( &entry->priv->effect, effet, sizeof( GDate ));
	}
	entry->priv->number = 0;
	if( ope && g_date_valid( ope )){
		memcpy( &entry->priv->operation, ope, sizeof( GDate ));
	}
	entry->priv->label = g_strdup( label );
	entry->priv->ref = g_strdup( ref );
	entry->priv->dev_id = dev_id;
	entry->priv->jou_id = jou_id;
	entry->priv->amount = amount;
	entry->priv->sens = sens;
	entry->priv->status = ENT_STATUS_ROUGH;

	return( entry );
}

/**
 * ofo_entry_insert:
 */
gboolean
ofo_entry_insert( ofoEntry *entry, ofoDossier *dossier )
{
	return( FALSE );
}

/**
 * ofo_entry_validate:
 */
gboolean
ofo_entry_validate( ofoEntry *entry, ofoDossier *dossier )
{
	return( FALSE );
}

/**
 * ofo_entry_delete:
 */
gboolean
ofo_entry_delete( ofoEntry *entry, ofoDossier *dossier )
{
	return( FALSE );
}
