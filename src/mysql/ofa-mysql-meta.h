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

#ifndef __OFA_MYSQL_META_H__
#define __OFA_MYSQL_META_H__

/**
 * SECTION: ofa_mysql_meta
 * @short_description: #ofaMySQLMeta class definition.
 * @include: mysql/ofa-mysql-meta.h
 *
 * A class to manage dossier identification and other external
 * properties. In particular, it implements #ofaIFileMeta interface.
 */

#include "api/my-settings.h"

#include "ofa-mysql-idbprovider.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_META                ( ofa_mysql_meta_get_type())
#define OFA_MYSQL_META( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_META, ofaMySQLMeta ))
#define OFA_MYSQL_META_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_META, ofaMySQLMetaClass ))
#define OFA_IS_MYSQL_META( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_META ))
#define OFA_IS_MYSQL_META_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_META ))
#define OFA_MYSQL_META_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_META, ofaMySQLMetaClass ))

typedef struct _ofaMySQLMetaPrivate        ofaMySQLMetaPrivate;

typedef struct {
	/*< public members >*/
	GObject              parent;

	/*< private members >*/
	ofaMySQLMetaPrivate *priv;
}
	ofaMySQLMeta;

typedef struct {
	/*< public members >*/
	GObjectClass         parent;
}
	ofaMySQLMetaClass;

GType         ofa_mysql_meta_get_type    ( void ) G_GNUC_CONST;

ofaMySQLMeta *ofa_mysql_meta_new         ( void );

void          ofa_mysql_meta_load_periods( ofaMySQLMeta *meta,
												GList *keys );

const gchar  *ofa_mysql_meta_get_host    ( const ofaMySQLMeta *meta );

const gchar  *ofa_mysql_meta_get_socket  ( const ofaMySQLMeta *meta );

guint         ofa_mysql_meta_get_port    ( const ofaMySQLMeta *meta );

void          ofa_mysql_meta_add_period  ( ofaMySQLMeta *meta,
												gboolean current,
												const GDate *begin,
												const GDate *end,
												const gchar *database );

G_END_DECLS

#endif /* __OFA_MYSQL_META_H__ */
