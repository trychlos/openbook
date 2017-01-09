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
 * - type:       non-modal dialog
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

#include "tva/ofo-tva-record.h"

G_BEGIN_DECLS

#define OFA_TYPE_TVA_RECORD_NEW                ( ofa_tva_record_new_get_type())
#define OFA_TVA_RECORD_NEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNew ))
#define OFA_TVA_RECORD_NEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNewClass ))
#define OFA_IS_TVA_RECORD_NEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVA_RECORD_NEW ))
#define OFA_IS_TVA_RECORD_NEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVA_RECORD_NEW ))
#define OFA_TVA_RECORD_NEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVA_RECORD_NEW, ofaTVARecordNewClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaTVARecordNew;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaTVARecordNewClass;

GType ofa_tva_record_new_get_type( void ) G_GNUC_CONST;

void  ofa_tva_record_new_run     ( ofaIGetter *getter,
										GtkWindow *parent,
										ofoTVARecord *record );

G_END_DECLS

#endif /* __OFA_TVA_RECORD_NEW_H__ */
