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

#ifndef __OPENBOOK_API_OFA_BACKUP_PROPS_H__
#define __OPENBOOK_API_OFA_BACKUP_PROPS_H__

/**
 * SECTION: ofaBackupProps
 * @title: ofaBackupProps
 * @short_description: The #ofaBackupProps Class Definition
 * @include: openbook/ofa-backup-props.h
 *
 * The #ofaBackupProps class manages the properties of an archive file.
 *
 * The #ofaBackupProps class implements the ofaIJson interface, and
 * can thus be exported as a JSON string.
 */

#include <my/my-stamp.h>

G_BEGIN_DECLS

#define OFA_TYPE_BACKUP_PROPS                ( ofa_backup_props_get_type())
#define OFA_BACKUP_PROPS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BACKUP_PROPS, ofaBackupProps ))
#define OFA_BACKUP_PROPS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BACKUP_PROPS, ofaBackupPropsClass ))
#define OFA_IS_BACKUP_PROPS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BACKUP_PROPS ))
#define OFA_IS_BACKUP_PROPS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BACKUP_PROPS ))
#define OFA_BACKUP_PROPS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BACKUP_PROPS, ofaBackupPropsClass ))

typedef struct _ofaBackupProps               ofaBackupProps;

struct _ofaBackupProps {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaBackupPropsClass;

GType             ofa_backup_props_get_type       ( void ) G_GNUC_CONST;

ofaBackupProps   *ofa_backup_props_new            ( void );
ofaBackupProps   *ofa_backup_props_new_from_string( const gchar *string );

const gchar      *ofa_backup_props_get_comment    ( ofaBackupProps *props );
void              ofa_backup_props_set_comment    ( ofaBackupProps *props, const gchar *comment );

const myStampVal *ofa_backup_props_get_stamp      ( ofaBackupProps *props );
void              ofa_backup_props_set_stamp      ( ofaBackupProps *props, const myStampVal *stamp );

const gchar      *ofa_backup_props_get_userid     ( ofaBackupProps *props );
void              ofa_backup_props_set_userid     ( ofaBackupProps *props, const gchar *userid );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_BACKUP_PROPS_H__ */
