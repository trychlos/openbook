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
 * @include: ui/ofo-class.h
 *
 * This class implements the ofoClass behavior, including the general
 * DB definition.
 *
 * Note that no method is provided for inserting or deleting a row in
 * the sgbd. The dossier comes with 9 predefined classes. The user may
 * freely modify their label, but there is no sense in adding/removing
 * any class.
 */

#include "ui/ofo-class-def.h"
#include "ui/ofo-dossier-def.h"

G_BEGIN_DECLS

GType           ofo_class_get_type       ( void ) G_GNUC_CONST;

GList          *ofo_class_get_dataset    ( const ofoDossier *dossier );
ofoClass       *ofo_class_get_by_number  ( const ofoDossier *dossier, gint number );

ofoClass       *ofo_class_new            ( void );

gint            ofo_class_get_number     ( const ofoClass *class );
const gchar    *ofo_class_get_label      ( const ofoClass *class );
const gchar    *ofo_class_get_notes      ( const ofoClass *class );
const gchar    *ofo_class_get_maj_user   ( const ofoClass *class );
const GTimeVal *ofo_class_get_maj_stamp  ( const ofoClass *class );

gboolean        ofo_class_is_valid       ( gint number, const gchar *label );
gboolean        ofo_class_is_valid_number( gint number );
gboolean        ofo_class_is_valid_label ( const gchar *label );
gboolean        ofo_class_is_deletable   ( const ofoClass *class );

gboolean        ofo_class_set_number     ( ofoClass *class, gint number );
gboolean        ofo_class_set_label      ( ofoClass *class, const gchar *label );
void            ofo_class_set_notes      ( ofoClass *class, const gchar *notes );
void            ofo_class_set_maj_user   ( ofoClass *class, const gchar *user );
void            ofo_class_set_maj_stamp  ( ofoClass *class, const GTimeVal *stamp );

gboolean        ofo_class_insert         ( ofoClass *class );
gboolean        ofo_class_update         ( ofoClass *class, gint prev_id );
gboolean        ofo_class_delete         ( ofoClass *class );

GSList         *ofo_class_get_csv        ( const ofoDossier *dossier );
void            ofo_class_import_csv     ( const ofoDossier *dossier, GSList *lines, gboolean with_header );

G_END_DECLS

#endif /* __OFO_CLASS_H__ */
