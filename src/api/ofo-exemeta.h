/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFO_EXEMETA_H__
#define __OPENBOOK_API_OFO_EXEMETA_H__

/**
 * SECTION: ofoexemeta
 * @title: ofoExemeta
 * @short_description: #ofoExemeta class definition.
 * @include: openbook/ofo-exemeta.h
 *
 * This file defines the #ofoExemeta public API.
 *
 * The #ofoExemeta class handles the exercice meta data which are stored
 * in the OFA_T_DOSSIER table. This class may be used without actually
 * opening the dossier (from Openbook point of view), when the user just
 * wants access some data of the table.
 *
 * The #ofoExemeta class does not derive from ofoBase class, and cannot
 * take advantage of standard behavior it provides. Instead all datas
 * must be handled by themselves.
 */

#include <glib-object.h>

#include "api/ofa-idbconnect-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_EXEMETA                ( ofo_exemeta_get_type())
#define OFO_EXEMETA( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_EXEMETA, ofoExemeta ))
#define OFO_EXEMETA_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_EXEMETA, ofoExemetaClass ))
#define OFO_IS_EXEMETA( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_EXEMETA ))
#define OFO_IS_EXEMETA_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_EXEMETA ))
#define OFO_EXEMETA_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_EXEMETA, ofoExemetaClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofoExemeta;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofoExemetaClass;

GType        ofo_exemeta_get_type      ( void ) G_GNUC_CONST;

ofoExemeta  *ofo_exemeta_new           ( ofaIDBConnect *connect );

const GDate *ofo_exemeta_get_begin_date( ofoExemeta *meta );

const GDate *ofo_exemeta_get_end_date  ( ofoExemeta *meta );

gboolean     ofo_exemeta_get_current   ( ofoExemeta *meta );

gboolean     ofo_exemeta_set_current   ( ofoExemeta *meta,
											gboolean current );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_EXEMETA_H__ */
