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

#ifndef __OFA_DOSSIER_PERIOD_H__
#define __OFA_DOSSIER_PERIOD_H__

/**
 * SECTION: ofa_dossier_period
 * @short_description: #ofaDossierPeriod class definition.
 * @include: core/ofa-dossier-period.h
 *
 * A class to manage dossiers financial periods. In particular, it
 * implements #ofaIFilePeriod interface.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_PERIOD                ( ofa_dossier_period_get_type())
#define OFA_DOSSIER_PERIOD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_PERIOD, ofaDossierPeriod ))
#define OFA_DOSSIER_PERIOD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_PERIOD, ofaDossierPeriodClass ))
#define OFA_IS_DOSSIER_PERIOD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_PERIOD ))
#define OFA_IS_DOSSIER_PERIOD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_PERIOD ))
#define OFA_DOSSIER_PERIOD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_PERIOD, ofaDossierPeriodClass ))

typedef struct _ofaDossierPeriodPrivate        ofaDossierPeriodPrivate;

typedef struct {
	/*< public members >*/
	GObject                  parent;

	/*< private members >*/
	ofaDossierPeriodPrivate *priv;
}
	ofaDossierPeriod;

typedef struct {
	/*< public members >*/
	GObjectClass             parent;
}
	ofaDossierPeriodClass;

GType             ofa_dossier_period_get_type      ( void ) G_GNUC_CONST;

ofaDossierPeriod *ofa_dossier_period_new           ( void );

void              ofa_dossier_period_set_begin_date( ofaDossierPeriod *period,
															const GDate *date );

void              ofa_dossier_period_set_end_date  ( ofaDossierPeriod *period,
															const GDate *date );

void              ofa_dossier_period_set_current   ( ofaDossierPeriod *period,
															gboolean current );

G_END_DECLS

#endif /* __OFA_DOSSIER_PERIOD_H__ */
