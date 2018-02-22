/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_BALANCE_GRID_BIN_H__
#define __OFA_BALANCE_GRID_BIN_H__

/**
 * SECTION: ofa_balance_grid_bin
 * @short_description: #ofaBalanceGridBin class definition.
 * @include: ui/ofa-balance-grid-bin.h
 *
 * A convenience class to manage a balance grid.
 *
 * A balance grid contains at least four row groups for rough, validated
 * and future entries, each row group containing itself one row per
 * currency.
 *
 * The class defines one "ofa-update" action signal to let the user
 * update a balance row in the grid.
 */

#include <gtk/gtk.h>

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofs-currency.h"

G_BEGIN_DECLS

#define OFA_TYPE_BALANCE_GRID_BIN                ( ofa_balance_grid_bin_get_type())
#define OFA_BALANCE_GRID_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BALANCE_GRID_BIN, ofaBalanceGridBin ))
#define OFA_BALANCE_GRID_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BALANCE_GRID_BIN, ofaBalanceGridBinClass ))
#define OFA_IS_BALANCE_GRID_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BALANCE_GRID_BIN ))
#define OFA_IS_BALANCE_GRID_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BALANCE_GRID_BIN ))
#define OFA_BALANCE_GRID_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BALANCE_GRID_BIN, ofaBalanceGridBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaBalanceGridBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaBalanceGridBinClass;

/**
 * The target row group in the grid.
 */
enum {
	BALANCEGRID_CURRENT_ROUGH = 1,
	BALANCEGRID_CURRENT_VALIDATED,
	BALANCEGRID_FUTUR_ROUGH,
	BALANCEGRID_FUTUR_VALIDATED,
	BALANCEGRID_TOTAL
};

GType              ofa_balance_grid_bin_get_type    ( void ) G_GNUC_CONST;

ofaBalanceGridBin *ofa_balance_grid_bin_new         ( ofaIGetter *getter );

void               ofa_balance_grid_bin_set_amounts ( ofaBalanceGridBin *bin,
															guint group,
															const gchar *currency,
															ofxAmount debit,
															ofxAmount credit );

void               ofa_balance_grid_bin_set_currency( ofaBalanceGridBin *bin,
															guint group,
															ofsCurrency *sbal );

G_END_DECLS

#endif /* __OFA_BALANCE_GRID_BIN_H__ */
