/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_IMPORTER_TXT_BOURSO_H__
#define __OFA_IMPORTER_TXT_BOURSO_H__

/**
 * SECTION: ofa_importer_txt_bourso
 * @short_description: #ofaImporterTxtBourso class definition.
 * @include: ofa-importer-txt-bourso.h
 *
 * Boursorama Import Bank Account Transaction (BAT) files in tabulated
 * text format.
 */

#include "importers/ofa-importer-txt.h"

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTER_TXT_BOURSO                ( ofa_importer_txt_bourso_get_type())
#define OFA_IMPORTER_TXT_BOURSO( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTER_TXT_BOURSO, ofaImporterTxtBourso ))
#define OFA_IMPORTER_TXT_BOURSO_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTER_TXT_BOURSO, ofaImporterTxtBoursoClass ))
#define OFA_IS_IMPORTER_TXT_BOURSO( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTER_TXT_BOURSO ))
#define OFA_IS_IMPORTER_TXT_BOURSO_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTER_TXT_BOURSO ))
#define OFA_IMPORTER_TXT_BOURSO_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTER_TXT_BOURSO, ofaImporterTxtBoursoClass ))

typedef struct {
	/*< public members >*/
	ofaImporterTxt      parent;
}
	ofaImporterTxtBourso;

typedef struct {
	/*< public members >*/
	ofaImporterTxtClass parent;
}
	ofaImporterTxtBoursoClass;

GType ofa_importer_txt_bourso_get_type( void );

G_END_DECLS

#endif /* __OFA_IMPORTER_TXT_BOURSO_H__ */
