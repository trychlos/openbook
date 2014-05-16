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

#ifndef __OFA_MOD_FAMILIES_SET_H__
#define __OFA_MOD_FAMILIES_SET_H__

/**
 * SECTION: ofa_mod_families_set
 * @short_description: #ofaModFamiliesSet class definition.
 * @include: ui/ofa-mod_families-set.h
 *
 * Families of entry models.
 */

#include "ui/ofa-main-page.h"

G_BEGIN_DECLS

#define OFA_TYPE_MOD_FAMILIES_SET                ( ofa_mod_families_set_get_type())
#define OFA_MOD_FAMILIES_SET( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MOD_FAMILIES_SET, ofaModFamiliesSet ))
#define OFA_MOD_FAMILIES_SET_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MOD_FAMILIES_SET, ofaModFamiliesSetClass ))
#define OFA_IS_MOD_FAMILIES_SET( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MOD_FAMILIES_SET ))
#define OFA_IS_MOD_FAMILIES_SET_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MOD_FAMILIES_SET ))
#define OFA_MOD_FAMILIES_SET_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MOD_FAMILIES_SET, ofaModFamiliesSetClass ))

typedef struct _ofaModFamiliesSetPrivate        ofaModFamiliesSetPrivate;

typedef struct {
	/*< private >*/
	ofaMainPage               parent;
	ofaModFamiliesSetPrivate *private;
}
	ofaModFamiliesSet;

typedef struct _ofaModFamiliesSetClassPrivate   ofaModFamiliesSetClassPrivate;

typedef struct {
	/*< private >*/
	ofaMainPageClass               parent;
	ofaModFamiliesSetClassPrivate *private;
}
	ofaModFamiliesSetClass;

GType ofa_mod_families_set_get_type( void );

void  ofa_mod_families_set_run     ( ofaMainPage *this );

G_END_DECLS

#endif /* __OFA_MOD_FAMILIES_SET_H__ */
