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

#ifndef __OFO_TAUX_H__
#define __OFO_TAUX_H__

/**
 * SECTION: ofo_taux
 * @short_description: #ofoTaux class definition.
 * @include: ui/ofo-taux.h
 *
 * This class implements the Taux behavior.
 */

#include "ui/ofo-base.h"
#include "ui/ofo-sgbd.h"

G_BEGIN_DECLS

#define OFO_TYPE_TAUX                ( ofo_taux_get_type())
#define OFO_TAUX( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_TAUX, ofoTaux ))
#define OFO_TAUX_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_TAUX, ofoTauxClass ))
#define OFO_IS_TAUX( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_TAUX ))
#define OFO_IS_TAUX_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_TAUX ))
#define OFO_TAUX_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_TAUX, ofoTauxClass ))

typedef struct {
	/*< private >*/
	ofoBaseClass parent;
}
	ofoTauxClass;

typedef struct _ofoTauxPrivate       ofoTauxPrivate;

typedef struct {
	/*< private >*/
	ofoBase         parent;
	ofoTauxPrivate *priv;
}
	ofoTaux;

GType        ofo_taux_get_type     ( void ) G_GNUC_CONST;

ofoTaux     *ofo_taux_new          ( void );

GList       *ofo_taux_load_set     ( ofoSgbd *sgbd );
void         ofo_taux_dump_set     ( GList *chart );

gint         ofo_taux_get_id       ( const ofoTaux *taux );
const gchar *ofo_taux_get_mnemo    ( const ofoTaux *taux );
const gchar *ofo_taux_get_label    ( const ofoTaux *taux );
const gchar *ofo_taux_get_notes    ( const ofoTaux *taux );
const GDate *ofo_taux_get_val_begin( const ofoTaux *taux );
const GDate *ofo_taux_get_val_end  ( const ofoTaux *taux );
gdouble      ofo_taux_get_taux     ( const ofoTaux *taux );

void         ofo_taux_set_id       ( ofoTaux *taux, gint id );
void         ofo_taux_set_mnemo    ( ofoTaux *taux, const gchar *number );
void         ofo_taux_set_label    ( ofoTaux *taux, const gchar *label );
void         ofo_taux_set_notes    ( ofoTaux *taux, const gchar *notes );
void         ofo_taux_set_val_begin( ofoTaux *taux, const GDate *date );
void         ofo_taux_set_val_end  ( ofoTaux *taux, const GDate *date );
void         ofo_taux_set_taux     ( ofoTaux *taux, gdouble value );
void         ofo_taux_set_maj_user ( ofoTaux *taux, const gchar *user );
void         ofo_taux_set_maj_stamp( ofoTaux *taux, const GTimeVal *stamp );

gboolean     ofo_taux_insert       ( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user );
gboolean     ofo_taux_update       ( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user );
gboolean     ofo_taux_delete       ( ofoTaux *taux, ofoSgbd *sgbd, const gchar *user );

G_END_DECLS

#endif /* __OFO_TAUX_H__ */
