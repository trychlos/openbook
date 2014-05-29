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

#ifndef __OFA_DEVISE_PROPERTIES_H__
#define __OFA_DEVISE_PROPERTIES_H__

/**
 * SECTION: ofa_devise_properties
 * @short_description: #ofaDeviseProperties class definition.
 * @include: ui/ofa-devise-properties.h
 *
 * Update the devise properties.
 */

#include "ui/ofa-base-dialog.h"
#include "ui/ofa-main-window-def.h"
#include "ui/ofo-devise-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DEVISE_PROPERTIES                ( ofa_devise_properties_get_type())
#define OFA_DEVISE_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DEVISE_PROPERTIES, ofaDeviseProperties ))
#define OFA_DEVISE_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DEVISE_PROPERTIES, ofaDevisePropertiesClass ))
#define OFA_IS_DEVISE_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DEVISE_PROPERTIES ))
#define OFA_IS_DEVISE_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DEVISE_PROPERTIES ))
#define OFA_DEVISE_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DEVISE_PROPERTIES, ofaDevisePropertiesClass ))

typedef struct _ofaDevisePropertiesPrivate        ofaDevisePropertiesPrivate;

typedef struct {
	/*< private >*/
	ofaBaseDialog               parent;
	ofaDevisePropertiesPrivate *private;
}
	ofaDeviseProperties;

typedef struct {
	/*< private >*/
	ofaBaseDialogClass parent;
}
	ofaDevisePropertiesClass;

GType    ofa_devise_properties_get_type( void );

gboolean ofa_devise_properties_run     ( ofaMainWindow *parent, ofoDevise *devise );

G_END_DECLS

#endif /* __OFA_DEVISE_PROPERTIES_H__ */
