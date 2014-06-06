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

G_BEGIN_DECLS

/**
 * OFA_SIGNAL_DATASET_UPDATED: signal to be sent when an object is
 *         inserted in, updated or removed from the sgbd, or when a
 *         full dataset is reloaded from the sgbd.
 */
#define OFA_SIGNAL_DATASET_UPDATED     "ofa-signal-dataset_reloaded"

typedef enum {
	SIGNAL_OBJECT_NEW = 1,
	SIGNAL_OBJECT_UPDATED,
	SIGNAL_OBJECT_DELETED,
	SIGNAL_DATASET_RELOADED
}
	eSignalDetail;

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

GType           ofo_dossier_get_type                ( void ) G_GNUC_CONST;

ofoDossier     *ofo_dossier_new                     ( const gchar *name );

gboolean        ofo_dossier_open                    ( ofoDossier *dossier,
														const gchar *host, gint port,
														const gchar *socket, const gchar *dbname,
														const gchar *account, const gchar *password );

const gchar    *ofo_dossier_get_name                ( const ofoDossier *dossier );
const gchar    *ofo_dossier_get_user                ( const ofoDossier *dossier );
const ofoSgbd  *ofo_dossier_get_sgbd                ( const ofoDossier *dossier );

gboolean        ofo_dossier_use_devise              ( const ofoDossier *dossier, gint devise );

const gchar    *ofo_dossier_get_label               ( const ofoDossier *dossier );
gint            ofo_dossier_get_exercice_length     ( const ofoDossier *dossier );
gint            ofo_dossier_get_default_devise      ( const ofoDossier *dossier );
const gchar    *ofo_dossier_get_notes               ( const ofoDossier *dossier );
const gchar    *ofo_dossier_get_maj_user            ( const ofoDossier *dossier );
const GTimeVal *ofo_dossier_get_maj_stamp           ( const ofoDossier *dossier );

gint            ofo_dossier_get_current_exe_id      ( const ofoDossier *dossier );
const GDate    *ofo_dossier_get_current_exe_deb     ( const ofoDossier *dossier );
const GDate    *ofo_dossier_get_current_exe_fin     ( const ofoDossier *dossier );
gint            ofo_dossier_get_current_exe_last_ecr( const ofoDossier *dossier );

const GDate    *ofo_dossier_get_exe_fin             ( const ofoDossier *dossier, gint exe_id );

const GDate    *ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier );
gint            ofo_dossier_get_next_entry_number   ( const ofoDossier *dossier );

gboolean        ofo_dossier_is_valid                ( const gchar *label, gint duree, gint devise );

void            ofo_dossier_set_label               ( ofoDossier *dossier, const gchar *label );
void            ofo_dossier_set_exercice_length     ( ofoDossier *dossier, gint duree );
void            ofo_dossier_set_default_devise      ( ofoDossier *dossier, gint id );
void            ofo_dossier_set_notes               ( ofoDossier *dossier, const gchar *notes );
void            ofo_dossier_set_maj_user            ( ofoDossier *dossier, const gchar *user );
void            ofo_dossier_set_maj_stamp           ( ofoDossier *dossier, const GTimeVal *stamp );

void            ofo_dossier_set_current_exe_id      ( const ofoDossier *dossier, gint exe_id );
void            ofo_dossier_set_current_exe_deb     ( const ofoDossier *dossier, const GDate *date );
void            ofo_dossier_set_current_exe_fin     ( const ofoDossier *dossier, const GDate *date );
void            ofo_dossier_set_current_exe_last_ecr( const ofoDossier *dossier, gint number );

gboolean        ofo_dossier_dbmodel_update          ( ofoSgbd *sgbd, const gchar *account );

gboolean        ofo_dossier_update                  ( ofoDossier *dossier );

GSList         *ofo_dossier_get_csv                 ( const ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_DOSSIER_H__ */
