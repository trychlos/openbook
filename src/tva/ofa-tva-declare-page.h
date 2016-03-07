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

#ifndef __OFA_TVA_DECLARE_PAGE_H__
#define __OFA_TVA_DECLARE_PAGE_H__

/**
 * SECTION: ofa_tva_declare_page
 * @short_description: #ofaTVADeclarePage class definition.
 * @include: tva/ofa-tva-declare-page.h
 *
 * This is an ofaPage-derived page which shows the list of existing
 * TVA declarations, either current or validated. The user has Update
 * and Delete usual buttons.
 * Defining a new TVA declaration means selecting a TVA form from
 * management page, and clicking 'Declare from form' button.
 */

#include "api/ofa-page-def.h"

#include "tva/ofo-tva-record.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_DECLARE_PAGE                ( ofa_tva_declare_page_get_type())
#define OFA_TVA_DECLARE_PAGE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_DECLARE_PAGE, ofaTVADeclarePage ))
#define OFA_TVA_DECLARE_PAGE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_DECLARE_PAGE, ofaTVADeclarePageClass ))
#define OFA_IS_TVA_DECLARE_PAGE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_DECLARE_PAGE ))
#define OFA_IS_TVA_DECLARE_PAGE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_DECLARE_PAGE ))
#define OFA_TVA_DECLARE_PAGE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_DECLARE_PAGE, ofaTVADeclarePageClass ))

typedef struct _ofaTVADeclarePagePrivate         ofaTVADeclarePagePrivate;

typedef struct {
	/*< public members >*/
	ofaPage      parent;
}
	ofaTVADeclarePage;

typedef struct {
	/*< public members >*/
	ofaPageClass parent;
}
	ofaTVADeclarePageClass;

GType ofa_tva_declare_page_get_type    ( void ) G_GNUC_CONST;

void  ofa_tva_declare_page_set_selected( ofaTVADeclarePage *page,
												ofoTVARecord *record );

G_END_DECLS

#endif /* __OFA_TVA_DECLARE_PAGE_H__ */
