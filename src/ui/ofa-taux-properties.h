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

#ifndef __OFA_TAUX_PROPERTIES_H__
#define __OFA_TAUX_PROPERTIES_H__

/**
 * SECTION: ofa_taux_properties
 * @short_description: #ofaTauxProperties class definition.
 * @include: ui/ofa-taux-properties.h
 *
 * Update the taux properties.
 *
 * From the ofaTauxSet page, create a new taux, or update an existing
 * one. in the two cases, zero, one or more validities can be created,
 * updated, deleted.
 *
 * The content of the provided ofoTaux object is not modified until the
 * do_update() function. At this time, all its content is _replaced_
 * with which is found in the dialog box.
 *
 * When creating a new validity, we take care of checking that it
 * doesn't override an already existing validity period.
 *
 * Examples:
 * Existing validity
 * (null)     (null)   impossible to create a new period because there
 *                     is no place
 * (null)   31/12/2013 it is possible to create a new period starting
 *                     with 01/01/2014
 */

#include "ui/my-dialog.h"
#include "api/ofo-taux-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_TAUX_PROPERTIES                ( ofa_taux_properties_get_type())
#define OFA_TAUX_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TAUX_PROPERTIES, ofaTauxProperties ))
#define OFA_TAUX_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TAUX_PROPERTIES, ofaTauxPropertiesClass ))
#define OFA_IS_TAUX_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TAUX_PROPERTIES ))
#define OFA_IS_TAUX_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TAUX_PROPERTIES ))
#define OFA_TAUX_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TAUX_PROPERTIES, ofaTauxPropertiesClass ))

typedef struct _ofaTauxPropertiesPrivate        ofaTauxPropertiesPrivate;

typedef struct {
	/*< private >*/
	myDialog                  parent;
	ofaTauxPropertiesPrivate *private;
}
	ofaTauxProperties;

typedef struct {
	/*< private >*/
	myDialogClass parent;
}
	ofaTauxPropertiesClass;

GType    ofa_taux_properties_get_type( void ) G_GNUC_CONST;

gboolean ofa_taux_properties_run     ( ofaMainWindow *parent, ofoTaux *taux );

G_END_DECLS

#endif /* __OFA_TAUX_PROPERTIES_H__ */
