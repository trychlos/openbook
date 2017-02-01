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

#ifndef __OFA_EBP_IDENT_H__
#define __OFA_EBP_IDENT_H__

/**
 * SECTION: ofa_ebp_ident
 * @short_description: #ofaEbpIdent class definition.
 * @include: recurrent/ofa-ebp-ident.h
 *
 * The class which provides identification to the dynamic plugin.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_EBP_IDENT                ( ofa_ebp_ident_get_type())
#define OFA_EBP_IDENT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EBP_IDENT, ofaEbpIdent ))
#define OFA_EBP_IDENT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EBP_IDENT, ofaEbpIdentClass ))
#define OFA_IS_EBP_IDENT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EBP_IDENT ))
#define OFA_IS_EBP_IDENT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EBP_IDENT ))
#define OFA_EBP_IDENT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EBP_IDENT, ofaEbpIdentClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaEbpIdent;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaEbpIdentClass;

GType ofa_ebp_ident_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_EBP_IDENT_H__ */
