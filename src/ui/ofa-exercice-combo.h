/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_EXERCICE_COMBO_H__
#define __OFA_EXERCICE_COMBO_H__

/**
 * SECTION: ofa_exercice_combo
 * @short_description: #ofaExerciceCombo class definition.
 * @include: ui/ofa-exercice-combo.h
 *
 * This class manages a combobox which display the known exercices.
 *
 * It sends an 'ofa-changed' message on selection change.
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbexercice-meta-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_COMBO                ( ofa_exercice_combo_get_type())
#define OFA_EXERCICE_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_COMBO, ofaExerciceCombo ))
#define OFA_EXERCICE_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_COMBO, ofaExerciceComboClass ))
#define OFA_IS_EXERCICE_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_COMBO ))
#define OFA_IS_EXERCICE_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_COMBO ))
#define OFA_EXERCICE_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_COMBO, ofaExerciceComboClass ))

typedef struct {
	/*< public members >*/
	GtkComboBox      parent;
}
	ofaExerciceCombo;

typedef struct {
	/*< public members >*/
	GtkComboBoxClass parent;
}
	ofaExerciceComboClass;

GType             ofa_exercice_combo_get_type    ( void ) G_GNUC_CONST;

ofaExerciceCombo *ofa_exercice_combo_new         ( ofaIGetter *getter );

void              ofa_exercice_combo_set_dossier ( ofaExerciceCombo *combo,
															ofaIDBDossierMeta *meta );

void              ofa_exercice_combo_set_selected( ofaExerciceCombo *combo,
															ofaIDBExerciceMeta *period );

G_END_DECLS

#endif /* __OFA_EXERCICE_COMBO_H__ */
