/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_BACKUP_HEADER_H__
#define __OPENBOOK_API_OFA_BACKUP_HEADER_H__

/**
 * SECTION: ofahub
 * @title: ofaBackupHeader
 * @short_description: The #ofaBackupHeader Class Definition
 * @include: openbook/ofa-backup-header.h
 *
 * The #ofaBackupHeader file gathers some convenience functions to manage
 * the headers read from (resp. written into) the archive files.
 *
 * Rationale
 * =========
 *
 * Starting with 0.65, Openbook prefers writes the data archives as a
 * .zip file, because this format is able to host several named data
 * streams.
 *
 * Data archives so contains:
 *
 * - 1..n headers, each containing various reference informations;
 *   each header is named in the archive file as "[HEADER] <name>",
 *   where <name> identifies the header content.
 *
 * - exactly one (should!) data stream named "[DATA] <name>".
 */

#include <archive.h>

#include "api/ofa-hub-def.h"
#include "api/ofa-dossier-props.h"

G_BEGIN_DECLS

/*
 * The prefix of the headers in the archive
 */
#define OFA_BACKUP_HEADER_HEADER        "[HEADER] "
#define OFA_BACKUP_HEADER_DATA          "[DATA] "

gboolean  ofa_backup_header_write_headers   ( ofaHub *hub,
													const gchar *comment,
													struct archive *archive );

GList    *ofa_backup_header_get_headers_list( const gchar *uri );

void     *ofa_backup_header_get_data        ( const gchar *uri,
													const gchar *header_name,
													glong *size );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_BACKUP_HEADER_H__ */
