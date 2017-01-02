/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MYSQL_DOSSIER_META_H__
#define __OFA_MYSQL_DOSSIER_META_H__

/**
 * SECTION: ofa_mysql_dossier_meta
 * @short_description: #ofaMysqlDossierMeta class definition.
 * @include: mysql/ofa-mysql-dossier-meta.h
 *
 * A class to manage dossier identification and other external
 * properties. In particular, it implements #ofaIDBDossierMeta interface.
 */

#include <glib-object.h>

#include "mysql/ofa-mysql-dossier-bin.h"
#include "mysql/ofa-mysql-root-bin.h"

G_BEGIN_DECLS

#define OFA_TYPE_MYSQL_DOSSIER_META                ( ofa_mysql_dossier_meta_get_type())
#define OFA_MYSQL_DOSSIER_META( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MYSQL_DOSSIER_META, ofaMysqlDossierMeta ))
#define OFA_MYSQL_DOSSIER_META_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MYSQL_DOSSIER_META, ofaMysqlDossierMetaClass ))
#define OFA_IS_MYSQL_DOSSIER_META( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MYSQL_DOSSIER_META ))
#define OFA_IS_MYSQL_DOSSIER_META_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MYSQL_DOSSIER_META ))
#define OFA_MYSQL_DOSSIER_META_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MYSQL_DOSSIER_META, ofaMysqlDossierMetaClass ))

typedef struct {
	/*< public members >*/
	GObject      parent;
}
	ofaMysqlDossierMeta;

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaMysqlDossierMetaClass;

GType                ofa_mysql_dossier_meta_get_type        ( void ) G_GNUC_CONST;

ofaMysqlDossierMeta *ofa_mysql_dossier_meta_new             ( void );

const gchar         *ofa_mysql_dossier_meta_get_host        ( ofaMysqlDossierMeta *meta );

guint                ofa_mysql_dossier_meta_get_port        ( ofaMysqlDossierMeta *meta );

const gchar         *ofa_mysql_dossier_meta_get_socket      ( ofaMysqlDossierMeta *meta );

const gchar         *ofa_mysql_dossier_meta_get_root_account( ofaMysqlDossierMeta *meta );

void                 ofa_mysql_dossier_meta_load_periods    ( ofaMysqlDossierMeta *meta,
																	GList *keys );

void                 ofa_mysql_dossier_meta_add_period      ( ofaMysqlDossierMeta *meta,
																	gboolean current,
																	const GDate *begin,
																	const GDate *end,
																	const gchar *database );

void                 ofa_mysql_dossier_meta_set_from_editor ( ofaMysqlDossierMeta *meta,
																	ofaMysqlDossierBin *dossier_bin,
																	ofaMysqlRootBin *root_bin );

G_END_DECLS

#endif /* __OFA_MYSQL_DOSSIER_META_H__ */
