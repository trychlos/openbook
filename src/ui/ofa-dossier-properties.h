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

#ifndef __OFA_DOSSIER_PROPERTIES_H__
#define __OFA_DOSSIER_PROPERTIES_H__

/**
 * SECTION: ofa_dossier_properties
 * @short_description: #ofaDossierProperties class definition.
 * @include: ui/ofa-dossier-properties.h
 *
 * Display/update the dossier properties.
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, maybe after having displayed an error message box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: no
 * - settings:           no
 * - current:            yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_PROPERTIES                ( ofa_dossier_properties_get_type())
#define OFA_DOSSIER_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_PROPERTIES, ofaDossierProperties ))
#define OFA_DOSSIER_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_PROPERTIES, ofaDossierPropertiesClass ))
#define OFA_IS_DOSSIER_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_PROPERTIES ))
#define OFA_IS_DOSSIER_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_PROPERTIES ))
#define OFA_DOSSIER_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_PROPERTIES, ofaDossierPropertiesClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaDossierProperties;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaDossierPropertiesClass;

/**
 * The key name for background image in dossier settings.
 */
#define DOSSIER_BACKGROUND_KEY          "ofa-BackgroundImage"

GType ofa_dossier_properties_get_type( void ) G_GNUC_CONST;

void  ofa_dossier_properties_run     ( ofaIGetter *getter,
											GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_DOSSIER_PROPERTIES_H__ */
