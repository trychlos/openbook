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

#ifndef __OFA_TVA_DEFINE_PAGE_H__
#define __OFA_TVA_DEFINE_PAGE_H__

/**
 * SECTION: ofa_tva_define_page
 * @short_description: #ofaTVADefinePage class definition.
 * @include: tva/ofa-tva-define-page.h
 *
 * Defines a new TVA form.
 * This is an ofaPage-derived page which shows the list of defined
 * forms. The user has New, Update and Delete usual buttons.
 */

#include "api/ofa-page-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_DEFINE_PAGE                ( ofa_tva_define_page_get_type())
#define OFA_TVA_DEFINE_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_DEFINE_PAGE, ofaTVADefinePage ))
#define OFA_TVA_DEFINE_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_DEFINE_PAGE, ofaTVADefinePageClass ))
#define OFA_IS_TVA_DEFINE_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_DEFINE_PAGE ))
#define OFA_IS_TVA_DEFINE_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_DEFINE_PAGE ))
#define OFA_TVA_DEFINE_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_DEFINE_PAGE, ofaTVADefinePageClass ))

typedef struct _ofaTVADefinePagePrivate         ofaTVADefinePagePrivate;

typedef struct {
	/*< public members >*/
	ofaPage                  parent;

	/*< private members >*/
	ofaTVADefinePagePrivate *priv;
}
	ofaTVADefinePage;

typedef struct {
	/*< public members >*/
	ofaPageClass             parent;
}
	ofaTVADefinePageClass;

GType ofa_tva_define_page_get_type( void ) G_GNUC_CONST;

G_END_DECLS

#endif /* __OFA_TVA_DEFINE_PAGE_H__ */