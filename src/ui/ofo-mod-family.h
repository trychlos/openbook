/*
 * Open Freelance ModFamilying
 * A double-entry mod_familying application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
 *
 * Open Freelance ModFamilying is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Freelance ModFamilying is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Freelance ModFamilying; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 *
 * $Id$
 */

#ifndef __OFO_MOD_FAMILY_H__
#define __OFO_MOD_FAMILY_H__

/**
 * SECTION: ofo_mod_family
 * @short_description: #ofoModFamily class definition.
 * @include: ui/ofo-mod_family.h
 *
 * This class implements the ModFamily behavior
 */

#include "ui/ofa-sgbd.h"

G_BEGIN_DECLS

#define OFO_TYPE_MOD_FAMILY                ( ofo_mod_family_get_type())
#define OFO_MOD_FAMILY( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_MOD_FAMILY, ofoModFamily ))
#define OFO_MOD_FAMILY_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_MOD_FAMILY, ofoModFamilyClass ))
#define OFO_IS_MOD_FAMILY( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_MOD_FAMILY ))
#define OFO_IS_MOD_FAMILY_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_MOD_FAMILY ))
#define OFO_MOD_FAMILY_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_MOD_FAMILY, ofoModFamilyClass ))

typedef struct _ofoModFamilyPrivate       ofoModFamilyPrivate;

typedef struct {
	/*< private >*/
	GObject              parent;
	ofoModFamilyPrivate *private;
}
	ofoModFamily;

typedef struct _ofoModFamilyClassPrivate  ofoModFamilyClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass              parent;
	ofoModFamilyClassPrivate *private;
}
	ofoModFamilyClass;

GType         ofo_mod_family_get_type     ( void );

ofoModFamily *ofo_mod_family_new          ( void );

GList        *ofo_mod_family_load_set     ( ofaSgbd *sgbd );
void          ofo_mod_family_dump_set     ( GList *chart );

gint          ofo_mod_family_get_id       ( const ofoModFamily *family );
const gchar  *ofo_mod_family_get_label    ( const ofoModFamily *family );
const gchar  *ofo_mod_family_get_notes    ( const ofoModFamily *family );

void          ofo_mod_family_set_id       ( ofoModFamily *family, gint id );
void          ofo_mod_family_set_label    ( ofoModFamily *family, const gchar *label );
void          ofo_mod_family_set_notes    ( ofoModFamily *family, const gchar *notes );
void          ofo_mod_family_set_maj_user ( ofoModFamily *family, const gchar *user );
void          ofo_mod_family_set_maj_stamp( ofoModFamily *family, const GTimeVal *stamp );

gboolean      ofo_mod_family_insert       ( ofoModFamily *family, ofaSgbd *sgbd, const gchar *user );
gboolean      ofo_mod_family_update       ( ofoModFamily *family, ofaSgbd *sgbd, const gchar *user );
gboolean      ofo_mod_family_delete       ( ofoModFamily *family, ofaSgbd *sgbd, const gchar *user );

G_END_DECLS

#endif /* __OFO_MOD_FAMILY_H__ */
