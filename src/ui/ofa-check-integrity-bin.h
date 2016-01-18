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

#ifndef __OFA_CHECK_INTEGRITY_BIN_H__
#define __OFA_CHECK_INTEGRITY_BIN_H__

/**
 * SECTION: ofa_check_integrity_bin
 * @short_description: #ofaCheckIntegrityBin class definition.
 * @include: ui/ofa-check-integrity-bin.h
 *
 * Check DBMS integrity.
 *
 * Please note that the checks are started when setting the dossier,
 * and run asynchronously. So they are most probably still running
 * when #ofa_check_integrity_bin_set_dossier() method returns.
 * Caller should connect to "ofa-done" signal in order to be signaled
 * when the checks are done.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: no
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CHECK_INTEGRITY_BIN                ( ofa_check_integrity_bin_get_type())
#define OFA_CHECK_INTEGRITY_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CHECK_INTEGRITY_BIN, ofaCheckIntegrityBin ))
#define OFA_CHECK_INTEGRITY_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CHECK_INTEGRITY_BIN, ofaCheckIntegrityBinClass ))
#define OFA_IS_CHECK_INTEGRITY_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CHECK_INTEGRITY_BIN ))
#define OFA_IS_CHECK_INTEGRITY_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CHECK_INTEGRITY_BIN ))
#define OFA_CHECK_INTEGRITY_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CHECK_INTEGRITY_BIN, ofaCheckIntegrityBinClass ))

typedef struct _ofaCheckIntegrityBinPrivate         ofaCheckIntegrityBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                      parent;

	/*< private members >*/
	ofaCheckIntegrityBinPrivate *priv;
}
	ofaCheckIntegrityBin;

typedef struct {
	/*< public members >*/
	GtkBinClass                 parent;
}
	ofaCheckIntegrityBinClass;

GType                ofa_check_integrity_bin_get_type  ( void ) G_GNUC_CONST;

ofaCheckIntegrityBin *ofa_check_integrity_bin_new      ( const gchar *settings );

void                 ofa_check_integrity_bin_set_hub   ( ofaCheckIntegrityBin *bin,
																ofaHub *hub );

gboolean             ofa_check_integrity_bin_get_status( const ofaCheckIntegrityBin *bin );

G_END_DECLS

#endif /* __OFA_CHECK_INTEGRITY_BIN_H__ */
