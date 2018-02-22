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

#ifndef __OFA_IMPORTER_TXT_LCL_H__
#define __OFA_IMPORTER_TXT_LCL_H__

/**
 * SECTION: ofa_importer_txt_lcl
 * @short_description: #ofaImporterTxtLcl class definition.
 * @include: importers/ofa-importer-txt-lcl.h
 *
 * LCL Import Bank Account Transaction (BAT) files in tabulated text
 * format.
 *
 * #ofaImporterTxtLcl class is built so that it is able to parse several
 * versions of the file.
 *
 * As of 2014- 6- 1, lines are:
 02/03/2015	\t -150,0	\t Chèque		\t 9192244
 02/03/2015	\t -26,9	\t Carte		\t			\t CB  ASF              27/02/15	\t 0	\t Divers
 02/03/2015	\t -350,0	\t Virement		\t			\t VIR.PERMANENT WIESER, BORIS
 02/03/2015	\t -16,25	\t Prélèvement	\t			\t ABONNEMENT VOTRE FORMULE ZEN
 31/03/2015	\t 68198,61	\t				\t			\t 01800 904778Z
 *
 * Ref may be:
 *  'Carte'        -> 'CB'
 *  'Virement'     -> 'Vir.'
 *  'Prélèvement'  -> 'Pr.'
 *  'Chèque'       -> 'Ch.'
 *  'TIP'          -> 'TIP'
 *
 * The '\t 0 \t Divers' seems to be a couple which comes with CB paiements.
 * None of these two fields are imported here.
 *
 * PWI 2015- 4-15:
 * At least in one case, a file has been downloaded with a badly formatted
 * line as:
 31/12/2014	\t 310,52	\t				\t			\t INTERETS 2014\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00\00
 * This made the file badly imported (and I do not know how to detect and
 * prevent this error).
 *
 * PWI 2016- 4-2:
 * Though this code is ported to new ofaIImporter interface, it is
 * deprecated to the benefit of ofaImporterPdf format.
 */

#include "importers/ofa-importer-txt.h"

G_BEGIN_DECLS

#define OFA_TYPE_IMPORTER_TXT_LCL                ( ofa_importer_txt_lcl_get_type())
#define OFA_IMPORTER_TXT_LCL( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_IMPORTER_TXT_LCL, ofaImporterTxtLcl ))
#define OFA_IMPORTER_TXT_LCL_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_IMPORTER_TXT_LCL, ofaImporterTxtLclClass ))
#define OFA_IS_IMPORTER_TXT_LCL( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_IMPORTER_TXT_LCL ))
#define OFA_IS_IMPORTER_TXT_LCL_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_IMPORTER_TXT_LCL ))
#define OFA_IMPORTER_TXT_LCL_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_IMPORTER_TXT_LCL, ofaImporterTxtLclClass ))

typedef struct {
	/*< public members >*/
	ofaImporterTxt      parent;
}
	ofaImporterTxtLcl;

typedef struct {
	/*< public members >*/
	ofaImporterTxtClass parent;
}
	ofaImporterTxtLclClass;

GType ofa_importer_txt_lcl_get_type( void );

G_END_DECLS

#endif /* __OFA_IMPORTER_TXT_LCL_H__ */
