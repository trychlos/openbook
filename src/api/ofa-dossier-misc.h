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
 *
 * $Id$
 */

#ifndef __OFA_DOSSIER_MISC_H__
#define __OFA_DOSSIER_MISC_H__

/**
 * SECTION: ofa_dossier_Misc
 * @short_description: Dossier mmiscellaneous functions
 * @include: api/ofa-dossier-misc.h
 *
 * Delete a dossier.
 */

#include "api/ofa-dbms.h"
#include "api/ofa-file-format.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

GSList   *ofa_dossier_misc_get_dossiers      ( void );
#define   ofa_dossier_misc_free_dossiers(L)  g_slist_free_full(( L ), ( GDestroyNotify ) g_free )

GSList   *ofa_dossier_misc_get_exercices     ( const gchar *dname );
#define   ofa_dossier_misc_free_exercices(L) g_slist_free_full(( L ), ( GDestroyNotify ) g_free )

gchar    *ofa_dossier_misc_get_exercice_label( const GDate *begin,
													const GDate *end,
													gboolean is_current );

gchar    *ofa_dossier_misc_get_current_dbname( const gchar *dname );

void      ofa_dossier_misc_set_current       ( const gchar *dname,
													const GDate *begin,
													const GDate *end );

void      ofa_dossier_misc_set_new_exercice  ( const gchar *dname,
													const gchar *dbname,
													const GDate *begin_next,
													const GDate *end_next );

guint     ofa_dossier_misc_import_csv        ( ofoDossier *dossier,
													ofaIImportable *object,
													const gchar *uri,
													const ofaFileFormat *settings,
													void *caller,
													guint *errors);

G_END_DECLS

#endif /* __OFA_DOSSIER_MISC_H__ */
