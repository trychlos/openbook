/*
 * Open Firm Paimeaning
 * A double-entry paimeaning application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Paimeaning is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Paimeaning is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Paimeaning; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFA_PAIMEAN_FRAME_BIN_H__
#define __OFA_PAIMEAN_FRAME_BIN_H__

/**
 * SECTION: ofa_paimean_frame_bin
 * @short_description: #ofaPaimeanFrameBin class definition.
 * @include: ui/ofa-paimean-frame-bin.h
 *
 * This is a convenience class which manages both the paimean treeview
 * and the actions box on the right.
 *
 * The class also acts as a proxy for "ofa-accchanged" and
 * "ofa-accactivated" messages sent by the ofaPaimeanTreeview's views.
 * It relays these messages as:
 * - 'ofa-changed' when the selection changes
 * - 'ofa-activated' when the selection is activated.
 *
 * At time, #ofaPaimeanFrameBin composite widget is used by
 * #ofaPaimeanPage page and by #ofaPaimeanSelect selection dialog box.
 */

#include <gtk/gtk.h>

#include "api/ofa-buttons-box.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PAIMEAN_FRAME_BIN                ( ofa_paimean_frame_bin_get_type())
#define OFA_PAIMEAN_FRAME_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAIMEAN_FRAME_BIN, ofaPaimeanFrameBin ))
#define OFA_PAIMEAN_FRAME_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PAIMEAN_FRAME_BIN, ofaPaimeanFrameBinClass ))
#define OFA_IS_PAIMEAN_FRAME_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAIMEAN_FRAME_BIN ))
#define OFA_IS_PAIMEAN_FRAME_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PAIMEAN_FRAME_BIN ))
#define OFA_PAIMEAN_FRAME_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAIMEAN_FRAME_BIN, ofaPaimeanFrameBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaPaimeanFrameBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaPaimeanFrameBinClass;

GType               ofa_paimean_frame_bin_get_type     ( void ) G_GNUC_CONST;

ofaPaimeanFrameBin *ofa_paimean_frame_bin_new          ( ofaIGetter *getter,
																const gchar *settings_prefix );

GtkWidget          *ofa_paimean_frame_bin_get_tree_view( ofaPaimeanFrameBin *bin );

ofoPaimean         *ofa_paimean_frame_bin_get_selected ( ofaPaimeanFrameBin *bin );

void                ofa_paimean_frame_bin_set_selected ( ofaPaimeanFrameBin *bin,
																const gchar *code );

G_END_DECLS

#endif /* __OFA_PAIMEAN_FRAME_BIN_H__ */
