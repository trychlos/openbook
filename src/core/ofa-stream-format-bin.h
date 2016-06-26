/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_STREAM_FORMAT_BIN_H__
#define __OFA_STREAM_FORMAT_BIN_H__

/**
 * SECTION: ofa_stream_format_bin
 * @short_description: #ofaStreamFormatBin class definition.
 * @include: core/ofa-stream-format-bin.h
 *
 * A convenience class which let the user manages its own export
 * and import settings. It is to be used as a #GtkBin in user
 * preferences.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include "api/ofa-stream-format.h"

G_BEGIN_DECLS

#define OFA_TYPE_STREAM_FORMAT_BIN                ( ofa_stream_format_bin_get_type())
#define OFA_STREAM_FORMAT_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_STREAM_FORMAT_BIN, ofaStreamFormatBin ))
#define OFA_STREAM_FORMAT_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_STREAM_FORMAT_BIN, ofaStreamFormatBinClass ))
#define OFA_IS_STREAM_FORMAT_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_STREAM_FORMAT_BIN ))
#define OFA_IS_STREAM_FORMAT_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_STREAM_FORMAT_BIN ))
#define OFA_STREAM_FORMAT_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_STREAM_FORMAT_BIN, ofaStreamFormatBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaStreamFormatBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaStreamFormatBinClass;

GType               ofa_stream_format_bin_get_type          ( void ) G_GNUC_CONST;

ofaStreamFormatBin *ofa_stream_format_bin_new               ( ofaStreamFormat *format );

GtkSizeGroup       *ofa_stream_format_bin_get_size_group    ( ofaStreamFormatBin *bin,
																guint col_number );

GtkWidget          *ofa_stream_format_bin_get_name_entry    ( ofaStreamFormatBin *bin );

void                ofa_stream_format_bin_set_mode_sensitive( ofaStreamFormatBin *bin,
																	gboolean sensitive );

void                ofa_stream_format_bin_set_format        ( ofaStreamFormatBin *bin,
																ofaStreamFormat *format );

void                ofa_stream_format_bin_set_updatable     ( ofaStreamFormatBin *bin,
																gboolean updatable );

gboolean            ofa_stream_format_bin_is_valid          ( ofaStreamFormatBin *bin,
																gchar **error_message );

gboolean            ofa_stream_format_bin_apply             ( ofaStreamFormatBin *bin );

G_END_DECLS

#endif /* __OFA_STREAM_FORMAT_BIN_H__ */
