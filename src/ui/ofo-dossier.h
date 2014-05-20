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

#include "ui/ofa-sgbd.h"
#include "ui/ofo-base.h"
#include "ui/ofo-account.h"
#include "ui/ofo-devise.h"
#include "ui/ofo-journal.h"
#include "ui/ofo-model.h"
#include "ui/ofo-taux.h"

G_BEGIN_DECLS

#define OFO_TYPE_DOSSIER                ( ofo_dossier_get_type())
#define OFO_DOSSIER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_DOSSIER, ofoDossier ))
#define OFO_DOSSIER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_DOSSIER, ofoDossierClass ))
#define OFO_IS_DOSSIER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_DOSSIER ))
#define OFO_IS_DOSSIER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_DOSSIER ))
#define OFO_DOSSIER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_DOSSIER, ofoDossierClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass            parent;
}
	ofoDossierClass;

typedef struct _ofoDossierPrivate       ofoDossierPrivate;

typedef struct {
	/*< private >*/
	ofoBase            parent;
	ofoDossierPrivate *priv;
}
	ofoDossier;

GType         ofo_dossier_get_type( void );

ofoDossier   *ofo_dossier_new     ( const gchar *name );

gboolean      ofo_dossier_open    ( ofoDossier *dossier,
										const gchar *host, gint port,
										const gchar *socket, const gchar *dbname,
										const gchar *account, const gchar *password );

const gchar  *ofo_dossier_get_name( const ofoDossier *dossier );
const gchar  *ofo_dossier_get_user( const ofoDossier *dossier );

ofoAccount   *ofo_dossier_get_account       ( const ofoDossier *dossier, const gchar *number );
GList        *ofo_dossier_get_accounts_chart( ofoDossier *dossier );
gboolean      ofo_dossier_insert_account    ( ofoDossier *dossier, ofoAccount *account );
gboolean      ofo_dossier_update_account    ( ofoDossier *dossier, ofoAccount *account, const gchar *prev_number );
gboolean      ofo_dossier_delete_account    ( ofoDossier *dossier, ofoAccount *account );

ofoDevise    *ofo_dossier_get_devise        ( const ofoDossier *dossier, const gchar *code );
GList        *ofo_dossier_get_devises_set   ( ofoDossier *dossier );
gboolean      ofo_dossier_insert_devise     ( ofoDossier *dossier, ofoDevise *devise );
gboolean      ofo_dossier_update_devise     ( ofoDossier *dossier, ofoDevise *devise );
gboolean      ofo_dossier_delete_devise     ( ofoDossier *dossier, ofoDevise *devise );

ofoJournal   *ofo_dossier_get_journal       ( const ofoDossier *dossier, const gchar *mnemo );
GList        *ofo_dossier_get_journals_set  ( ofoDossier *dossier );
gboolean      ofo_dossier_insert_journal    ( ofoDossier *dossier, ofoJournal *journal );
gboolean      ofo_dossier_update_journal    ( ofoDossier *dossier, ofoJournal *journal );
gboolean      ofo_dossier_delete_journal    ( ofoDossier *dossier, ofoJournal *journal );

ofoModel     *ofo_dossier_get_model         ( const ofoDossier *dossier, const gchar *mnemo );
GList        *ofo_dossier_get_models_set    ( ofoDossier *dossier );
gboolean      ofo_dossier_insert_model      ( ofoDossier *dossier, ofoModel *model );
gboolean      ofo_dossier_update_model      ( ofoDossier *dossier, ofoModel *model, const gchar *prev_mnemo );
gboolean      ofo_dossier_delete_model      ( ofoDossier *dossier, ofoModel *model );

ofoTaux      *ofo_dossier_check_for_taux    ( const ofoDossier *dossier,
												gint id, const gchar *mnemo,
												const GDate *begin, const GDate *end );
/* this is a the standard method which one use to get a taux for a date
 * ofoTaux      *ofo_dossier_get_taux          ( const ofoDossier *dossier, const gchar *mnemo, const GDate *date );*/
GList        *ofo_dossier_get_taux_set      ( ofoDossier *dossier );
gboolean      ofo_dossier_insert_taux       ( ofoDossier *dossier, ofoTaux *taux );
gboolean      ofo_dossier_update_taux       ( ofoDossier *dossier, ofoTaux *taux );
gboolean      ofo_dossier_delete_taux       ( ofoDossier *dossier, ofoTaux *taux );

gboolean      ofo_dossier_dbmodel_update    ( ofaSgbd *sgbd, GtkWindow *parent, const gchar *account );

G_END_DECLS

#endif /* __OFO_DOSSIER_H__ */
