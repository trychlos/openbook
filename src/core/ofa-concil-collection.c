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

#include "api/ofo-concil.h"
#include "api/ofo-dossier.h"
#include "api/ofs-concil-id.h"

#include "core/ofa-collection-prot.h"
#include "core/ofa-concil-collection.h"
#include "core/ofa-icollector.h"

/* priv instance data
 */
struct _ofaConcilCollectionPrivate {
	GList *concils;						/* list of known conciliation groups */
	GList *unconcils;					/* list of known unreconciliated members */
};

G_DEFINE_TYPE( ofaConcilCollection, ofa_concil_collection, OFA_TYPE_COLLECTION )

static ofoConcil *concil_collection_get_by_id( ofaConcilCollection *collection, ofxCounter rec_id, ofoDossier *dossier );
static ofoConcil *find_among_reconciliated( ofaConcilCollection *self, const gchar *type, ofxCounter id );
static gboolean   find_among_unreconciliated( ofaConcilCollection *self, const gchar *type, ofxCounter id );
static void       add_to_unconcils( ofaConcilCollection *self, const gchar *type, ofxCounter id );

static void
concil_collection_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_concil_collection_finalize";

	g_return_if_fail( instance && OFA_IS_CONCIL_COLLECTION( instance ));

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_concil_collection_parent_class )->finalize( instance );
}

static void
concil_collection_dispose( GObject *instance )
{
	ofaConcilCollectionPrivate *priv;

	if( !OFA_COLLECTION( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFA_CONCIL_COLLECTION( instance )->priv;

		g_list_free_full( priv->concils, ( GDestroyNotify ) g_object_unref );
		g_list_free_full( priv->unconcils, ( GDestroyNotify ) ofs_concil_id_free );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_concil_collection_parent_class )->dispose( instance );
}

static void
ofa_concil_collection_init( ofaConcilCollection *self )
{
	static const gchar *thisfn = "ofa_concil_collection_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_CONCIL_COLLECTION, ofaConcilCollectionPrivate );
}

static void
ofa_concil_collection_class_init( ofaConcilCollectionClass *klass )
{
	static const gchar *thisfn = "ofa_concil_collection_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = concil_collection_dispose;
	G_OBJECT_CLASS( klass )->finalize = concil_collection_finalize;

	g_type_class_add_private( klass, sizeof( ofaConcilCollectionPrivate ));
}

/**
 * ofa_concil_collection_get_by_id:
 * @rec_id:
 * @dossier:
 *
 * Returns: the #ofoConcil reconciliation group identified by @rec_id,
 *  or %NULL.
 *
 * The collection is first searched in memory, and only then in
 * database. The database result is cached in memory.
 */
ofoConcil *
ofa_concil_collection_get_by_id( ofxCounter rec_id, ofoDossier *dossier )
{
	ofaConcilCollectionPrivate *priv;
	GObject *collection;
	ofoConcil *concil;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	collection = ofa_icollector_get_object( OFA_ICOLLECTOR( dossier ), OFA_TYPE_CONCIL_COLLECTION );
	g_return_val_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ), NULL );

	concil = concil_collection_get_by_id( OFA_CONCIL_COLLECTION( collection ), rec_id, dossier );
	if( !concil ){
		concil = ofo_concil_get_by_id( dossier, rec_id );
		if( concil ){
			priv = OFA_CONCIL_COLLECTION( collection )->priv;
			priv->concils = g_list_prepend( priv->concils, concil );
		}
	}

	return( concil );
}

static ofoConcil *
concil_collection_get_by_id( ofaConcilCollection *collection, ofxCounter rec_id, ofoDossier *dossier )
{
	ofaConcilCollectionPrivate *priv;
	ofoConcil *concil, *found;
	GList *it;

	found = NULL;

	if( !OFA_COLLECTION( collection )->prot->dispose_has_run ){

		priv = collection->priv;

		for( it=priv->concils ; it ; it=it->next ){
			concil = OFO_CONCIL( it->data );
			if( ofo_concil_get_id( concil ) == rec_id ){
				found = concil;
				break;
			}
		}
	}

	return( found );
}

/**
 * ofa_concil_collection_get_by_other_id:
 * @collection:
 * @type:
 * @id:
 * @dossier:
 *
 * Returns: the #ofoConcil reconciliation group the member specified
 * by its @type and its @id belongs to, or %NULL.
 *
 * The collection is first searched in memory, and only then in
 * database. The database result is cached in memory.
 */
ofoConcil *
ofa_concil_collection_get_by_other_id( ofaConcilCollection *collection,
		const gchar *type, ofxCounter id, ofoDossier *dossier )
{
	ofaConcilCollectionPrivate *priv;
	ofoConcil *concil;

	g_return_val_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ), NULL );
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	concil = NULL;
	priv = collection->priv;

	if( !OFA_COLLECTION( collection )->prot->dispose_has_run ){

		concil = find_among_reconciliated( collection, type, id );
		if( !concil && !find_among_unreconciliated( collection, type, id )){
			concil = ofo_concil_get_by_other_id( dossier, type, id );
			if( concil ){
				priv->concils = g_list_prepend( priv->concils, concil );
			} else {
				add_to_unconcils( collection, type, id );
			}
		}
	}

	return( concil );
}

/**
 * ofa_concil_collection_add:
 * @collection:
 * @concil:
 */
void
ofa_concil_collection_add( ofaConcilCollection *collection, ofoConcil *concil )
{
	ofaConcilCollectionPrivate *priv;

	g_return_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	if( !OFA_COLLECTION( collection )->prot->dispose_has_run ){

		priv = collection->priv;
		priv->concils = g_list_prepend( priv->concils, concil );
	}
}

/**
 * ofa_concil_collection_remove:
 * @collection:
 * @concil:
 */
void
ofa_concil_collection_remove( ofaConcilCollection *collection, ofoConcil *concil )
{
	static const gchar *thisfn = "ofa_concil_collection_remove";
	ofaConcilCollectionPrivate *priv;

	g_return_if_fail( collection && OFA_IS_CONCIL_COLLECTION( collection ));
	g_return_if_fail( concil && OFO_IS_CONCIL( concil ));

	if( !OFA_COLLECTION( collection )->prot->dispose_has_run ){

		priv = collection->priv;
		priv->concils = g_list_remove( priv->concils, concil );
		g_debug( "%s: collection=%p, concil=%p, id=%ld",
				thisfn, ( void * ) collection, ( void * ) concil, ofo_concil_get_id( concil ));
		g_object_unref( concil );
	}
}

static ofoConcil *
find_among_reconciliated( ofaConcilCollection *self, const gchar *type, ofxCounter id )
{
	ofaConcilCollectionPrivate *priv;
	ofoConcil *concil, *found;
	GList *it;

	found = NULL;
	priv = self->priv;

	for( it=priv->concils ; it ; it=it->next ){
		concil = OFO_CONCIL( it->data );
		if( ofo_concil_has_member( concil, type, id )){
			found = concil;
			break;
		}
	}

	return( found );
}

static gboolean
find_among_unreconciliated( ofaConcilCollection *self, const gchar *type, ofxCounter id )
{
	ofaConcilCollectionPrivate *priv;
	gboolean found;
	GList *it;
	ofsConcilId *sid;

	found = FALSE;
	priv = self->priv;

	for( it=priv->unconcils ; it ; it=it->next ){
		sid = ( ofsConcilId * ) it->data;
		if( ofs_concil_id_is_equal( sid, type, id )){
			found = TRUE;
			break;
		}
	}

	return( found );
}

static void
add_to_unconcils( ofaConcilCollection *self, const gchar *type, ofxCounter id )
{
	ofaConcilCollectionPrivate *priv;
	ofsConcilId *unconcil;

	priv = self->priv;

	unconcil = ofs_concil_id_new();
	unconcil->type = g_strdup( type );
	unconcil->other_id = id;

	priv->unconcils = g_list_prepend( priv->unconcils, unconcil );
}
