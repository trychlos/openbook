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

#ifndef __OPENBOOK_API_OFO_BASE_H__
#define __OPENBOOK_API_OFO_BASE_H__

/**
 * SECTION: ofo_base
 * @short_description: #ofoBase class definition.
 * @include: openbook/ofo-base.h
 *
 * The ofoBase class is the class base for application objects.
 */

#include "api/ofa-box.h"
#include "api/ofa-hub-def.h"
#include "api/ofa-idbconnect.h"
#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_BASE                ( ofo_base_get_type())
#define OFO_BASE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_BASE, ofoBase ))
#define OFO_BASE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_BASE, ofoBaseClass ))
#define OFO_IS_BASE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_BASE ))
#define OFO_IS_BASE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_BASE ))
#define OFO_BASE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_BASE, ofoBaseClass ))

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
		g_return_val_if_fail((V) && OFO_IS_ ## C(V),(R)); \
		if( OFO_BASE(V)->prot->dispose_has_run) g_return_val_if_reached(R); \
		return(ofa_box_get_ ## T(OFO_BASE(V)->prot->fields,(I)))

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
		g_return_if_fail((V) && OFO_IS_ ## C(V)); \
		g_return_if_fail( !OFO_BASE(V)->prot->dispose_has_run ); \
		ofa_box_set_ ## T(OFO_BASE(V)->prot->fields,(I),(D))

/**
 * Identifier unset value
 */
#define OFO_BASE_UNSET_ID               -1

GType   ofo_base_get_type        ( void ) G_GNUC_CONST;

GList  *ofo_base_init_fields_list( const ofsBoxDef *defs );

GList  *ofo_base_load_dataset    ( const ofsBoxDef *defs,
										const gchar *from,
										GType type,
										ofaHub *hub );

GList  *ofo_base_load_dataset_from_dossier( const ofsBoxDef *defs,
											const ofaIDBConnect *connect,
											const gchar *from,
											GType type );

GList  *ofo_base_load_rows       ( const ofsBoxDef *defs,
										const ofaIDBConnect *connect,
										const gchar *from );

ofaHub *ofo_base_get_hub         ( const ofoBase *base );

void    ofo_base_set_hub         ( ofoBase *base,
										ofaHub *hub );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_BASE_H__ */
