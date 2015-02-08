/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFO_OPE_TEMPLATE_DEF_H__
#define __OFO_OPE_TEMPLATE_DEF_H__

/**
 * SECTION: ofo_ope_template
 * @short_description: #ofoOpeTemplate class definition.
 * @include: api/ofo-ope-template.h
 *
 * This file defines the #ofoOpeTemplate class behavior.
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_OPE_TEMPLATE                ( ofo_ope_template_get_type())
#define OFO_OPE_TEMPLATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplate ))
#define OFO_OPE_TEMPLATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplateClass ))
#define OFO_IS_OPE_TEMPLATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_OPE_TEMPLATE ))
#define OFO_IS_OPE_TEMPLATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_OPE_TEMPLATE ))
#define OFO_OPE_TEMPLATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_OPE_TEMPLATE, ofoOpeTemplateClass ))

typedef struct _ofoOpeTemplatePrivate        ofoOpeTemplatePrivate;

typedef struct {
	/*< public members >*/
	ofoBase                parent;

	/*< private members >*/
	ofoOpeTemplatePrivate *priv;
}
	ofoOpeTemplate;

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoOpeTemplateClass;

GType ofo_ope_template_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFO_OPE_TEMPLATE_DEF_H__ */
