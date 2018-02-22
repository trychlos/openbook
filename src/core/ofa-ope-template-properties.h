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

#ifndef __OFA_OPE_TEMPLATE_PROPERTIES_H__
#define __OFA_OPE_TEMPLATE_PROPERTIES_H__

/**
 * SECTION: ofa_ope_template_properties
 * @short_description: #ofaOpeTemplateProperties class definition.
 * @include: core/ofa-ope-template-properties.h
 *
 * Update the model properties.
 *
 * See api/ofo-ope-template.h for a full description of the model language.
 *
 * Development rules:
 * - type:       per-mnemo modal/non-modal dialog
 * - settings:   yes
 * - current:    yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_PROPERTIES                ( ofa_ope_template_properties_get_type())
#define OFA_OPE_TEMPLATE_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_PROPERTIES, ofaOpeTemplateProperties ))
#define OFA_OPE_TEMPLATE_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_PROPERTIES, ofaOpeTemplatePropertiesClass ))
#define OFA_IS_OPE_TEMPLATE_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_PROPERTIES ))
#define OFA_IS_OPE_TEMPLATE_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_PROPERTIES ))
#define OFA_OPE_TEMPLATE_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_PROPERTIES, ofaOpeTemplatePropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaOpeTemplateProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaOpeTemplatePropertiesClass;

GType ofa_ope_template_properties_get_type( void ) G_GNUC_CONST;

void  ofa_ope_template_properties_run     ( ofaIGetter *getter,
												GtkWindow *parent,
												ofoOpeTemplate *template,
												const gchar *ledger );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_PROPERTIES_H__ */
