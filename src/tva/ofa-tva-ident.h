/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_TVA_IDENT_H__
#define __OFA_TVA_IDENT_H__

/**
 * SECTION: ofa_tva_ident
 * @short_description: #ofaTVAIdent class definition.
 * @include: recurrent/ofa-tva-ident.h
 *
 * The class which provides identification to the dynamic plugin.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_TVA_IDENT                ( ofa_tva_ident_get_type())
#define OFA_TVA_IDENT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_IDENT, ofaTVAIdent ))
#define OFA_TVA_IDENT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_IDENT, ofaTVAIdentClass ))
#define OFA_IS_TVA_IDENT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_IDENT ))
#define OFA_IS_TVA_IDENT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_IDENT ))
#define OFA_TVA_IDENT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_IDENT, ofaTVAIdentClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaTVAIdent;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaTVAIdentClass;

GType ofa_tva_ident_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_TVA_IDENT_H__ */
