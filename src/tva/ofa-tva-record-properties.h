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

#ifndef __OFA_TVA_RECORD_PROPERTIES_H__
#define __OFA_TVA_RECORD_PROPERTIES_H__

/**
 * SECTION: ofa_tva_record_properties
 * @short_description: #ofaTVARecordProperties class definition.
 * @include: ui/ofa-tva-record-properties.h
 *
 * Display/update a tva declaration.
 *
 * We are running this dialog either with an already recorded
 * declaration which is here displayed or updated, or with a new
 * declaration from a form. This new declaration is nonetheless
 * already recorded, with an end date.
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, maybe after having displayed an error message box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: no
 * - settings:           yes
 * - current:            yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

#include "tva/ofo-tva-record.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_RECORD_PROPERTIES                ( ofa_tva_record_properties_get_type())
#define OFA_TVA_RECORD_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_RECORD_PROPERTIES, ofaTVARecordProperties ))
#define OFA_TVA_RECORD_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_RECORD_PROPERTIES, ofaTVARecordPropertiesClass ))
#define OFA_IS_TVA_RECORD_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_RECORD_PROPERTIES ))
#define OFA_IS_TVA_RECORD_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_RECORD_PROPERTIES ))
#define OFA_TVA_RECORD_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_RECORD_PROPERTIES, ofaTVARecordPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaTVARecordProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaTVARecordPropertiesClass;

GType ofa_tva_record_properties_get_type( void ) G_GNUC_CONST;

void  ofa_tva_record_properties_run     ( ofaIGetter *getter,
												GtkWindow *parent,
												ofoTVARecord *record );

G_END_DECLS

#endif /* __OFA_TVA_RECORD_PROPERTIES_H__ */
