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

#ifndef __MY_PERIOD_BIN_H__
#define __MY_PERIOD_BIN_H__

/**
 * SECTION: my_period_bin
 * @short_description: #myPeriodBin class definition.
 * @include: my/my-period-bin.h
 *
 * Let the user choose and configure a periodicity.
 *
 * The widget is always considered valid, though the periodicity itself
 * being considered unset.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "my/my-isettings.h"
#include "my/my-period.h"

G_BEGIN_DECLS

#define MY_TYPE_PERIOD_BIN                ( my_period_bin_get_type())
#define MY_PERIOD_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, MY_TYPE_PERIOD_BIN, myPeriodBin ))
#define MY_PERIOD_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, MY_TYPE_PERIOD_BIN, myPeriodBinClass ))
#define MY_IS_PERIOD_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, MY_TYPE_PERIOD_BIN ))
#define MY_IS_PERIOD_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), MY_TYPE_PERIOD_BIN ))
#define MY_PERIOD_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), MY_TYPE_PERIOD_BIN, myPeriodBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	myPeriodBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	myPeriodBinClass;

GType        my_period_bin_get_type  ( void ) G_GNUC_CONST;

myPeriodBin *my_period_bin_new       ( myISettings *settings );

void         my_period_bin_set_period( myPeriodBin *bin, myPeriod *period );

myPeriod    *my_period_bin_get_period( myPeriodBin *bin );

G_END_DECLS

#endif /* __MY_PERIOD_BIN_H__ */
