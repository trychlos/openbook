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

#ifndef __OFA_TVA_RECORD_NEW_H__
#define __OFA_TVA_RECORD_NEW_H__

/**
 * SECTION: ofa_tva_record_new
 * @short_description: #ofaTVARecordNew class definition.
 * @include: ui/ofa-tva-record-new.h
 *
 * Enter the end date of a TVA declaration.
 *
 * The only target of this dialog is to let the user enter an end
 * date for a new TVA declaration. This end date is then unmodifiable
 * (the declaration has to be deleted before recreated).
 *
 * Development rules:
 * - type:       dialog
 * - settings:   yes
 * - current:    yes
 */

#include "api/my-dialog.h"
#include "api/ofa-main-window-def.h"

#include "tva/ofo-tva-record.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_RECORD_NEW                ( ofa_tva_record_new_get_type())
#define OFA_TVA_RECORD_NEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNew ))
#define OFA_TVA_RECORD_NEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNewClass ))
#define OFA_IS_TVA_RECORD_NEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_RECORD_NEW ))
#define OFA_IS_TVA_RECORD_NEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_RECORD_NEW ))
#define OFA_TVA_RECORD_NEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNewClass ))

typedef struct _ofaTVARecordNewPrivate         ofaTVARecordNewPrivate;

typedef struct {
	/*< public members >*/
	myDialog                parent;

	/*< private members >*/
	ofaTVARecordNewPrivate *priv;
}
	ofaTVARecordNew;

typedef struct {
	/*< public members >*/
	myDialogClass           parent;
}
	ofaTVARecordNewClass;

GType    ofa_tva_record_new_get_type( void ) G_GNUC_CONST;

gboolean ofa_tva_record_new_run     ( const ofaMainWindow *main_window,
														ofoTVARecord *record );

G_END_DECLS

#endif /* __OFA_TVA_RECORD_NEW_H__ */
