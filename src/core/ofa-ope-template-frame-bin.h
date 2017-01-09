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

#ifndef __OFA_OPE_TEMPLATE_FRAME_BIN_H__
#define __OFA_OPE_TEMPLATE_FRAME_BIN_H__

/**
 * SECTION: ofa_ope_template_frame_bin
 * @short_description: #ofaOpeTemplateFrameBin class definition.
 * @include: core/ofa-ope-template-frame-bin.h
 *
 * This is a convenience class which manages both the operation templates
 * notebook and the buttons box on the right.
 *
 * The class also acts as a proxy for "ofa-opechanged" and
 * "ofa-opeactivated" messages sent by the ofaOpeTemplateTreeview's views.
 * It relays these messages as:
 * - 'ofa-changed' when the selection changes
 * - 'ofa-activated' when the selection is activated.
 *
 * See api/ofo-ope-template.h for a full description of the model language.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofa-ope-template-store.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_FRAME_BIN                ( ofa_ope_template_frame_bin_get_type())
#define OFA_OPE_TEMPLATE_FRAME_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBin ))
#define OFA_OPE_TEMPLATE_FRAME_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBinClass ))
#define OFA_IS_OPE_TEMPLATE_FRAME_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN ))
#define OFA_IS_OPE_TEMPLATE_FRAME_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_FRAME_BIN ))
#define OFA_OPE_TEMPLATE_FRAME_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaOpeTemplateFrameBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaOpeTemplateFrameBinClass;

/**
 * ofeOpeTemplateFrameBtn:
 *
 * These are the actions that the frame is able to manage.
 * It is up to the caller to set the desired actions.
 * Default is none.
 */
typedef enum {
	TEMPLATE_ACTION_SPACER = 1,
	TEMPLATE_ACTION_NEW,
	TEMPLATE_ACTION_PROPERTIES,
	TEMPLATE_ACTION_DELETE,
	TEMPLATE_ACTION_DUPLICATE,
	TEMPLATE_ACTION_GUIDED_INPUT
}
	ofeOpeTemplateAction;

GType                   ofa_ope_template_frame_bin_get_type          ( void ) G_GNUC_CONST;

ofaOpeTemplateFrameBin *ofa_ope_template_frame_bin_new               ( ofaIGetter *getter );

void                    ofa_ope_template_frame_bin_add_action        ( ofaOpeTemplateFrameBin *bin,
																			ofeOpeTemplateAction id );

GtkWidget              *ofa_ope_template_frame_bin_get_current_page  ( ofaOpeTemplateFrameBin *bin );

GList                  *ofa_ope_template_frame_bin_get_pages_list    ( ofaOpeTemplateFrameBin *bin );

ofoOpeTemplate         *ofa_ope_template_frame_bin_get_selected      ( ofaOpeTemplateFrameBin *bin );

void                    ofa_ope_template_frame_bin_set_selected      ( ofaOpeTemplateFrameBin *bin,
																			const gchar *mnemo );

void                    ofa_ope_template_frame_bin_set_settings_key  ( ofaOpeTemplateFrameBin *bin,
																			const gchar *key );

void                    ofa_ope_template_frame_bin_load_dataset      ( ofaOpeTemplateFrameBin *bin );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_FRAME_BIN_H__ */
