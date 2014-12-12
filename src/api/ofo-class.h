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

#ifndef __OFO_CLASS_H__
#define __OFO_CLASS_H__

/**
 * SECTION: ofo_class
 * @short_description: #ofoClass class definition.
 * @include: api/ofo-class.h
 *
 * This file defines the #ofoClass public API.
 *
 * Note that no method is provided for inserting or deleting a row in
 * the sgbd. The dossier comes with 9 predefined classes. The user may
 * freely modify their label, but there is no sense in adding/removing
 * any class.
 */

#include "api/ofo-class-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

GList          *ofo_class_get_dataset    ( ofoDossier *dossier );
ofoClass       *ofo_class_get_by_number  ( ofoDossier *dossier, gint number );

ofoClass       *ofo_class_new            ( void );

gint            ofo_class_get_number     ( const ofoClass *class );
const gchar    *ofo_class_get_label      ( const ofoClass *class );
const gchar    *ofo_class_get_notes      ( const ofoClass *class );
const gchar    *ofo_class_get_upd_user   ( const ofoClass *class );
const GTimeVal *ofo_class_get_upd_stamp  ( const ofoClass *class );

gboolean        ofo_class_is_valid       ( gint number, const gchar *label );
gboolean        ofo_class_is_valid_number( gint number );
gboolean        ofo_class_is_valid_label ( const gchar *label );
gboolean        ofo_class_is_deletable   ( const ofoClass *class, ofoDossier *dossier );

void            ofo_class_set_number     ( ofoClass *class, gint number );
void            ofo_class_set_label      ( ofoClass *class, const gchar *label );
void            ofo_class_set_notes      ( ofoClass *class, const gchar *notes );

gboolean        ofo_class_insert         ( ofoClass *class, ofoDossier *dossier );
gboolean        ofo_class_update         ( ofoClass *class, ofoDossier *dossier, gint prev_id );
gboolean        ofo_class_delete         ( ofoClass *class, ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_CLASS_H__ */
