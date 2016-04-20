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

#ifndef __OPENBOOK_API_OFO_CLASS_H__
#define __OPENBOOK_API_OFO_CLASS_H__

/**
 * SECTION: ofoclass
 * @title: ofoClass
 * @short_description: #ofoClass class definition.
 * @include: openbook/ofo-class.h
 *
 * This file defines the #ofoClass public API.
 *
 * Note that no method is provided for inserting or deleting a row in
 * the sgbd. The dossier comes with 9 predefined classes. The user may
 * freely modify their label, but there is no sense in adding/removing
 * any class.
 */

#include "api/ofa-hub-def.h"
#include "api/ofo-base-def.h"
#include "api/ofo-class-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_CLASS                ( ofo_class_get_type())
#define OFO_CLASS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_CLASS, ofoClass ))
#define OFO_CLASS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_CLASS, ofoClassClass ))
#define OFO_IS_CLASS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_CLASS ))
#define OFO_IS_CLASS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_CLASS ))
#define OFO_CLASS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_CLASS, ofoClassClass ))

#if 0
typedef struct _ofoClass              ofoClass;
#endif

struct _ofoClass {
	/*< public members >*/
	ofoBase      parent;
};

typedef struct {
	/*< public members >*/
	ofoBaseClass parent;
}
	ofoClassClass;

GType           ofo_class_get_type          ( void ) G_GNUC_CONST;

GList          *ofo_class_get_dataset       ( ofaHub *hub );
#define         ofo_class_free_dataset( L ) g_list_free_full(( L ),( GDestroyNotify ) g_object_unref )

ofoClass       *ofo_class_get_by_number     ( ofaHub *hub, gint number );

ofoClass       *ofo_class_new               ( void );

gint            ofo_class_get_number        ( const ofoClass *class );
const gchar    *ofo_class_get_label         ( const ofoClass *class );
const gchar    *ofo_class_get_notes         ( const ofoClass *class );
const gchar    *ofo_class_get_upd_user      ( const ofoClass *class );
const GTimeVal *ofo_class_get_upd_stamp     ( const ofoClass *class );

gboolean        ofo_class_is_valid_data     ( gint number, const gchar *label, gchar **msgerr );
gboolean        ofo_class_is_valid_number   ( gint number );
gboolean        ofo_class_is_valid_label    ( const gchar *label );
gboolean        ofo_class_is_deletable      ( const ofoClass *class );

void            ofo_class_set_number        ( ofoClass *class, gint number );
void            ofo_class_set_label         ( ofoClass *class, const gchar *label );
void            ofo_class_set_notes         ( ofoClass *class, const gchar *notes );

gboolean        ofo_class_insert            ( ofoClass *class, ofaHub *hub );
gboolean        ofo_class_update            ( ofoClass *class, gint prev_id );
gboolean        ofo_class_delete            ( ofoClass *class );

G_END_DECLS

#endif /* __OPENBOOK_API_OFO_CLASS_H__ */
