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

#ifndef __OPENBOOK_API_OFA_DOSSIER_COLLECTION_H__
#define __OPENBOOK_API_OFA_DOSSIER_COLLECTION_H__

/**
 * SECTION: ofa_dossier_collection
 * @short_description: #ofaDossierCollection class definition.
 * @include: openbook/ofa-dossier-collection.h
 *
 * This class manages the dossiers directory as a list of
 * #ofaIDBDossierMeta instances.
 *
 * It is defined to be implemented as a singleton by any program of the
 * Openbook software suite. It takes care of maintaining itself up-to-
 * date, and sends a 'changed' signal when the directory has changed
 * and has been reloaded.
 *
 * This is an Openbook software suite decision to have the dossiers
 * directory stored in a single dedicated .ini file, said dossiers
 * settings.
 */

#include <glib-object.h>

#include "api/ofa-dossier-collection-def.h"
#include "api/ofa-idbconnect-def.h"
#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_COLLECTION                ( ofa_dossier_collection_get_type())
#define OFA_DOSSIER_COLLECTION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_COLLECTION, ofaDossierCollection ))
#define OFA_DOSSIER_COLLECTION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_COLLECTION, ofaDossierCollectionClass ))
#define OFA_IS_DOSSIER_COLLECTION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_COLLECTION ))
#define OFA_IS_DOSSIER_COLLECTION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_COLLECTION ))
#define OFA_DOSSIER_COLLECTION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_COLLECTION, ofaDossierCollectionClass ))

#if 0
typedef struct _ofaDossierCollection               ofaDossierCollection;
typedef struct _ofaDossierCollectionClass          ofaDossierCollectionClass;
#endif

struct _ofaDossierCollection {
	/*< public members >*/
	GObject      parent;
};

struct _ofaDossierCollectionClass {
	/*< public members >*/
	GObjectClass parent;
};

GType                 ofa_dossier_collection_get_type     ( void ) G_GNUC_CONST;

ofaDossierCollection *ofa_dossier_collection_new          ( ofaIGetter *getter );

GList                *ofa_dossier_collection_get_list     ( ofaDossierCollection *collection );

guint                 ofa_dossier_collection_get_count    ( ofaDossierCollection *collection );

ofaIDBDossierMeta    *ofa_dossier_collection_get_by_name  ( ofaDossierCollection *collection,
																const gchar *dossier_name );

void                  ofa_dossier_collection_add_meta     ( ofaDossierCollection *collection,
																ofaIDBDossierMeta *meta );

void                  ofa_dossier_collection_remove_meta  ( ofaDossierCollection *collection,
																ofaIDBDossierMeta *meta );

gboolean              ofa_dossier_collection_delete_period( ofaDossierCollection *collection,
																ofaIDBConnect *connect,
																ofaIDBExerciceMeta *period,
																gboolean delete_dossier_on_last,
																gchar **msgerr );

void                  ofa_dossier_collection_dump         ( ofaDossierCollection *collection );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_DOSSIER_COLLECTION_H__ */
