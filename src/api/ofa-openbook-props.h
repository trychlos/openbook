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

#ifndef __OPENBOOK_API_OFA_OPENBOOK_PROPS_H__
#define __OPENBOOK_API_OFA_OPENBOOK_PROPS_H__

/**
 * SECTION: ofahub
 * @title: ofaOpenbookProps
 * @short_description: The #ofaOpenbookProps Class Definition
 * @include: openbook/ofa-openbook-props.h
 *
 * The #ofaOpenbookProps class manages the properties of the Openbook
 * software build and runtime.
 *
 * The #ofaOpenbookProps class implements the ofaIJson interface, and
 * can thus be exported as a JSON string.
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPENBOOK_PROPS                ( ofa_openbook_props_get_type())
#define OFA_OPENBOOK_PROPS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPENBOOK_PROPS, ofaOpenbookProps ))
#define OFA_OPENBOOK_PROPS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPENBOOK_PROPS, ofaOpenbookPropsClass ))
#define OFA_IS_OPENBOOK_PROPS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPENBOOK_PROPS ))
#define OFA_IS_OPENBOOK_PROPS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPENBOOK_PROPS ))
#define OFA_OPENBOOK_PROPS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPENBOOK_PROPS, ofaOpenbookPropsClass ))

typedef struct _ofaOpenbookProps               ofaOpenbookProps;

struct _ofaOpenbookProps {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaOpenbookPropsClass;

GType             ofa_openbook_props_get_type            ( void ) G_GNUC_CONST;

ofaOpenbookProps *ofa_openbook_props_new                 ( ofaHub *hub );

ofaOpenbookProps *ofa_openbook_props_new_from_string     ( ofaHub *hub,
																const gchar *string );

const gchar      *ofa_openbook_props_get_openbook_version( ofaOpenbookProps *props );

void              ofa_openbook_props_set_openbook_version( ofaOpenbookProps *props,
																const gchar *version );

void              ofa_openbook_props_set_plugin          ( ofaOpenbookProps *props,
																const gchar *canon_name,
																const gchar *display_name,
																const gchar *version );

void              ofa_openbook_props_set_dbmodel         ( ofaOpenbookProps *props,
																const gchar *id,
																const gchar *version );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_OPENBOOK_PROPS_H__ */
