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

#ifndef __OPENBOOK_API_OFA_FILE_DIR_H__
#define __OPENBOOK_API_OFA_FILE_DIR_H__

/**
 * SECTION: ofa_file_dir
 * @short_description: #ofaFileDir class definition.
 * @include: openbook/ofa-file-dir.h
 *
 * This class manages the dossiers directory.
 * It is defined to be implemented as a singleton by any program of the
 * Openbook software suite. It takes care of maintaining itself up-to-
 * date.
 *
 * The instance sends a 'changed' signal when the directory changes.
 *
 * This is an Openbook software suite decision to have the dossiers
 * directory stored in a single dedicated ini file, said dossiers
 * settings.
 */

#include "api/ofa-ifile-meta.h"

G_BEGIN_DECLS

#define OFA_TYPE_FILE_DIR                ( ofa_file_dir_get_type())
#define OFA_FILE_DIR( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_FILE_DIR, ofaFileDir ))
#define OFA_FILE_DIR_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_FILE_DIR, ofaFileDirClass ))
#define OFA_IS_FILE_DIR( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_FILE_DIR ))
#define OFA_IS_FILE_DIR_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_FILE_DIR ))
#define OFA_FILE_DIR_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_FILE_DIR, ofaFileDirClass ))

typedef struct _ofaFileDirPrivate        ofaFileDirPrivate;

typedef struct {
	/*< public members >*/
	GObject            parent;

	/*< private members >*/
	ofaFileDirPrivate *priv;
}
	ofaFileDir;

typedef struct {
	/*< public members >*/
	GObjectClass       parent;
}
	ofaFileDirClass;

GType       ofa_file_dir_get_type           ( void ) G_GNUC_CONST;

ofaFileDir *ofa_file_dir_new                ( void );

GList      *ofa_file_dir_get_dossiers       ( ofaFileDir *dir );

#define     ofa_file_dir_free_dossiers(L)   g_list_free_full(( L ), \
													( GDestroyNotify ) g_object_unref )

guint       ofa_file_dir_get_dossiers_count ( const ofaFileDir *dir );

GList      *ofa_file_dir_get_dossier_periods( const ofaFileDir *dir,
													const ofaIFileMeta *id );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_FILE_DIR_H__ */
