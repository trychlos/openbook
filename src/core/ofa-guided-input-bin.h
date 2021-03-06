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

#ifndef __OFA_GUIDED_INPUT_BIN_H__
#define __OFA_GUIDED_INPUT_BIN_H__

/**
 * SECTION: ofa_guided_input_bin
 * @short_description: #ofaGuidedInputBin class definition.
 * @include: core/ofa-guided-input-bin.h
 *
 * A convenience class which factorize the code common to both
 * ofaGuidedInput dialog box and ofaGuidedEx main page.
 *
 * Behaviors:
 * - dialog box, is opened with a model, take input, validate, and close
 * - main page: is opened first, then receives a model, take input,
 *   validate the entries, and stays opened (though resetted); then
 *   receives another model, and so on
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_GUIDED_INPUT_BIN                ( ofa_guided_input_bin_get_type())
#define OFA_GUIDED_INPUT_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_GUIDED_INPUT_BIN, ofaGuidedInputBin ))
#define OFA_GUIDED_INPUT_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_GUIDED_INPUT_BIN, ofaGuidedInputBinClass ))
#define OFA_IS_GUIDED_INPUT_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_GUIDED_INPUT_BIN ))
#define OFA_IS_GUIDED_INPUT_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_GUIDED_INPUT_BIN ))
#define OFA_GUIDED_INPUT_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_GUIDED_INPUT_BIN, ofaGuidedInputBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaGuidedInputBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaGuidedInputBinClass;

GType              ofa_guided_input_bin_get_type        ( void ) G_GNUC_CONST;

ofaGuidedInputBin *ofa_guided_input_bin_new             ( ofaIGetter *getter );

void               ofa_guided_input_bin_set_ope_template( ofaGuidedInputBin *bin,
																ofoOpeTemplate *template );

gboolean           ofa_guided_input_bin_is_valid        ( ofaGuidedInputBin *bin );

gboolean           ofa_guided_input_bin_apply           ( ofaGuidedInputBin *bin );

void               ofa_guided_input_bin_reset           ( ofaGuidedInputBin *bin );

G_END_DECLS

#endif /* __OFA_GUIDED_INPUT_BIN_H__ */
