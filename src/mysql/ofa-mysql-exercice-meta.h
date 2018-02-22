/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_EXERCICE_META_H__
#define __OFA_MYSQL_EXERCICE_META_H__

/**
 * SECTION: ofa_mysql_exercice_meta
 * @short_description: #ofaMysqlExerciceMeta class definition.
 * @include: mysql/ofa-mysql-exercice-meta.h
 *
 * A class to manage financial periods defined for a dossier.
 * In particular, it implements #ofaIDBExerciceMeta interface.
 */

#include "my/my-isettings.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_EXERCICE_META                ( ofa_mysql_exercice_meta_get_type())
#define OFA_MYSQL_EXERCICE_META( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_EXERCICE_META, ofaMysqlExerciceMeta ))
#define OFA_MYSQL_EXERCICE_META_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_EXERCICE_META, ofaMysqlExerciceMetaClass ))
#define OFA_IS_MYSQL_EXERCICE_META( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_EXERCICE_META ))
#define OFA_IS_MYSQL_EXERCICE_META_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_EXERCICE_META ))
#define OFA_MYSQL_EXERCICE_META_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_EXERCICE_META, ofaMysqlExerciceMetaClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlExerciceMeta;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlExerciceMetaClass;

GType                 ofa_mysql_exercice_meta_get_type         ( void ) G_GNUC_CONST;

ofaMysqlExerciceMeta *ofa_mysql_exercice_meta_new              ( void );

const gchar          *ofa_mysql_exercice_meta_get_database     ( ofaMysqlExerciceMeta *period );

void                  ofa_mysql_exercice_meta_set_database     ( ofaMysqlExerciceMeta *period,
																		const gchar *database );

G_END_DECLS

#endif /* __OFA_MYSQL_EXERCICE_META_H__ */
