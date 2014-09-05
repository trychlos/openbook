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

#ifndef __OFA_MODEL_PROPERTIES_H__
#define __OFA_MODEL_PROPERTIES_H__

/**
 * SECTION: ofa_model_properties
 * @short_description: #ofaModelProperties class definition.
 * @include: ui/ofa-model-properties.h
 *
 * Update the model properties.
 *
 * The user is allowed to use some sort of very simple language: each
 * field which begins with an equal '=' sign is supposed to be computed
 * at run time.
 *
 * Columns are named with a letter:
 * - Account: 'A'
 * - Label: 'L'
 * - Debit: 'D':
 * - Credit: 'C'
 *
 * Rows are numbered starting from 1 at the top.
 *
 * So one can do:
 *
 * - e.g. say that entry labels will the same:
 *   enter an entry label in the 'Label' column of the first row, or let
 *   the user enter it at run time;
 *   then enter the '=L1' formula in all successive rows of the model.
 *
 * - e.g. say that the amount will be computed from another row:
 *   the '=D1*TVATS' says that the amount of the field is the product
 *   of the debit of the first row per the rate 'TVATS' which applies
 *   at the date of the entry
 *
 * The simple language implements some useful shortcuts:
 *
 * - the formula '=SOLDE' says that the given entry will balance the
 *   operation; the amount will be computed at run time to be sure
 *   all entries are equally balanced between debit and credit.
 *
 * - the formula '=IDEM' says that the given entry will take the same
 *   value than those of the previous row, same column.
 */

#include "api/ofo-model-def.h"

#include "core/my-dialog.h"

G_BEGIN_DECLS

#define OFA_TYPE_MODEL_PROPERTIES                ( ofa_model_properties_get_type())
#define OFA_MODEL_PROPERTIES( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MODEL_PROPERTIES, ofaModelProperties ))
#define OFA_MODEL_PROPERTIES_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MODEL_PROPERTIES, ofaModelPropertiesClass ))
#define OFA_IS_MODEL_PROPERTIES( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MODEL_PROPERTIES ))
#define OFA_IS_MODEL_PROPERTIES_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MODEL_PROPERTIES ))
#define OFA_MODEL_PROPERTIES_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MODEL_PROPERTIES, ofaModelPropertiesClass ))

typedef struct _ofaModelPropertiesPrivate        ofaModelPropertiesPrivate;

typedef struct {
	/*< private >*/
	myDialog                   parent;
	ofaModelPropertiesPrivate *private;
}
	ofaModelProperties;

typedef struct {
	/*< private >*/
	myDialogClass parent;
}
	ofaModelPropertiesClass;

GType    ofa_model_properties_get_type( void ) G_GNUC_CONST;

gboolean ofa_model_properties_run     ( ofaMainWindow *parent, ofoModel *model, const gchar *journal );

G_END_DECLS

#endif /* __OFA_MODEL_PROPERTIES_H__ */
