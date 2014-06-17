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

#ifndef __OFA_JOURNAL_COMBO_H__
#define __OFA_JOURNAL_COMBO_H__

/**
 * SECTION: ofa_journal_combo
 * @short_description: #ofaJournalCombo class definition.
 * @include: ui/ofa-journal-combo.h
 *
 * A class to embed a Journals combobox in a dialog.
 */

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_JOURNAL_COMBO                ( ofa_journal_combo_get_type())
#define OFA_JOURNAL_COMBO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_JOURNAL_COMBO, ofaJournalCombo ))
#define OFA_JOURNAL_COMBO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_JOURNAL_COMBO, ofaJournalComboClass ))
#define OFA_IS_JOURNAL_COMBO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_JOURNAL_COMBO ))
#define OFA_IS_JOURNAL_COMBO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_JOURNAL_COMBO ))
#define OFA_JOURNAL_COMBO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_JOURNAL_COMBO, ofaJournalComboClass ))

typedef struct _ofaJournalComboPrivate        ofaJournalComboPrivate;

typedef struct {
	/*< private >*/
	GObject                 parent;
	ofaJournalComboPrivate *private;
}
	ofaJournalCombo;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaJournalComboClass;

/**
 * ofaJournalComboCb:
 *
 * A callback to be triggered when a new journal is selected.
 *
 * Passed parameters are:
 * - mnemo
 * - user_data provided at init_dialog() time
 */
typedef void ( *ofaJournalComboCb )( const gchar *, gpointer );

GType            ofa_journal_combo_get_type   ( void ) G_GNUC_CONST;

/**
 * ofaJournalComboParms
 *
 * The structure passed to the init_dialog() function.
 *
 * @container: the parent container of the target combo box
 * @dossier: the current opened ofoDossier
 * @combo_name: the name of the GtkComboBox widget
 * @label_name: [allow-none]: the name of a GtkLabel widget which will
 *  receive the label of the selected journal each time the selection
 *  changes
 * @disp_mnemo: whether the combo box should display the mnemo
 * @disp_label: whether the combo box should display the label
 * @pfn: [allow-none]: a user-provided callback which will be triggered
 *  on each selection change
 * @user_data: user-data passed to the callback
 * @initial_mnemo: the journal identifier of the initial selection, or
 *  NULL
 */
typedef struct {
	GtkContainer     *container;
	ofoDossier       *dossier;
	const gchar      *combo_name;
	const gchar      *label_name;
	gboolean          disp_mnemo;
	gboolean          disp_label;
	ofaJournalComboCb pfn;
	gpointer          user_data;
	const gchar      *initial_mnemo;
}
	ofaJournalComboParms;

ofaJournalCombo *ofa_journal_combo_new          ( const ofaJournalComboParms *parms );

gint             ofa_journal_combo_get_selection( ofaJournalCombo *self, gchar **mnemo, gchar **label );

void             ofa_journal_combo_set_selection( ofaJournalCombo *self, const gchar *mnemo );

G_END_DECLS

#endif /* __OFA_JOURNAL_COMBO_H__ */
