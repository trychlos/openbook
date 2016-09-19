/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_OPE_TEMPLATE_TREEVIEW_H__
#define __OFA_OPE_TEMPLATE_TREEVIEW_H__

/**
 * SECTION: ofa_ope_template_treeview
 * @short_description: #ofaOpeTemplateTreeview class definition.
 * @include: core/ofa-ope-template-treeview.h
 *
 * Manage a treeview with a filtered list of ope templates.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +------------------+-----------------+
 *    | Signal           | OpeTemplate may |
 *    |                  | be %NULL        |
 *    +------------------+-----------------+
 *    | ofa-opechanged   |       Yes       |
 *    | ofa-opeactivated |        No       |
 *    | ofa-opedelete    |        No       |
 *    +------------------+-----------------+
 *
 * Properties:
 * - ofa-ope-template-treeview-ledger: ledger identifier attached to this page.
 *
 * See api/ofo-ope-template.h for a full description of the model language.
 */

#include <gtk/gtk.h>

#include "api/ofa-tvbin.h"
#include "api/ofo-ope-template-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_TREEVIEW                ( ofa_ope_template_treeview_get_type())
#define OFA_OPE_TEMPLATE_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_TREEVIEW, ofaOpeTemplateTreeview ))
#define OFA_OPE_TEMPLATE_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_TREEVIEW, ofaOpeTemplateTreeviewClass ))
#define OFA_IS_OPE_TEMPLATE_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_TREEVIEW ))
#define OFA_IS_OPE_TEMPLATE_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_TREEVIEW ))
#define OFA_OPE_TEMPLATE_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_TREEVIEW, ofaOpeTemplateTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaOpeTemplateTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaOpeTemplateTreeviewClass;

GType                   ofa_ope_template_treeview_get_type        ( void ) G_GNUC_CONST;

ofaOpeTemplateTreeview *ofa_ope_template_treeview_new             ( const gchar *ledger );

const gchar            *ofa_ope_template_treeview_get_ledger      ( ofaOpeTemplateTreeview *view );

ofoOpeTemplate         *ofa_ope_template_treeview_get_selected    ( ofaOpeTemplateTreeview *view );

void                    ofa_ope_template_treeview_set_selected    ( ofaOpeTemplateTreeview *view,
																			const gchar *mnemo );

void                    ofa_ope_template_treeview_set_settings_key( ofaOpeTemplateTreeview *view,
																			const gchar *key );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_TREEVIEW_H__ */
