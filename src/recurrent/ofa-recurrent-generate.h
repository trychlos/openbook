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

#ifndef __REC_GENERATE_H__
#define __REC_GENERATE_H__

/**
 * SECTION: ofa_recurrent_generate
 * @short_description: #ofaRecurrentGenerate class definition.
 * @include: recurrent/ofa-recurrent-generate.h
 *
 * Let the user validate the generated operations before recording.
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, after having displayed a success or an error message
 * box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: yes
 * - settings:           yes
 * - current:            no
 *
 * Because this dialog is not modal, the user may dynamically change
 * the selection in #ofaRecurrentModelPage page, and try to re-generate
 * new recurrent operations with the new selection.
 */

#include "api/ofa-igetter-def.h"

#include "recurrent/ofa-recurrent-model-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_GENERATE                ( ofa_recurrent_generate_get_type())
#define OFA_RECURRENT_GENERATE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_GENERATE, ofaRecurrentGenerate ))
#define REC_GENERATE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_GENERATE, ofaRecurrentGenerateClass ))
#define OFA_IS_RECURRENT_GENERATE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_GENERATE ))
#define OFA_IS_RECURRENT_GENERATE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_GENERATE ))
#define REC_GENERATE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_GENERATE, ofaRecurrentGenerateClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaRecurrentGenerate;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaRecurrentGenerateClass;

GType ofa_recurrent_generate_get_type( void ) G_GNUC_CONST;

void  ofa_recurrent_generate_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										ofaRecurrentModelPage *page );

G_END_DECLS

#endif /* __REC_GENERATE_H__ */
