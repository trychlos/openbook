/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OFA_OPE_TEMPLATE_FRAME_BIN_H__
#define __OFA_OPE_TEMPLATE_FRAME_BIN_H__

/**
 * SECTION: ofa_ope_template_frame_bin
 * @short_description: #ofaOpeTemplateFrameBin class definition.
 * @include: ui/ofa-ope-template-frame-bin.h
 *
 * This is a convenience class which manages both the operation templates
 * notebook and the buttons box on the right.
 *
 * The class also acts as a proxy for "changed" and "activated" messages
 * sent by the underlying ofaOpeTemplateStore class.
 */

#include "ui/ofa-main-window-def.h"
#include "ui/ofa-ope-template-book-bin.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_FRAME_BIN                ( ofa_ope_template_frame_bin_get_type())
#define OFA_OPE_TEMPLATE_FRAME_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBin ))
#define OFA_OPE_TEMPLATE_FRAME_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBinClass ))
#define OFA_IS_OPE_TEMPLATE_FRAME_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_FRAME_BIN ))
#define OFA_IS_OPE_TEMPLATE_FRAME_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_FRAME_BIN ))
#define OFA_OPE_TEMPLATE_FRAME_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_FRAME_BIN, ofaOpeTemplateFrameBinClass ))

typedef struct _ofaOpeTemplateFrameBinPrivate          ofaOpeTemplateFrameBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                         parent;

	/*< private members >*/
	ofaOpeTemplateFrameBinPrivate *priv;
}
	ofaOpeTemplateFrameBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                    parent;
}
	ofaOpeTemplateFrameBinClass;

GType                   ofa_ope_template_frame_bin_get_type   ( void ) G_GNUC_CONST;

ofaOpeTemplateFrameBin *ofa_ope_template_frame_bin_new        ( const ofaMainWindow *main_window );

void                    ofa_ope_template_frame_bin_set_buttons( ofaOpeTemplateFrameBin *bin,
																			gboolean guided_input );

ofaOpeTemplateBookBin    *ofa_ope_template_frame_bin_get_book ( ofaOpeTemplateFrameBin *bin );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_FRAME_BIN_H__ */
