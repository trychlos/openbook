/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_BAT_PAGE_H__
#define __OFA_BAT_PAGE_H__

/**
 * SECTION: ofa_bat_page
 * @short_description: #ofaBatPage class definition.
 * @include: ui/ofa-bat-set.h
 *
 * Display the list of known bat, letting the user edit it.
 *
 * The display treeview is sorted in a the ascending currency code
 * order with insensitive case.
 */

#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_PAGE                ( ofa_bat_page_get_type())
#define OFA_BAT_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_PAGE, ofaBatPage ))
#define OFA_BAT_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_PAGE, ofaBatPageClass ))
#define OFA_IS_BAT_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_PAGE ))
#define OFA_IS_BAT_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_PAGE ))
#define OFA_BAT_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_PAGE, ofaBatPageClass ))

typedef struct {
	/*< public members >*/
	ofaPage      parent;
}
	ofaBatPage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaBatPageClass;

GType ofa_bat_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_BAT_PAGE_H__ */
