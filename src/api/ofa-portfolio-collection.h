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

#ifndef __OPENBOOK_API_OFA_PORTFOLIO_COLLECTION_H__
#define __OPENBOOK_API_OFA_PORTFOLIO_COLLECTION_H__

/**
 * SECTION: ofa_portfolio_collection
 * @short_description: #ofaPortfolioCollection class definition.
 * @include: openbook/ofa-portfolio-collection.h
 *
 * This class manages the dossiers directory as a list of #ofaIDBMeta
 * instances.
 *
 * It is defined to be implemented as a singleton by any program of the
 * Openbook software suite. It takes care of maintaining itself up-to-
 * date, and sends a 'changed' signal when the directory has changed
 * and has been reloaded.
 *
 * This is an Openbook software suite decision to have the dossiers
 * directory stored in a single dedicated ini file, said dossiers
 * settings.
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"
#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PORTFOLIO_COLLECTION                ( ofa_portfolio_collection_get_type())
#define OFA_PORTFOLIO_COLLECTION( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PORTFOLIO_COLLECTION, ofaPortfolioCollection ))
#define OFA_PORTFOLIO_COLLECTION_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PORTFOLIO_COLLECTION, ofaPortfolioCollectionClass ))
#define OFA_IS_PORTFOLIO_COLLECTION( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PORTFOLIO_COLLECTION ))
#define OFA_IS_PORTFOLIO_COLLECTION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PORTFOLIO_COLLECTION ))
#define OFA_PORTFOLIO_COLLECTION_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PORTFOLIO_COLLECTION, ofaPortfolioCollectionClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaPortfolioCollection;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaPortfolioCollectionClass;

GType                   ofa_portfolio_collection_get_type            ( void ) G_GNUC_CONST;

ofaPortfolioCollection *ofa_portfolio_collection_new                 ( ofaHub *hub );

GList                  *ofa_portfolio_collection_get_dossiers        ( ofaPortfolioCollection *collection );

#define                 ofa_portfolio_collection_free_dossiers(L)    g_list_free_full(( L ), \
																			( GDestroyNotify ) g_object_unref )

guint                   ofa_portfolio_collection_get_dossiers_count  ( ofaPortfolioCollection *collection );

ofaIDBMeta             *ofa_portfolio_collection_get_meta            ( ofaPortfolioCollection *collection,
																			const gchar *dossier_name );

void                    ofa_portfolio_collection_set_meta_from_editor( ofaPortfolioCollection *collection,
																			ofaIDBMeta *meta,
																			const ofaIDBEditor *editor );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_PORTFOLIO_COLLECTION_H__ */
