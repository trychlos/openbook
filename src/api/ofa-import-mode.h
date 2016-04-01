/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IMPORT_MODE_H__
#define __OPENBOOK_API_OFA_IMPORT_MODE_H__

/**
 * SECTION: ofa_import_mode
 * @title: ofaImportMode
 * @short_description: The #ofaImportMode data definition
 * @include: openbook/ofa-import-mode.h
 *
 * Import mode manages the import behavior.
 */

G_BEGIN_DECLS

/**
 * ofeImportMode:
 * @OFA_IMMODE_REPLACE: replace duplicate records.
 * @OFA_IMMODE_IGNORE: ignore duplicate records (do not replace).
 * @OFA_IMMODE_ABORT: abort on duplicate.
 *
 * What to do with duplicate imported datas ?
 *
 */
typedef enum {
	OFA_IMMODE_REPLACE = 1,				/* let this first element = 1 (limit of validity) */
	OFA_IMMODE_IGNORE,
	OFA_IMMODE_ABORT
}
	ofeImportMode;

/**
 * ImportModeEnumCb:
 *
 * The #ofa_import_mode_enum() callback.
 */
typedef void (*ImportModeEnumCb)( ofeImportMode mode, const gchar *label, void *user_data );

gchar *ofa_import_mode_get_label( ofeImportMode mode );

void   ofa_import_mode_enum     ( ImportModeEnumCb fn,
										void *user_data );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IMPORT_MODE_H__ */
