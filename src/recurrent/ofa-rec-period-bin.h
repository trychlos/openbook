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

#ifndef __OFA_REC_PERIOD_BIN_H__
#define __OFA_REC_PERIOD_BIN_H__

/**
 * SECTION: ofa_rec_period_bin
 * @short_description: #ofaRecPeriodBin class definition.
 * @include: api/ofa-rec-period-bin.h
 *
 * A #GtkComboBox -derived class to manage periodicities.
 *
 * The class defines following signal:
 * - "ofa-perchanged" when the selected periodicity changes.
 * - "ofa-detchanged" when the selected periodicity detail changes.
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

#include "recurrent/ofo-rec-period.h"

G_BEGIN_DECLS

#define OFA_TYPE_REC_PERIOD_BIN                ( ofa_rec_period_bin_get_type())
#define OFA_REC_PERIOD_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_REC_PERIOD_BIN, ofaRecPeriodBin ))
#define OFA_REC_PERIOD_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_REC_PERIOD_BIN, ofaRecPeriodBinClass ))
#define OFA_IS_REC_PERIOD_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_REC_PERIOD_BIN ))
#define OFA_IS_REC_PERIOD_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_REC_PERIOD_BIN ))
#define OFA_REC_PERIOD_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_REC_PERIOD_BIN, ofaRecPeriodBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaRecPeriodBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaRecPeriodBinClass;

GType            ofa_rec_period_bin_get_type             ( void ) G_GNUC_CONST;

ofaRecPeriodBin *ofa_rec_period_bin_new                  ( ofaHub *hub );

void             ofa_rec_period_bin_get_selected         ( ofaRecPeriodBin *bin,
																ofoRecPeriod **period,
																ofxCounter *detail_id );

void             ofa_rec_period_bin_set_selected         ( ofaRecPeriodBin *bin,
																const gchar *period_id,
																ofxCounter detail_id );

GtkWidget       *ofa_rec_period_bin_get_periodicity_combo( ofaRecPeriodBin *bin );

G_END_DECLS

#endif /* __OFA_REC_PERIOD_BIN_H__ */
