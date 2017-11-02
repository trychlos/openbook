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

#ifndef __OFA_RECURRENT_GENERATE_OVER_H__
#define __OFA_RECURRENT_GENERATE_OVER_H__

/**
 * SECTION: ofa_recurrent_generate_over
 * @short_description: #ofaRecurrentGenerateOver class definition.
 * @include: core/ofa-recurrent-generate-over.h
 *
 * Require the user for a choice and a confirmation when generating
 * recurrent operations.
 *
 * Development rules:
 * - type:       modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-entry.h"

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT_GENERATE_OVER                ( ofa_recurrent_generate_over_get_type())
#define OFA_RECURRENT_GENERATE_OVER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT_GENERATE_OVER, ofaRecurrentGenerateOver ))
#define OFA_RECURRENT_GENERATE_OVER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT_GENERATE_OVER, ofaRecurrentGenerateOverClass ))
#define OFA_IS_RECURRENT_GENERATE_OVER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT_GENERATE_OVER ))
#define OFA_IS_RECURRENT_GENERATE_OVER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT_GENERATE_OVER ))
#define OFA_RECURRENT_GENERATE_OVER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT_GENERATE_OVER, ofaRecurrentGenerateOverClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaRecurrentGenerateOver;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaRecurrentGenerateOverClass;

/**
 * ofeRecurrentGenerateOver:
 *
 * @OFA_RECURRENT_GENERATE_CANCEL: cancel the generation.
 * @OFA_RECURRENT_GENERATE_OVER: regenerate operations from the requested
 *  beginning date, even if these operations were already generated
 *  previously.
 * @OFA_RECURRENT_GENERATE_NEW: only generate new operations.
 */
typedef enum {
	OFA_RECURRENT_GENERATE_CANCEL = 1,
	OFA_RECURRENT_GENERATE_OVER,
	OFA_RECURRENT_GENERATE_NEW
}
	ofeRecurrentGenerateOver;

GType                    ofa_recurrent_generate_over_get_type( void ) G_GNUC_CONST;

ofeRecurrentGenerateOver ofa_recurrent_generate_over_run     ( ofaIGetter *getter,
																	const GDate *last,
																	const GDate *begin );

G_END_DECLS

#endif /* __OFA_RECURRENT_GENERATE_OVER_H__ */
