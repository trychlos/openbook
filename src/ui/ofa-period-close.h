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

#ifndef __OFA_PERIOD_CLOSE_H__
#define __OFA_PERIOD_CLOSE_H__

/**
 * SECTION: ofa_period_close
 * @short_description: #ofaPeriodClose class definition.
 * @include: ui/ofa-period-close.h
 *
 * Run an intermediate closing for a period.
 *
 * Closing a period is almost the same that closing all ledgers for the
 * specified date. The difference is that even a newly created ledger
 * will not allowed to enter an entry before the closing date.
 *
 * All ledgers will all be closed for the date.
 *
 * On option, the validated accounts balances may be saved for this
 * date.
 *
 * Whether an error be detected or not at recording time, the dialog
 * terminates on OK, after having displayed a success or an error message
 * box.
 *
 * Development rules:
 * - type:               non-modal dialog
 * - message on success: yes
 * - settings:           yes
 * - current:            yes
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PERIOD_CLOSE                ( ofa_period_close_get_type())
#define OFA_PERIOD_CLOSE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PERIOD_CLOSE, ofaPeriodClose ))
#define OFA_PERIOD_CLOSE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PERIOD_CLOSE, ofaPeriodCloseClass ))
#define OFA_IS_PERIOD_CLOSE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PERIOD_CLOSE ))
#define OFA_IS_PERIOD_CLOSE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PERIOD_CLOSE ))
#define OFA_PERIOD_CLOSE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PERIOD_CLOSE, ofaPeriodCloseClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaPeriodClose;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaPeriodCloseClass;

GType    ofa_period_close_get_type( void ) G_GNUC_CONST;

void     ofa_period_close_run     ( ofaIGetter *getter,
										GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_PERIOD_CLOSE_H__ */
