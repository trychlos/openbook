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

#ifndef __OFO_TVA_FORM_H__
#define __OFO_TVA_FORM_H__

/**
 * SECTION: ofo_tva_form
 * @short_description: #ofoTVAForm class definition.
 * @include: tva/ofo-tva-form.h
 *
 * This file defines the #ofoTVAForm class behavior.
 *
 * An #ofoTVAForm describes a TVA formular.
 */

#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_TVA_FORM                ( ofo_tva_form_get_type())
#define OFO_TVA_FORM( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TVA_FORM, ofoTVAForm ))
#define OFO_TVA_FORM_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TVA_FORM, ofoTVAFormClass ))
#define OFO_IS_TVA_FORM( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TVA_FORM ))
#define OFO_IS_TVA_FORM_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TVA_FORM ))
#define OFO_TVA_FORM_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TVA_FORM, ofoTVAFormClass ))

typedef struct _ofoTVAFormPrivate        ofoTVAFormPrivate;

typedef struct {
	/*< public members >*/
	ofoBase            parent;

	/*< private members >*/
	ofoTVAFormPrivate *priv;
}
	ofoTVAForm;

typedef struct {
	/*< public members >*/
	ofoBaseClass       parent;
}
	ofoTVAFormClass;

GType           ofo_tva_form_get_type              ( void ) G_GNUC_CONST;

void            ofo_tva_form_connect_handlers      ( const ofoDossier *dossier );

GList          *ofo_tva_form_get_dataset           ( ofoDossier *dossier );
ofoTVAForm     *ofo_tva_form_get_by_mnemo          ( ofoDossier *dossier, const gchar *mnemo );
gboolean        ofo_tva_form_use_account           ( ofoDossier *dossier, const gchar *account );

ofoTVAForm     *ofo_tva_form_new                   ( void );
ofoTVAForm     *ofo_tva_form_new_from_form         ( const ofoTVAForm *form );

const gchar    *ofo_tva_form_get_mnemo             ( const ofoTVAForm *form );
gchar          *ofo_tva_form_get_mnemo_new_from    ( const ofoTVAForm *form, ofoDossier *dossier );
const gchar    *ofo_tva_form_get_label             ( const ofoTVAForm *form );
gboolean        ofo_tva_form_get_has_correspondence( const ofoTVAForm *form );
const gchar    *ofo_tva_form_get_notes             ( const ofoTVAForm *form );
const gchar    *ofo_tva_form_get_upd_user          ( const ofoTVAForm *form );
const GTimeVal *ofo_tva_form_get_upd_stamp         ( const ofoTVAForm *form );

gboolean        ofo_tva_form_is_deletable          ( const ofoTVAForm *form, ofoDossier *dossier );
gboolean        ofo_tva_form_is_valid              ( ofoDossier *dossier,
														const gchar *mnemo );

void            ofo_tva_form_set_mnemo             ( ofoTVAForm *form, const gchar *mnemo );
void            ofo_tva_form_set_label             ( ofoTVAForm *form, const gchar *label );
void            ofo_tva_form_set_has_correspondence( ofoTVAForm *form, gboolean has_correspondence );
void            ofo_tva_form_set_notes             ( ofoTVAForm *form, const gchar *notes );

void            ofo_tva_form_detail_add            ( ofoTVAForm *form,
														guint level,
														const gchar *code,
														const gchar *label,
														gboolean has_base,
														const gchar *base,
														gboolean has_amount,
														const gchar *amount );

void            ofo_tva_form_detail_free_all       ( ofoTVAForm *form );

guint           ofo_tva_form_detail_get_count      ( const ofoTVAForm *form );
guint           ofo_tva_form_detail_get_level      ( const ofoTVAForm *form, guint idx );
const gchar    *ofo_tva_form_detail_get_code       ( const ofoTVAForm *form, guint idx );
const gchar    *ofo_tva_form_detail_get_label      ( const ofoTVAForm *form, guint idx );
gboolean        ofo_tva_form_detail_get_has_base   ( const ofoTVAForm *form, guint idx );
const gchar    *ofo_tva_form_detail_get_base       ( const ofoTVAForm *form, guint idx );
gboolean        ofo_tva_form_detail_get_has_amount ( const ofoTVAForm *form, guint idx );
const gchar    *ofo_tva_form_detail_get_amount     ( const ofoTVAForm *form, guint idx );

void            ofo_tva_form_boolean_add           ( ofoTVAForm *form,
														const gchar *label );

void            ofo_tva_form_boolean_free_all      ( ofoTVAForm *form );

guint           ofo_tva_form_boolean_get_count     ( const ofoTVAForm *form );
const gchar    *ofo_tva_form_boolean_get_label     ( const ofoTVAForm *form, guint idx );

gboolean        ofo_tva_form_insert                ( ofoTVAForm *form, ofoDossier *dossier );
gboolean        ofo_tva_form_update                ( ofoTVAForm *form, ofoDossier *dossier, const gchar *prev_mnemo );
gboolean        ofo_tva_form_delete                ( ofoTVAForm *form, ofoDossier *dossier );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_TVA_FORM_H__ */
