/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_TVA_RECORD_TREEVIEW_H__
#define __OFA_TVA_RECORD_TREEVIEW_H__

/**
 * SECTION: ofa_tva_record_treeview
 * @short_description: #ofaTVARecordTreeview class definition.
 * @include: recurrent/ofa-tva-record-treeview.h
 *
 * Manage a treeview with the list of the recurrent models.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+----------------+
 *    | Signal           | VAT Record may |
 *    |                  | be %NULL       |
 *    +------------------+----------------+
 *    | ofa-vatchanged   |       Yes      |
 *    | ofa-vatactivated |        No      |
 *    | ofa-vatdelete    |        No      |
 *    +------------------+----------------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"

#include "tva/ofo-tva-record.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_RECORD_TREEVIEW                ( ofa_tva_record_treeview_get_type())
#define OFA_TVA_RECORD_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_RECORD_TREEVIEW, ofaTVARecordTreeview ))
#define OFA_TVA_RECORD_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_RECORD_TREEVIEW, ofaTVARecordTreeviewClass ))
#define OFA_IS_TVA_RECORD_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_RECORD_TREEVIEW ))
#define OFA_IS_TVA_RECORD_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_RECORD_TREEVIEW ))
#define OFA_TVA_RECORD_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_RECORD_TREEVIEW, ofaTVARecordTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaTVARecordTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaTVARecordTreeviewClass;

GType                 ofa_tva_record_treeview_get_type        ( void ) G_GNUC_CONST;

ofaTVARecordTreeview *ofa_tva_record_treeview_new             ( ofaIGetter *getter );

void                  ofa_tva_record_treeview_set_settings_key( ofaTVARecordTreeview *view,
																		const gchar *key );

void                  ofa_tva_record_treeview_setup_columns   ( ofaTVARecordTreeview *view );

void                  ofa_tva_record_treeview_setup_store     ( ofaTVARecordTreeview *view );

ofoTVARecord         *ofa_tva_record_treeview_get_selected    ( ofaTVARecordTreeview *view );

G_END_DECLS

#endif /* __OFA_TVA_RECORD_TREEVIEW_H__ */
