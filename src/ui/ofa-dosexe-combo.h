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

#ifndef __OFA_DOSEXE_COMBO_H__
#define __OFA_DOSEXE_COMBO_H__

/**
 * SECTION: ofa_dosexe_combo
 * @short_description: #ofaDosexeCombo class definition.
 * @include: ui/ofa-dosexe-combo.h
 *
 * A class to manage a combobox which displays the archived and current
 * exercices for a dossier.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSEXE_COMBO                ( ofa_dosexe_combo_get_type())
#define OFA_DOSEXE_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSEXE_COMBO, ofaDosexeCombo ))
#define OFA_DOSEXE_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSEXE_COMBO, ofaDosexeComboClass ))
#define OFA_IS_DOSEXE_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSEXE_COMBO ))
#define OFA_IS_DOSEXE_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSEXE_COMBO ))
#define OFA_DOSEXE_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSEXE_COMBO, ofaDosexeComboClass ))

typedef struct _ofaDosexeComboPrivate        ofaDosexeComboPrivate;

typedef struct {
	/*< public members >*/
	GObject                parent;

	/*< private members >*/
	ofaDosexeComboPrivate *priv;
}
	ofaDosexeCombo;

typedef struct {
	/*< public members >*/
	GObjectClass           parent;
}
	ofaDosexeComboClass;

/**
 * DOSEXE_SIGNAL_EXE_SELECTED:
 * Signal sent on exercice selection in the combobox.
 */
#define DOSEXE_SIGNAL_EXE_SELECTED      "ofa-signal-exercice-selected"

/**
 * ofaDosexeComboParms
 *
 * The structure passed to the ofa_dosexe_combo_new() function.
 *
 * @container: the parent #GtkContainer of the target combo box
 * @combo_name: the name of the #GtkComboBox widget
 * @main_window: the main window
 * @exe_id: the exercice identifier to be initially selected, or -1.
 */
typedef struct {
	GtkContainer  *container;
	const gchar   *combo_name;
	ofaMainWindow *main_window;
	gint           exe_id;
}
	ofaDosexeComboParms;

GType           ofa_dosexe_combo_get_type     ( void ) G_GNUC_CONST;

ofaDosexeCombo *ofa_dosexe_combo_new          ( const ofaDosexeComboParms *parms );

G_END_DECLS

#endif /* __OFA_DOSEXE_COMBO_H__ */
