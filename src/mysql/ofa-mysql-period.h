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

#ifndef __OFA_MYSQL_PERIOD_H__
#define __OFA_MYSQL_PERIOD_H__

/**
 * SECTION: ofa_mysql_period
 * @short_description: #ofaMySQLPeriod class definition.
 * @include: mysql/ofa-mysql-period.h
 *
 * A class to manage financial periods defined for a dossier.
 * In particular, it implements #ofaIFilePeriod interface.
 */

#include "ofa-mysql-idbprovider.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_PERIOD                ( ofa_mysql_period_get_type())
#define OFA_MYSQL_PERIOD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_PERIOD, ofaMySQLPeriod ))
#define OFA_MYSQL_PERIOD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_PERIOD, ofaMySQLPeriodClass ))
#define OFA_IS_MYSQL_PERIOD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_PERIOD ))
#define OFA_IS_MYSQL_PERIOD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_PERIOD ))
#define OFA_MYSQL_PERIOD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_PERIOD, ofaMySQLPeriodClass ))

typedef struct _ofaMySQLPeriodPrivate        ofaMySQLPeriodPrivate;

typedef struct {
	/*< public members >*/
	GObject                parent;

	/*< private members >*/
	ofaMySQLPeriodPrivate *priv;
}
	ofaMySQLPeriod;

typedef struct {
	/*< public members >*/
	GObjectClass           parent;
}
	ofaMySQLPeriodClass;

GType           ofa_mysql_period_get_type( void ) G_GNUC_CONST;

ofaMySQLPeriod *ofa_mysql_period_new     ( gboolean current,
												const GDate *begin,
												const GDate *end,
												const gchar *database_name );

G_END_DECLS

#endif /* __OFA_MYSQL_PERIOD_H__ */
