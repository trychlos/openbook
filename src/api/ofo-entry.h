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

#ifndef __OFO_ENTRY_H__
#define __OFO_ENTRY_H__

/**
 * SECTION: ofo_entry
 * @short_description: #ofoEntry class definition.
 * @include: api/ofo-entry.h
 *
 * This class implements the Entry behavior.
 */

#include "api/my-date.h"
#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_ENTRY                ( ofo_entry_get_type())
#define OFO_ENTRY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_ENTRY, ofoEntry ))
#define OFO_ENTRY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_ENTRY, ofoEntryClass ))
#define OFO_IS_ENTRY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_ENTRY ))
#define OFO_IS_ENTRY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_ENTRY ))
#define OFO_ENTRY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_ENTRY, ofoEntryClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoEntryClass;

typedef struct _ofoEntryPrivate       ofoEntryPrivate;

typedef struct {
	/*< private >*/
	ofoBase          parent;
	ofoEntryPrivate *private;
}
	ofoEntry;

/**
 * ofaEntryStatus:
 */
typedef enum {
	ENT_STATUS_ROUGH = 1,
	ENT_STATUS_VALIDATED,
	ENT_STATUS_DELETED
}
	ofaEntryStatus;

/**
 * ofaEntryConcil:
 */
typedef enum {
	ENT_CONCILED_YES = 1,
	ENT_CONCILED_NO,
	ENT_CONCILED_ALL
}
	ofaEntryConcil;

GType          ofo_entry_get_type        ( void ) G_GNUC_CONST;

ofoEntry      *ofo_entry_new             ( void );

void           ofo_entry_connect_handlers( const ofoDossier *dossier );

GList         *ofo_entry_get_dataset_by_concil         ( const ofoDossier *dossier,
															const gchar *account,
															ofaEntryConcil mode );
GList         *ofo_entry_get_dataset_by_account        ( const ofoDossier *dossier,
															const gchar *account,
															const GDate *from, const GDate *to );
GList         *ofo_entry_get_dataset_by_ledger         ( const ofoDossier *dossier,
															const gchar *ledger,
															const GDate *from, const GDate *to );
GList         *ofo_entry_get_dataset_for_print_reconcil( const ofoDossier *dossier,
															const gchar *account,
															const GDate *date );

void           ofo_entry_free_dataset    ( GList *dataset );

gboolean       ofo_entry_use_devise      ( const ofoDossier *dossier, const gchar *devise );
gboolean       ofo_entry_use_ledger      ( const ofoDossier *dossier, const gchar *journal );
gboolean       ofo_entry_use_ope_template( const ofoDossier *dossier, const gchar *model );

gint           ofo_entry_get_number      ( const ofoEntry *entry );
const gchar   *ofo_entry_get_label       ( const ofoEntry *entry );
const GDate   *ofo_entry_get_deffect     ( const ofoEntry *entry );
const GDate   *ofo_entry_get_dope        ( const ofoEntry *entry );
const gchar   *ofo_entry_get_ref         ( const ofoEntry *entry );
const gchar   *ofo_entry_get_account     ( const ofoEntry *entry );
const gchar   *ofo_entry_get_devise      ( const ofoEntry *entry );
const gchar   *ofo_entry_get_ledger      ( const ofoEntry *entry );
const gchar   *ofo_entry_get_ope_template( const ofoEntry *entry );
gdouble        ofo_entry_get_debit       ( const ofoEntry *entry );
gdouble        ofo_entry_get_credit      ( const ofoEntry *entry );
ofaEntryStatus ofo_entry_get_status      ( const ofoEntry *entry );
const gchar   *ofo_entry_get_maj_user    ( const ofoEntry *entry );
const GTimeVal*ofo_entry_get_maj_stamp   ( const ofoEntry *entry );
const GDate   *ofo_entry_get_rappro_dval ( const ofoEntry *entry );
const gchar   *ofo_entry_get_rappro_user ( const ofoEntry *entry );
const GTimeVal*ofo_entry_get_rappro_stamp( const ofoEntry *entry );

void           ofo_entry_set_number      ( ofoEntry *entry, gint number );
void           ofo_entry_set_label       ( ofoEntry *entry, const gchar *label );
void           ofo_entry_set_deffect     ( ofoEntry *entry, const GDate *date );
void           ofo_entry_set_dope        ( ofoEntry *entry, const GDate *date );
void           ofo_entry_set_ref         ( ofoEntry *entry, const gchar *ref );
void           ofo_entry_set_account     ( ofoEntry *entry, const gchar *number );
void           ofo_entry_set_devise      ( ofoEntry *entry, const gchar *devise );
void           ofo_entry_set_ledger      ( ofoEntry *entry, const gchar *journal );
void           ofo_entry_set_ope_template( ofoEntry *entry, const gchar *model );
void           ofo_entry_set_debit       ( ofoEntry *entry, gdouble amount );
void           ofo_entry_set_credit      ( ofoEntry *entry, gdouble amount );
void           ofo_entry_set_status      ( ofoEntry *entry, ofaEntryStatus status );
void           ofo_entry_set_maj_user    ( ofoEntry *entry, const gchar *user );
void           ofo_entry_set_maj_stamp   ( ofoEntry *entry, const GTimeVal *stamp );
void           ofo_entry_set_rappro_dval ( ofoEntry *entry, const GDate *date );
void           ofo_entry_set_rappro_user ( ofoEntry *entry, const gchar *user );
void           ofo_entry_set_rappro_stamp( ofoEntry *entry, const GTimeVal *stamp );

gboolean       ofo_entry_is_valid        ( const ofoDossier *dossier,
													const GDate *effet, const GDate *ope,
													const gchar *label,
													const gchar *account, const gchar *devise,
													const gchar *ledger, const gchar *model,
													gdouble debit, gdouble credit );

ofoEntry      *ofo_entry_new_with_data   ( const ofoDossier *dossier,
													const GDate *effet, const GDate *ope,
													const gchar *label, const gchar *ref,
													const gchar *account, const gchar *devise,
													const gchar *ledger, const gchar *model,
													gdouble debit, gdouble credit );

gboolean       ofo_entry_insert          ( ofoEntry *entry, ofoDossier *dossier );
gboolean       ofo_entry_update          ( ofoEntry *entry, const ofoDossier *dossier );
gboolean       ofo_entry_update_rappro   ( ofoEntry *entry, const ofoDossier *dossier );
gboolean       ofo_entry_validate        ( ofoEntry *entry, const ofoDossier *dossier );

gboolean       ofo_entry_validate_by_ledger( const ofoDossier *dossier,
												const gchar *mnemo, const myDate *effect );

gboolean       ofo_entry_delete          ( ofoEntry *entry, const ofoDossier *dossier );

GSList        *ofo_entry_get_csv         ( const ofoDossier *dossier );
void           ofo_entry_import_csv      ( ofoDossier *dossier, GSList *lines, gboolean with_header );

G_END_DECLS

#endif /* __OFO_ENTRY_H__ */
