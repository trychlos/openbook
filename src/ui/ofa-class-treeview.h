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

#ifndef __OFA_CLASS_TREEVIEW_H__
#define __OFA_CLASS_TREEVIEW_H__

/**
 * SECTION: ofa_class_treeview
 * @short_description: #ofaClassTreeview class definition.
 * @include: ui/ofa-class-treeview.h
 *
 * Manage a treeview with the list of the account classes.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class.
 *    +--------------------+-----------+
 *    | Signal             | Class may |
 *    |                    | be %NULL  |
 *    +--------------------+-----------+
 *    | ofa-classchanged   |    Yes    |
 *    | ofa-classactivated |     No    |
 *    | ofa-classdelete    |     No    |
 *    +--------------------+-----------+
 */

#include "api/ofa-igetter-def.h"
#include "api/ofa-tvbin.h"
#include "api/ofo-class-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_CLASS_TREEVIEW                ( ofa_class_treeview_get_type())
#define OFA_CLASS_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_CLASS_TREEVIEW, ofaClassTreeview ))
#define OFA_CLASS_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_CLASS_TREEVIEW, ofaClassTreeviewClass ))
#define OFA_IS_CLASS_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_CLASS_TREEVIEW ))
#define OFA_IS_CLASS_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_CLASS_TREEVIEW ))
#define OFA_CLASS_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_CLASS_TREEVIEW, ofaClassTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaClassTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaClassTreeviewClass;

GType             ofa_class_treeview_get_type     ( void ) G_GNUC_CONST;

ofaClassTreeview *ofa_class_treeview_new          ( ofaIGetter *getter,
															const gchar *settings_prefix );

void              ofa_class_treeview_setup_columns( ofaClassTreeview *view );

void              ofa_class_treeview_setup_store  ( ofaClassTreeview *view );

ofoClass         *ofa_class_treeview_get_selected ( ofaClassTreeview *view );

G_END_DECLS

#endif /* __OFA_CLASS_TREEVIEW_H__ */
