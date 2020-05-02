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

#ifndef __OPENBOOK_API_OFO_PAIMEAN_H__
#define __OPENBOOK_API_OFO_PAIMEAN_H__

/**
 * SECTION: ofopaimean
 * @title: ofoPaimean
 * @short_description: #ofoPaimean class definition.
 * @include: openbook/ofo-paimean.h
 *
 * This file defines the #ofoPaimean class public API.
 */

#include "api/ofa-box.h"
#include "api/ofa-igetter-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_PAIMEAN                ( ofo_paimean_get_type())
#define OFO_PAIMEAN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_PAIMEAN, ofoPaimean ))
#define OFO_PAIMEAN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_PAIMEAN, ofoPaimeanClass ))
#define OFO_IS_PAIMEAN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_PAIMEAN ))
#define OFO_IS_PAIMEAN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_PAIMEAN ))
#define OFO_PAIMEAN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_PAIMEAN, ofoPaimeanClass ))

typedef struct _ofoPaimean              ofoPaimean;

struct _ofoPaimean {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoPaimeanClass;

/* data max length */
#define PAM_NUMBER_WIDTH                10
#define PAM_NUMBER_MAX_LENGTH           64

GType             ofo_paimean_get_type              ( void ) G_GNUC_CONST;

GList            *ofo_paimean_get_dataset           ( ofaIGetter *getter );
#define           ofo_paimean_free_dataset( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_object_unref ))

ofoPaimean       *ofo_paimean_get_by_code           ( ofaIGetter *getter, const gchar *code );

ofoPaimean       *ofo_paimean_new                   ( ofaIGetter *getter );

const gchar      *ofo_paimean_get_code              ( const ofoPaimean *paimean );
const gchar      *ofo_paimean_get_cre_user          ( const ofoPaimean *paimean );
const myStampVal *ofo_paimean_get_cre_stamp         ( const ofoPaimean *paimean );
const gchar      *ofo_paimean_get_label             ( const ofoPaimean *paimean );
const gchar      *ofo_paimean_get_account           ( const ofoPaimean *paimean );
const gchar      *ofo_paimean_get_notes             ( const ofoPaimean *paimean );
const gchar      *ofo_paimean_get_upd_user          ( const ofoPaimean *paimean );
const myStampVal *ofo_paimean_get_upd_stamp         ( const ofoPaimean *paimean );

gboolean          ofo_paimean_is_deletable          ( const ofoPaimean *paimean );
gboolean          ofo_paimean_is_valid_data         ( const gchar *code, gchar **msgerr );

void              ofo_paimean_set_code              ( ofoPaimean *paimean, const gchar *code );
void              ofo_paimean_set_label             ( ofoPaimean *paimean, const gchar *label );
void              ofo_paimean_set_account           ( ofoPaimean *paimean, const gchar *account );
void              ofo_paimean_set_notes             ( ofoPaimean *paimean, const gchar *notes );

guint             ofo_paimean_doc_get_count         ( ofoPaimean *paimean );

GList            *ofo_paimean_doc_get_orphans       ( ofaIGetter *getter );
#define           ofo_paimean_free_orphans( L )     ( g_list_free_full(( L ), ( GDestroyNotify ) g_free ))

gboolean          ofo_paimean_insert                ( ofoPaimean *paimean );
gboolean          ofo_paimean_update                ( ofoPaimean *paimean, const gchar *prev_code );
gboolean          ofo_paimean_delete                ( ofoPaimean *paimean );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_PAIMEAN_H__ */
