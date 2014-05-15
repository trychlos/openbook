/*
 * Open Freelance ModFamilying
 * A double-entry mod_familying application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance ModFamilying is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance ModFamilying is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance ModFamilying; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 *
 * $Id$
 */

#ifndef __OFA_MOD_FAMILY_PROPERTIES_H__
#define __OFA_MOD_FAMILY_PROPERTIES_H__

/**
 * SECTION: ofa_mod_family_properties
 * @short_description: #ofaModFamilyProperties class definition.
 * @include: ui/ofa-mod-family-properties.h
 *
 * Update the properties of the family of the entry model.
 * mainly label and notes
 */

#include "ui/ofa-main-window.h"
#include "ui/ofo-mod-family.h"

G_BEGIN_DECLS

#define OFA_TYPE_MOD_FAMILY_PROPERTIES                ( ofa_mod_family_properties_get_type())
#define OFA_MOD_FAMILY_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MOD_FAMILY_PROPERTIES, ofaModFamilyProperties ))
#define OFA_MOD_FAMILY_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MOD_FAMILY_PROPERTIES, ofaModFamilyPropertiesClass ))
#define OFA_IS_MOD_FAMILY_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MOD_FAMILY_PROPERTIES ))
#define OFA_IS_MOD_FAMILY_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MOD_FAMILY_PROPERTIES ))
#define OFA_MOD_FAMILY_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MOD_FAMILY_PROPERTIES, ofaModFamilyPropertiesClass ))

typedef struct _ofaModFamilyPropertiesPrivate        ofaModFamilyPropertiesPrivate;

typedef struct {
	/*< private >*/
	GObject                        parent;
	ofaModFamilyPropertiesPrivate *private;
}
	ofaModFamilyProperties;

typedef struct _ofaModFamilyPropertiesClassPrivate   ofaModFamilyPropertiesClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass                        parent;
	ofaModFamilyPropertiesClassPrivate *private;
}
	ofaModFamilyPropertiesClass;

GType    ofa_mod_family_properties_get_type( void );

gboolean ofa_mod_family_properties_run     ( ofaMainWindow *parent, ofoModFamily *family );

G_END_DECLS

#endif /* __OFA_MOD_FAMILY_PROPERTIES_H__ */
