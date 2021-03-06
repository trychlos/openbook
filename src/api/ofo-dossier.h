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

#ifndef __OPENBOOK_API_OFO_DOSSIER_H__
#define __OPENBOOK_API_OFO_DOSSIER_H__

/**
 * SECTION: ofodossier
 * @title: ofoDossier
 * @short_description: #ofoDossier class definition.
 * @include: openbook/ofo-dossier.h
 *
 * This file defines the #ofoDossier public API, including the general
 * DB definition.
 *
 * Terminology:
 * - dbname: the name of the subjacent database
 * - dname: the name of the dossier as recorded in the global
 *   configuration, which appears in combo boxes
 * - label: the 'raison sociale' of the dossier, recorded in DBMS
 *   at creation time, this label defaults to the dossier name
 */

#include "api/ofa-box.h"
#include "api/ofa-idbconnect-def.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"
#include "api/ofo-ledger-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_DOSSIER                ( ofo_dossier_get_type())
#define OFO_DOSSIER( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_DOSSIER, ofoDossier ))
#define OFO_DOSSIER_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_DOSSIER, ofoDossierClass ))
#define OFO_IS_DOSSIER( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_DOSSIER ))
#define OFO_IS_DOSSIER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_DOSSIER ))
#define OFO_DOSSIER_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_DOSSIER, ofoDossierClass ))

#if 0
typedef struct _ofoDossier              ofoDossier;
#endif

struct _ofoDossier {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoDossierClass;

/* default length of exercice in months
 */
#define DOSSIER_EXERCICE_DEFAULT_LENGTH 12

/* the identifier of the dossier row
 */
#define DOSSIER_ROW_ID                  1

GType                ofo_dossier_get_type                   ( void ) G_GNUC_CONST;

ofoDossier          *ofo_dossier_new                        ( ofaIGetter *getter );

const gchar         *ofo_dossier_get_cre_user               ( const ofoDossier *dossier );
const myStampVal    *ofo_dossier_get_cre_stamp              ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_default_currency       ( const ofoDossier *dossier );
const GDate         *ofo_dossier_get_exe_begin              ( const ofoDossier *dossier );
const GDate         *ofo_dossier_get_exe_end                ( const ofoDossier *dossier );
gint                 ofo_dossier_get_exe_length             ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_exe_notes              ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_forward_ope            ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_sld_ope                ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_import_ledger          ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_label                  ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_label2                 ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_notes                  ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_siren                  ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_siret                  ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_vatic                  ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_naf                    ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_upd_user               ( const ofoDossier *dossier );
const myStampVal    *ofo_dossier_get_upd_stamp              ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_status                 ( const ofoDossier *dossier );
const GDate         *ofo_dossier_get_last_closing_date      ( const ofoDossier *dossier );
ofxCounter           ofo_dossier_get_prevexe_last_entry     ( const ofoDossier *dossier );
const GDate         *ofo_dossier_get_prevexe_end            ( const ofoDossier *dossier );
const gchar         *ofo_dossier_get_rpid                   ( const ofoDossier *dossier );

GDate               *ofo_dossier_get_min_deffect            ( const ofoDossier *dossier,
																	const ofoLedger *ledger, GDate *date );

gboolean             ofo_dossier_is_current                 ( const ofoDossier *dossier );

gboolean             ofo_dossier_is_valid_data              ( const gchar *label,
																	gint nb_months,
																	const gchar *currency,
																	const GDate *begin,
																	const GDate *end,
																	gchar **msgerr );

void                 ofo_dossier_set_default_currency       ( ofoDossier *dossier, const gchar *currency );
void                 ofo_dossier_set_exe_begin              ( ofoDossier *dossier, const GDate *date );
void                 ofo_dossier_set_exe_end                ( ofoDossier *dossier, const GDate *date );
void                 ofo_dossier_set_exe_length             ( ofoDossier *dossier, gint nb_months );
void                 ofo_dossier_set_exe_notes              ( ofoDossier *dossier, const gchar *notes );
void                 ofo_dossier_set_forward_ope            ( ofoDossier *dossier, const gchar *ope );
void                 ofo_dossier_set_sld_ope                ( ofoDossier *dossier, const gchar *ope );
void                 ofo_dossier_set_import_ledger          ( ofoDossier *dossier, const gchar *mnemo );
void                 ofo_dossier_set_label                  ( ofoDossier *dossier, const gchar *label );
void                 ofo_dossier_set_label2                 ( ofoDossier *dossier, const gchar *label );
void                 ofo_dossier_set_notes                  ( ofoDossier *dossier, const gchar *notes );
void                 ofo_dossier_set_siren                  ( ofoDossier *dossier, const gchar *siren );
void                 ofo_dossier_set_siret                  ( ofoDossier *dossier, const gchar *siret );
void                 ofo_dossier_set_vatic                  ( ofoDossier *dossier, const gchar *tvaic );
void                 ofo_dossier_set_naf                    ( ofoDossier *dossier, const gchar *naf );
void                 ofo_dossier_set_last_closing_date      ( ofoDossier *dossier, const GDate *last_closing );
void                 ofo_dossier_set_prevexe_last_entry     ( ofoDossier *dossier, ofxCounter number );
void                 ofo_dossier_set_prevexe_end            ( ofoDossier *dossier, const GDate *date );
void                 ofo_dossier_set_current                ( ofoDossier *dossier, gboolean current );
void                 ofo_dossier_set_rpid                   ( ofoDossier *dossier, const gchar *rpid );

GSList              *ofo_dossier_currency_get_list          ( ofoDossier *dossier );
#define              ofo_dossier_currency_free_list( L )    ( g_slist_free_full(( L ), ( GDestroyNotify ) g_free ))

const gchar         *ofo_dossier_currency_get_sld_account   ( ofoDossier *dossier,
																	const gchar *currency );

GList               *ofo_dossier_currency_get_orphans           ( ofaIGetter *getter );
#define              ofo_dossier_currency_free_orphans( L ) ( g_list_free( L ))

void                 ofo_dossier_currency_reset             ( ofoDossier *dossier );
void                 ofo_dossier_currency_set_sld_account   ( ofoDossier *dossier, const gchar *currency, const gchar *account );

guint                ofo_dossier_doc_get_count              ( ofoDossier *dossier );

GList               *ofo_dossier_doc_get_orphans            ( ofaIGetter *getter );
#define              ofo_dossier_doc_free_orphans( L )      ( g_list_free( L ))

guint                ofo_dossier_prefs_get_count            ( ofoDossier *dossier );

GList               *ofo_dossier_prefs_get_orphans          ( ofaIGetter *getter );
#define              ofo_dossier_prefs_free_orphans( L )    ( g_list_free( L ))

gboolean             ofo_dossier_update                     ( ofoDossier *dossier );
gboolean             ofo_dossier_update_currencies          ( ofoDossier *dossier );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_DOSSIER_H__ */
