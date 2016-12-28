/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_EXERCICE_META_BIN_H__
#define __OFA_EXERCICE_META_BIN_H__

/**
 * SECTION: ofa_exercice_meta_bin
 * @short_description: #ofaExerciceMetaBin class definition.
 * @include: ui/ofa-exercice-meta-bin.h
 *
 * Let the user define a new exercice.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXERCICE_META_BIN                ( ofa_exercice_meta_bin_get_type())
#define OFA_EXERCICE_META_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXERCICE_META_BIN, ofaExerciceMetaBin ))
#define OFA_EXERCICE_META_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXERCICE_META_BIN, ofaExerciceMetaBinClass ))
#define OFA_IS_EXERCICE_META_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXERCICE_META_BIN ))
#define OFA_IS_EXERCICE_META_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXERCICE_META_BIN ))
#define OFA_EXERCICE_META_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXERCICE_META_BIN, ofaExerciceMetaBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaExerciceMetaBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaExerciceMetaBinClass;

GType               ofa_exercice_meta_bin_get_type         ( void ) G_GNUC_CONST;

ofaExerciceMetaBin *ofa_exercice_meta_bin_new              ( ofaHub *hub,
																	const gchar *settings_prefix,
																	guint rule);

GtkSizeGroup       *ofa_exercice_meta_bin_get_size_group   ( ofaExerciceMetaBin *bin,
																	guint column );

gboolean            ofa_exercice_meta_bin_is_valid         ( ofaExerciceMetaBin *bin,
																	gchar **error_message );

gboolean            ofa_exercice_meta_bin_apply            ( ofaExerciceMetaBin *bin );

const GDate        *ofa_exercice_meta_bin_get_begin_date   ( ofaExerciceMetaBin *bin );

const GDate        *ofa_exercice_meta_bin_get_end_date     ( ofaExerciceMetaBin *bin );

gboolean            ofa_exercice_meta_bin_get_is_current   ( ofaExerciceMetaBin *bin );

G_END_DECLS

#endif /* __OFA_EXERCICE_META_BIN_H__ */
