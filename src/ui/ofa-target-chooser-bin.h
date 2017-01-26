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

#ifndef __OFA_TARGET_CHOOSER_BIN_H__
#define __OFA_TARGET_CHOOSER_BIN_H__

/**
 * SECTION: ofa_target_chooser_bin
 * @short_description: #ofaTargetChooserBin class definition.
 * @include: ui/ofa-target-chooser-bin.h
 *
 * Let the user select a dossier or create a new one.
 * Let the user select a period, or create a new one.
 *
 * The class sends the 'ofa-changed' signal on any change.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_TARGET_CHOOSER_BIN                ( ofa_target_chooser_bin_get_type())
#define OFA_TARGET_CHOOSER_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TARGET_CHOOSER_BIN, ofaTargetChooserBin ))
#define OFA_TARGET_CHOOSER_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TARGET_CHOOSER_BIN, ofaTargetChooserBinClass ))
#define OFA_IS_TARGET_CHOOSER_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TARGET_CHOOSER_BIN ))
#define OFA_IS_TARGET_CHOOSER_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TARGET_CHOOSER_BIN ))
#define OFA_TARGET_CHOOSER_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TARGET_CHOOSER_BIN, ofaTargetChooserBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaTargetChooserBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaTargetChooserBinClass;

GType                ofa_target_chooser_bin_get_type       ( void ) G_GNUC_CONST;

ofaTargetChooserBin *ofa_target_chooser_bin_new            ( ofaIGetter *getter,
																const gchar *settings_prefix );

gboolean             ofa_target_chooser_bin_is_new_dossier ( ofaTargetChooserBin *bin,
																ofaIDBDossierMeta *dossier_meta );

gboolean             ofa_target_chooser_bin_is_new_exercice( ofaTargetChooserBin *bin,
																ofaIDBExerciceMeta *exercice_meta );

void                 ofa_target_chooser_bin_set_selected   ( ofaTargetChooserBin *bin,
																ofaIDBDossierMeta *dossier_meta,
																ofaIDBExerciceMeta *exercice_meta );

G_END_DECLS

#endif /* __OFA_TARGET_CHOOSER_BIN_H__ */
