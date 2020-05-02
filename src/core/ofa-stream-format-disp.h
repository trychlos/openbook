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

#ifndef __OFA_STREAM_FORMAT_DISP_H__
#define __OFA_STREAM_FORMAT_DISP_H__

/**
 * SECTION: ofa_stream_format_disp
 * @short_description: #ofaStreamFormatDisp class definition.
 * @include: core/ofa-stream-format-disp.h
 *
 * Display an ofaStreamFormat.
 */

#include <gtk/gtk.h>

#include "api/ofa-stream-format.h"

G_BEGIN_DECLS

#define OFA_TYPE_STREAM_FORMAT_DISP                ( ofa_stream_format_disp_get_type())
#define OFA_STREAM_FORMAT_DISP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_STREAM_FORMAT_DISP, ofaStreamFormatDisp ))
#define OFA_STREAM_FORMAT_DISP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_STREAM_FORMAT_DISP, ofaStreamFormatDispClass ))
#define OFA_IS_STREAM_FORMAT_DISP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_STREAM_FORMAT_DISP ))
#define OFA_IS_STREAM_FORMAT_DISP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_STREAM_FORMAT_DISP ))
#define OFA_STREAM_FORMAT_DISP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_STREAM_FORMAT_DISP, ofaStreamFormatDispClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaStreamFormatDisp;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaStreamFormatDispClass;

GType                ofa_stream_format_disp_get_type  ( void ) G_GNUC_CONST;

ofaStreamFormatDisp *ofa_stream_format_disp_new       ( void );

void                 ofa_stream_format_disp_set_format( ofaStreamFormatDisp *bin,
															ofaStreamFormat *format );

G_END_DECLS

#endif /* __OFA_STREAM_FORMAT_DISP_H__ */
