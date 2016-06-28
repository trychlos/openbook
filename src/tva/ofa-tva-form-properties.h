/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_TVA_FORM_PROPERTIES_H__
#define __OFA_TVA_FORM_PROPERTIES_H__

/**
 * SECTION: ofa_tva_form_properties
 * @short_description: #ofaTVAFormProperties class definition.
 * @include: ui/ofa-tva_form-properties.h
 *
 * Create a new/Update the tva_form properties.
 *
 * From the ofaTVAManage page, create a new tva_form, or update an existing
 * one.
 *
 * The content of the provided ofoTVAForm object is not modified until the
 * do_update() function. At this time, all its content is _replaced_
 * with which is found in the dialog box.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   yes
 * - current:    yes
 */

#include "api/ofa-igetter-def.h"

#include "tva/ofo-tva-form.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_FORM_PROPERTIES                ( ofa_tva_form_properties_get_type())
#define OFA_TVA_FORM_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_FORM_PROPERTIES, ofaTVAFormProperties ))
#define OFA_TVA_FORM_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_FORM_PROPERTIES, ofaTVAFormPropertiesClass ))
#define OFA_IS_TVA_FORM_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_FORM_PROPERTIES ))
#define OFA_IS_TVA_FORM_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_FORM_PROPERTIES ))
#define OFA_TVA_FORM_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_FORM_PROPERTIES, ofaTVAFormPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaTVAFormProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaTVAFormPropertiesClass;

GType ofa_tva_form_properties_get_type( void ) G_GNUC_CONST;

void  ofa_tva_form_properties_run     ( ofaIGetter *getter,
											GtkWindow *parent,
											ofoTVAForm *form );

G_END_DECLS

#endif /* __OFA_TVA_FORM_PROPERTIES_H__ */
