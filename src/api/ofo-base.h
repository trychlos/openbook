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

#ifndef __OFO_BASE_H__
#define __OFO_BASE_H__

/**
 * SECTION: ofo_base
 * @short_description: #ofoBase class definition.
 * @include: api/ofo-base.h
 *
 * The ofoBase class is the class base for application objects.
 */

#include "api/ofa-boxed.h"
#include "api/ofa-dbms-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_BASE_UNSET_ID               -1

/**
 * ofo_base_getter:
 * @C: the class mnemonic (e.g. 'ACCOUNT')
 * @V: the variable name (e.g. 'account')
 * @T: the type of required data (e.g. 'amount')
 * @R: the returned data in case of an error (e.g. '0')
 * @I: the identifier of the required field (e.g. 'ACC_DEB_AMOUNT')
 *
 * A convenience macro to get the value of an identified field from an
 * #ofoBase object.
 */
#define ofo_base_getter(C,V,T,R,I)			\
		g_return_val_if_fail( OFO_IS_ ## C(V),(R)); \
		if( OFO_BASE(V)->prot->dispose_has_run) return(R); \
		return(ofa_boxed_get_ ## T(OFO_BASE(V)->prot->fields,(I)))

/**
 * ofo_base_setter:
 * @C: the class mnemonic (e.g. 'ACCOUNT')
 * @V: the variable name (e.g. 'account')
 * @T: the type of required data (e.g. 'amount')
 * @I: the identifier of the required field (e.g. 'ACC_DEB_AMOUNT')
 * @D: the data value to be set
 *
 * A convenience macro to set the value of an identified field from an
 * #ofoBase object.
 */
#define ofo_base_setter(C,V,T,I,D)			\
		g_return_if_fail( OFO_IS_ ## C(V)); \
		if( OFO_BASE(V)->prot->dispose_has_run ) return; \
		ofa_boxed_set_ ## T(OFO_BASE(V)->prot->fields,(I),(D))

void   ofo_base_init_fields_list(const ofsBoxedDef *defs, ofoBase *object );

GList *ofo_base_load_dataset    ( const ofsBoxedDef *defs,
											const ofaDbms *dbms, const gchar *from, GType type );

G_END_DECLS

#endif /* __OFO_BASE_H__ */
