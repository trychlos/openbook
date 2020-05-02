/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_HUB_DEF_H__
#define __OPENBOOK_API_OFA_HUB_DEF_H__

/**
 * SECTION: ofahub
 * @include: openbook/ofa-hub-def.h
 */

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ofaHub                  ofaHub;

/**
 * @ofaDataCb: a general usage callback to pass some datas;
 *  on input, the #ofaDataCb callback function should return the actual
 *  size of data available in the buffer, 0 at end, <0 for an error;
 *  on output, the #ofaDataCb should return the actual size the callback
 *  has been successfully dealt with.
 * @ofaMsgCb: a general usage callback to display a message.
 *  this callback in only used on output.
 */
typedef gsize ( *ofaDataCb )( void *, gsize, void * );
typedef void  ( *ofaMsgCb ) ( const gchar *, void * );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_HUB_DEF_H__ */
