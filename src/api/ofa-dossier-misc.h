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

#ifndef __OPENBOOK_API_OFA_DOSSIER_MISC_H__
#define __OPENBOOK_API_OFA_DOSSIER_MISC_H__

/**
 * SECTION: ofa_dossier_Misc
 * @short_description: Dossier miscellaneous functions
 * @include: openbook/ofa-dossier-misc.h
 *
 * Delete a dossier.
 */

#include "api/ofa-file-format.h"
#include "api/ofa-iimportable.h"
#include "api/ofo-base-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

guint     ofa_dossier_misc_import_csv        ( ofoDossier *dossier,
													ofaIImportable *object,
													const gchar *uri,
													const ofaFileFormat *settings,
													void *caller,
													guint *errors);

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_DOSSIER_MISC_H__ */
