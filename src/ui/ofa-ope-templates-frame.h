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
 *
 */

#ifndef __OFA_OPE_TEMPLATES_FRAME_H__
#define __OFA_OPE_TEMPLATES_FRAME_H__

/**
 * SECTION: ofa_ope_templates_frame
 * @short_description: #ofaOpeTemplatesFrame class definition.
 * @include: ui/ofa-ope-templates-frame.h
 *
 * This is a convenience class which manages both the operation templates
 * notebook and the buttons box on the right.
 *
 * The class also acts as a proxy for "changed" and "activated" messages
 * sent by the underlying ofaOpeTemplateStore class.
 */

#include <gtk/gtk.h>

#include "core/ofa-main-window-def.h"

#include "ui/ofa-ope-templates-book.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATES_FRAME                ( ofa_ope_templates_frame_get_type())
#define OFA_OPE_TEMPLATES_FRAME( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATES_FRAME, ofaOpeTemplatesFrame ))
#define OFA_OPE_TEMPLATES_FRAME_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATES_FRAME, ofaOpeTemplatesFrameClass ))
#define OFA_IS_OPE_TEMPLATES_FRAME( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATES_FRAME ))
#define OFA_IS_OPE_TEMPLATES_FRAME_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATES_FRAME ))
#define OFA_OPE_TEMPLATES_FRAME_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATES_FRAME, ofaOpeTemplatesFrameClass ))

typedef struct _ofaOpeTemplatesFramePrivate         ofaOpeTemplatesFramePrivate;

typedef struct {
	/*< public members >*/
	GtkBin                       parent;

	/*< private members >*/
	ofaOpeTemplatesFramePrivate *priv;
}
	ofaOpeTemplatesFrame;

typedef struct {
	/*< public members >*/
	GtkBinClass                  parent;
}
	ofaOpeTemplatesFrameClass;

GType                 ofa_ope_templates_frame_get_type       ( void ) G_GNUC_CONST;

ofaOpeTemplatesFrame *ofa_ope_templates_frame_new            ( void );

void                  ofa_ope_templates_frame_set_main_window( ofaOpeTemplatesFrame *frame,
																		ofaMainWindow *main_window );

void                  ofa_ope_templates_frame_set_buttons    ( ofaOpeTemplatesFrame *frame,
																		gboolean guided_input );

ofaOpeTemplatesBook  *ofa_ope_templates_frame_get_book       ( ofaOpeTemplatesFrame *frame );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATES_FRAME_H__ */
