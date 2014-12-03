/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifndef __OFA_BALANCES_GRID_H__
#define __OFA_BALANCES_GRID_H__

/**
 * SECTION: ofa_balances_grid
 * @short_description: #ofaBalancesGrid class definition.
 * @include: ui/ofa-balances-grid.h
 *
 * A convenience class to manage a balance grid.
 * It defines one "update" action signal to let the user update a
 * balance row in the grid.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_BALANCES_GRID                ( ofa_balances_grid_get_type())
#define OFA_BALANCES_GRID( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BALANCES_GRID, ofaBalancesGrid ))
#define OFA_BALANCES_GRID_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BALANCES_GRID, ofaBalancesGridClass ))
#define OFA_IS_BALANCES_GRID( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BALANCES_GRID ))
#define OFA_IS_BALANCES_GRID_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BALANCES_GRID ))
#define OFA_BALANCES_GRID_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BALANCES_GRID, ofaBalancesGridClass ))

typedef struct _ofaBalancesGridPrivate        ofaBalancesGridPrivate;

typedef struct {
	/*< public members >*/
	GObject                 parent;

	/*< private members >*/
	ofaBalancesGridPrivate *priv;
}
	ofaBalancesGrid;

typedef struct {
	/*< public members >*/
	GObjectClass            parent;
}
	ofaBalancesGridClass;

GType            ofa_balances_grid_get_type ( void ) G_GNUC_CONST;

ofaBalancesGrid *ofa_balances_grid_new      ( void );

void             ofa_balances_grid_attach_to( ofaBalancesGrid *combo, GtkContainer *new_parent );

G_END_DECLS

#endif /* __OFA_BALANCES_GRID_H__ */
