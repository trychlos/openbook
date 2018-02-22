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

#ifndef __OPENBOOK_API_OFA_IMPORT_DUPLICATE_H__
#define __OPENBOOK_API_OFA_IMPORT_DUPLICATE_H__

/**
 * SECTION: ofa_import_duplicate
 * @title: ofaImportDuplicate
 * @short_description: The #ofaImportDuplicate data definition
 * @include: openbook/ofa-import-duplicate.h
 *
 * Import mode manages the import behavior.
 */

#include <glib.h>

G_BEGIN_DECLS

/**
 * ofeImportDuplicate:
 * @OFA_IDUPLICATE_REPLACE: replace duplicate records.
 * @OFA_IDUPLICATE_IGNORE: ignore duplicate records (do not replace).
 * @OFA_IDUPLICATE_ABORT: abort on duplicate.
 *
 * What to do with duplicate imported datas ?
 *
 */
typedef enum {
	OFA_IDUPLICATE_REPLACE = 1,				/* let this first element = 1 (limit of validity) */
	OFA_IDUPLICATE_IGNORE,
	OFA_IDUPLICATE_ABORT
}
	ofeImportDuplicate;

/**
 * ImportDuplicateEnumCb:
 *
 * The #ofa_import_duplicate_enum() callback.
 */
typedef void ( *ImportDuplicateEnumCb )( ofeImportDuplicate mode, const gchar *label, void *user_data );

gchar *ofa_import_duplicate_get_label( ofeImportDuplicate mode );

void   ofa_import_duplicate_enum     ( ImportDuplicateEnumCb fn,
											void *user_data );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IMPORT_DUPLICATE_H__ */
