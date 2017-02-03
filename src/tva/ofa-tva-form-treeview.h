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

#ifndef __OFA_TVA_FORM_TREEVIEW_H__
#define __OFA_TVA_FORM_TREEVIEW_H__

/**
 * SECTION: ofa_tva_form_treeview
 * @short_description: #ofaTVAFormTreeview class definition.
 * @include: recurrent/ofa-tva-form-treeview.h
 *
 * Manage a treeview with the list of the recurrent models.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+--------------+
 *    | Signal           | VAT Form may |
 *    |                  | be %NULL     |
 *    +------------------+--------------+
 *    | ofa-vatchanged   |      Yes     |
 *    | ofa-vatactivated |       No     |
 *    | ofa-vatdelete    |       No     |
 *    +------------------+--------------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"

#include "tva/ofo-tva-form.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_FORM_TREEVIEW                ( ofa_tva_form_treeview_get_type())
#define OFA_TVA_FORM_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_FORM_TREEVIEW, ofaTVAFormTreeview ))
#define OFA_TVA_FORM_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_FORM_TREEVIEW, ofaTVAFormTreeviewClass ))
#define OFA_IS_TVA_FORM_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_FORM_TREEVIEW ))
#define OFA_IS_TVA_FORM_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_FORM_TREEVIEW ))
#define OFA_TVA_FORM_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_FORM_TREEVIEW, ofaTVAFormTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaTVAFormTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaTVAFormTreeviewClass;

GType               ofa_tva_form_treeview_get_type        ( void ) G_GNUC_CONST;

ofaTVAFormTreeview *ofa_tva_form_treeview_new             ( ofaIGetter *getter );

void                ofa_tva_form_treeview_set_settings_key( ofaTVAFormTreeview *view,
																	const gchar *key );

void                ofa_tva_form_treeview_setup_columns   ( ofaTVAFormTreeview *view );

void                ofa_tva_form_treeview_setup_store     ( ofaTVAFormTreeview *view );

ofoTVAForm         *ofa_tva_form_treeview_get_selected    ( ofaTVAFormTreeview *view );

G_END_DECLS

#endif /* __OFA_TVA_FORM_TREEVIEW_H__ */
