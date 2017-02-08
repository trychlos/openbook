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

#ifndef __OPENBOOK_API_OFO_DOC_H__
#define __OPENBOOK_API_OFO_DOC_H__

/**
 * SECTION: ofodoc
 * @title: ofoDoc
 * @short_description: #ofoDoc class definition.
 * @include: openbook/ofo-doc.h
 *
 * This file defines the #ofoDoc public API.
 *
 * The #ofoDoc class handles the exercice meta data which are stored
 * in the OFA_T_DOSSIER table. This class may be used without actually
 * opening the dossier (from Openbook point of view), when the user just
 * wants access some data of the table.
 *
 * The #ofoDoc class does not derive from ofoBase class, and cannot
 * take advantage of standard behavior it provides. Instead all datas
 * must be handled by themselves.
 */

#include <glib-object.h>

#include "api/ofo-base-def.h"
#include "api/ofo-doc-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_DOC                ( ofo_doc_get_type())
#define OFO_DOC( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_DOC, ofoDoc ))
#define OFO_DOC_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_DOC, ofoDocClass ))
#define OFO_IS_DOC( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_DOC ))
#define OFO_IS_DOC_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_DOC ))
#define OFO_DOC_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_DOC, ofoDocClass ))

#if 0
typedef struct _ofoDoc              ofoDoc;
typedef struct _ofoDocClass         ofoDocClass;
#endif

struct _ofoDoc {
	/*< public members >*/
	ofoBase      parent;
};

struct _ofoDocClass {
	/*< public members >*/
	ofoBaseClass parent;
};

GType ofo_doc_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_DOC_H__ */
