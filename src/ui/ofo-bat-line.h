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

#ifndef __OFO_BAT_LINE_H__
#define __OFO_BAT_LINE_H__

/**
 * SECTION: ofo_bat_line
 * @short_description: #ofoBatLine class definition.
 * @include: ui/ofo-bat-line.h
 *
 * This class implements the BatLine behavior: these are the imported
 * bank account transaction lines.
 */

#include "api/ofa-iimporter.h"
#include "ui/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFO_TYPE_BAT_LINE                ( ofo_bat_line_get_type())
#define OFO_BAT_LINE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_BAT_LINE, ofoBatLine ))
#define OFO_BAT_LINE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_BAT_LINE, ofoBatLineClass ))
#define OFO_IS_BAT_LINE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_BAT_LINE ))
#define OFO_IS_BAT_LINE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_BAT_LINE ))
#define OFO_BAT_LINE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_BAT_LINE, ofoBatLineClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoBatLineClass;

typedef struct _ofoBatLinePrivate        ofoBatLinePrivate;

typedef struct {
	/*< private >*/
	ofoBase            parent;
	ofoBatLinePrivate *private;
}
	ofoBatLine;

GType           ofo_bat_line_get_type     ( void ) G_GNUC_CONST;

ofoBatLine     *ofo_bat_line_new          ( gint bat_id );

GList          *ofo_bat_line_get_dataset  ( const ofoDossier *dossier, gint bat_id );

gint            ofo_bat_line_get_id       ( const ofoBatLine *batline );
gint            ofo_bat_line_get_bat_id   ( const ofoBatLine *batline );
const GDate    *ofo_bat_line_get_valeur   ( const ofoBatLine *batline );
const GDate    *ofo_bat_line_get_ope      ( const ofoBatLine *batline );
const gchar    *ofo_bat_line_get_ref      ( const ofoBatLine *batline );
const gchar    *ofo_bat_line_get_label    ( const ofoBatLine *batline );
const gchar    *ofo_bat_line_get_currency ( const ofoBatLine *batline );
gdouble         ofo_bat_line_get_montant  ( const ofoBatLine *batline );
gint            ofo_bat_line_get_ecr      ( const ofoBatLine *batline );
const gchar    *ofo_bat_line_get_maj_user ( const ofoBatLine *batline );
const GTimeVal *ofo_bat_line_get_maj_stamp( const ofoBatLine *batline );

void            ofo_bat_line_set_id       ( ofoBatLine *batline, gint id );
void            ofo_bat_line_set_valeur   ( ofoBatLine *batline, const GDate *date );
void            ofo_bat_line_set_ope      ( ofoBatLine *batline, const GDate *date );
void            ofo_bat_line_set_ref      ( ofoBatLine *batline, const gchar *ref );
void            ofo_bat_line_set_label    ( ofoBatLine *batline, const gchar *label );
void            ofo_bat_line_set_currency ( ofoBatLine *batline, const gchar *currency );
void            ofo_bat_line_set_montant  ( ofoBatLine *batline, gdouble montant );
void            ofo_bat_line_set_ecr      ( ofoBatLine *batline, gint number );
void            ofo_bat_line_set_maj_user ( ofoBatLine *batline, const gchar *user );
void            ofo_bat_line_set_maj_stamp( ofoBatLine *batline, const GTimeVal *stamp );

gboolean        ofo_bat_line_insert       ( ofoBatLine *batline, const ofoDossier *dossier );
gboolean        ofo_bat_line_update       ( ofoBatLine *batline, const ofoDossier *dossier );

G_END_DECLS

#endif /* __OFO_BAT_LINE_H__ */
