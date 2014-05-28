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

#ifndef __OFO_DOSSIER_H__
#define __OFO_DOSSIER_H__

/**
 * SECTION: ofo_dossier
 * @short_description: #ofoDossier class definition.
 * @include: ui/ofo-dossier.h
 *
 * This class implements the Dossier behavior, including the general
 * DB definition.
 *
 * The dossier maintains the GList * list of accounts. It loads it on
 * demand, et releases it on instance dispose.
 */

#include "ui/ofo-dossier-def.h"
#include "ui/ofo-sgbd-def.h"

#include "ui/ofo-entry.h"

G_BEGIN_DECLS

/**
 * ofaDossierStatus:
 *
 * Status of the exercice.
 *
 * The system makes sure there is at most one opened exercice at any
 * time.
 */
typedef enum {
	DOS_STATUS_OPENED = 1,
	DOS_STATUS_CLOSED
}
	ofaDossierStatus;

GType         ofo_dossier_get_type( void ) G_GNUC_CONST;

ofoDossier   *ofo_dossier_new     ( const gchar *name );

gboolean      ofo_dossier_open    ( ofoDossier *dossier,
										const gchar *host, gint port,
										const gchar *socket, const gchar *dbname,
										const gchar *account, const gchar *password );

const gchar  *ofo_dossier_get_name( const ofoDossier *dossier );
const gchar  *ofo_dossier_get_user( const ofoDossier *dossier );
ofoSgbd      *ofo_dossier_get_sgbd( const ofoDossier *dossier );

gint          ofo_dossier_get_exercice_id         ( const ofoDossier *dossier );
const GDate  *ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier );
gint          ofo_dossier_get_next_entry_number   ( const ofoDossier *dossier );

gboolean      ofo_dossier_dbmodel_update    ( ofoSgbd *sgbd, const gchar *account );

G_END_DECLS

#endif /* __OFO_DOSSIER_H__ */
