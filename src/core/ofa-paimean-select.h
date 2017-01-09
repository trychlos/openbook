/*
 * Open Firm Paimeaning
 * A double-entry paimeaning application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Paimeaning is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Paimeaning is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Paimeaning; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OFA_PAIMEAN_SELECT_H__
#define __OFA_PAIMEAN_SELECT_H__

/**
 * SECTION: ofa_paimean_select
 * @short_description: #ofaPaimeanSelect class definition.
 * @include: core/ofa-paimean-select.h
 *
 * Display the means of paiement, letting the user select one.
 *
 * Development rules:
 * - type:       modal dialog
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_PAIMEAN_SELECT                ( ofa_paimean_select_get_type())
#define OFA_PAIMEAN_SELECT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PAIMEAN_SELECT, ofaPaimeanSelect ))
#define OFA_PAIMEAN_SELECT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PAIMEAN_SELECT, ofaPaimeanSelectClass ))
#define OFA_IS_PAIMEAN_SELECT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PAIMEAN_SELECT ))
#define OFA_IS_PAIMEAN_SELECT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PAIMEAN_SELECT ))
#define OFA_PAIMEAN_SELECT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PAIMEAN_SELECT, ofaPaimeanSelectClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaPaimeanSelect;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaPaimeanSelectClass;

GType  ofa_paimean_select_get_type( void ) G_GNUC_CONST;

gchar *ofa_paimean_select_run     ( ofaIGetter *getter,
											GtkWindow *parent,
											const gchar *asked_code );

G_END_DECLS

#endif /* __OFA_PAIMEAN_SELECT_H__ */
