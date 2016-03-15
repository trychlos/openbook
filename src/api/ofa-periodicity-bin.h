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

#ifndef __OFA_PERIODICITY_BIN_H__
#define __OFA_PERIODICITY_BIN_H__

/**
 * SECTION: ofa_periodicity_bin
 * @short_description: #ofaPeriodicityBin class definition.
 * @include: api/ofa-periodicity-combo.h
 *
 * A #GtkComboBox -derived class to manage periodicities.
 *
 * The class defines an "ofa-changed" signal which is triggered when the
 * selected periodicity changes.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_PERIODICITY_BIN                ( ofa_periodicity_bin_get_type())
#define OFA_PERIODICITY_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PERIODICITY_BIN, ofaPeriodicityBin ))
#define OFA_PERIODICITY_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PERIODICITY_BIN, ofaPeriodicityBinClass ))
#define OFA_IS_PERIODICITY_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PERIODICITY_BIN ))
#define OFA_IS_PERIODICITY_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PERIODICITY_BIN ))
#define OFA_PERIODICITY_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PERIODICITY_BIN, ofaPeriodicityBinClass ))

typedef struct _ofaPeriodicityBinPrivate        ofaPeriodicityBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaPeriodicityBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaPeriodicityBinClass;

GType                ofa_periodicity_bin_get_type           ( void ) G_GNUC_CONST;

ofaPeriodicityBin *ofa_periodicity_bin_new                  ( void );

void               ofa_periodicity_bin_get_selected         ( ofaPeriodicityBin *bin,
																	gchar **periodicity,
																	gchar **detail,
																	gboolean *valid,
																	gchar **msgerr );

void               ofa_periodicity_bin_set_selected         ( ofaPeriodicityBin *bin,
																	const gchar *periodicity,
																	const gchar *detail );

GtkWidget         *ofa_periodicity_bin_get_periodicity_combo( ofaPeriodicityBin *bin );

G_END_DECLS

#endif /* __OFA_PERIODICITY_BIN_H__ */
