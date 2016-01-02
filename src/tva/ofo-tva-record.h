/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFO_TVA_RECORD_H__
#define __OFO_TVA_RECORD_H__

/**
 * SECTION: ofo_tva_record
 * @short_description: #ofoTVARecord class definition.
 * @include: tva/ofo-tva-form.h
 *
 * This file defines the #ofoTVARecord class behavior.
 *
 * An #ofoTVARecord describes a TVA declaration, which happends to have
 * been based at its creation on a TVA form. This TVA form may have been
 * deleted since that.
 */

#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"

#include "tva/ofo-tva-form.h"

G_BEGIN_DECLS

#define OFO_TYPE_TVA_RECORD                ( ofo_tva_record_get_type())
#define OFO_TVA_RECORD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TVA_RECORD, ofoTVARecord ))
#define OFO_TVA_RECORD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TVA_RECORD, ofoTVARecordClass ))
#define OFO_IS_TVA_RECORD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TVA_RECORD ))
#define OFO_IS_TVA_RECORD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TVA_RECORD ))
#define OFO_TVA_RECORD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TVA_RECORD, ofoTVARecordClass ))

typedef struct _ofoTVARecordPrivate        ofoTVARecordPrivate;

typedef struct {
	/*< public members >*/
	ofoBase              parent;

	/*< private members >*/
	ofoTVARecordPrivate *priv;
}
	ofoTVARecord;

typedef struct {
	/*< public members >*/
	ofoBaseClass         parent;
}
	ofoTVARecordClass;

GType           ofo_tva_record_get_type              ( void ) G_GNUC_CONST;

GList          *ofo_tva_record_get_dataset           ( ofoDossier *dossier );

ofoTVARecord   *ofo_tva_record_new                   ( void );
ofoTVARecord   *ofo_tva_record_new_from_form         ( const ofoTVAForm *form );

const gchar    *ofo_tva_record_get_mnemo             ( const ofoTVARecord *record );
const gchar    *ofo_tva_record_get_label             ( const ofoTVARecord *record );
gboolean        ofo_tva_record_get_has_correspondence( const ofoTVARecord *record );
const gchar    *ofo_tva_record_get_notes             ( const ofoTVARecord *record );
gboolean        ofo_tva_record_get_is_validated      ( const ofoTVARecord *record );
const GDate    *ofo_tva_record_get_begin             ( const ofoTVARecord *record );
const GDate    *ofo_tva_record_get_end               ( const ofoTVARecord *record );
const gchar    *ofo_tva_record_get_upd_user          ( const ofoTVARecord *record );
const GTimeVal *ofo_tva_record_get_upd_stamp         ( const ofoTVARecord *record );

gboolean        ofo_tva_record_is_deletable          ( const ofoTVARecord *record, const ofoDossier *dossier );
gboolean        ofo_tva_record_is_validable          ( const ofoTVARecord *record );

gint            ofo_tva_record_compare_id            ( const ofoTVARecord *a, const ofoTVARecord *b );

void            ofo_tva_record_set_mnemo             ( ofoTVARecord *record, const gchar *mnemo );
void            ofo_tva_record_set_label             ( ofoTVARecord *record, const gchar *label );
void            ofo_tva_record_set_has_correspondence( ofoTVARecord *record, gboolean has_correspondence );
void            ofo_tva_record_set_notes             ( ofoTVARecord *record, const gchar *notes );
void            ofo_tva_record_set_is_validated      ( ofoTVARecord *record, gboolean is_validated );
void            ofo_tva_record_set_begin             ( ofoTVARecord *record, const GDate *date );
void            ofo_tva_record_set_end               ( ofoTVARecord *record, const GDate *date );

void            ofo_tva_record_detail_add            ( ofoTVARecord *record,
														guint level,
														const gchar *code,
														const gchar *label,
														gboolean has_base,
														const gchar *base,
														gboolean has_amount,
														const gchar *amount );

void            ofo_tva_record_detail_free_all       ( ofoTVARecord *record );

guint           ofo_tva_record_detail_get_count      ( const ofoTVARecord *record );
guint           ofo_tva_record_detail_get_level      ( const ofoTVARecord *record, guint idx );
const gchar    *ofo_tva_record_detail_get_code       ( const ofoTVARecord *record, guint idx );
const gchar    *ofo_tva_record_detail_get_label      ( const ofoTVARecord *record, guint idx );
gboolean        ofo_tva_record_detail_get_has_base   ( const ofoTVARecord *record, guint idx );
const gchar    *ofo_tva_record_detail_get_base       ( const ofoTVARecord *record, guint idx );
gboolean        ofo_tva_record_detail_get_has_amount ( const ofoTVARecord *record, guint idx );
const gchar    *ofo_tva_record_detail_get_amount     ( const ofoTVARecord *record, guint idx );

void            ofo_tva_record_detail_set_base       ( ofoTVARecord *record, guint idx, const gchar *base );
void            ofo_tva_record_detail_set_amount     ( ofoTVARecord *record, guint idx, const gchar *amount );

void            ofo_tva_record_boolean_add           ( ofoTVARecord *record,
														const gchar *label,
														gboolean is_true );

void            ofo_tva_record_boolean_free_all      ( ofoTVARecord *record );

guint           ofo_tva_record_boolean_get_count     ( const ofoTVARecord *record );
const gchar    *ofo_tva_record_boolean_get_label     ( const ofoTVARecord *record, guint idx );
gboolean        ofo_tva_record_boolean_get_is_true   ( const ofoTVARecord *record, guint idx );

gboolean        ofo_tva_record_insert                ( ofoTVARecord *record, ofoDossier *dossier );
gboolean        ofo_tva_record_update                ( ofoTVARecord *record, ofoDossier *dossier, const gchar *prev_mnemo );
gboolean        ofo_tva_record_delete                ( ofoTVARecord *record, ofoDossier *dossier );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_TVA_RECORD_H__ */