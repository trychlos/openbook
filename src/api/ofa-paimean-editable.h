/*
 * Open Firm Paimeaning
 * A double-entry paimeaning application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Paimeaning is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Paimeaning is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Paimeaning; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifndef __OPENBOOK_API_UI_OFA_PAIMEAN_EDITABLE_H__
#define __OPENBOOK_API_UI_OFA_PAIMEAN_EDITABLE_H__

/**
 * SECTION: paimean_editable
 * @title: ofaPaimeanEditbale
 * @short_description: The ofa_paimean_editable set of functions.
 * @include: openbook/ofa-paimean-editable.h
 *
 * The ofa_paimean_editable set of functions lets the user enter and
 * select means of paiement in the provided GtkEntry.
 *
 * Just call #ofa_paimean_editable_init() with each GtkEntry you want
 * set, and the function will take care of setting an icon, triggering
 * #ofaPaimeanSelect dialog for paimean selection.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"
#include "api/ofo-paimean.h"

G_BEGIN_DECLS

typedef char * ( *PaimeanPreSelectCb )( GtkEditable *editable, void *user_data );
typedef char * ( *PaimeanPostSelectCb )( GtkEditable *editable, const gchar *selected, void *user_data );

void  ofa_paimean_editable_init             ( GtkEditable *editable,
													ofaIGetter *getter );

void  ofa_paimean_editable_set_preselect_cb ( GtkEditable *editable,
													PaimeanPreSelectCb cb,
													void *user_data );

void  ofa_paimean_editable_set_postselect_cb( GtkEditable *editable,
												PaimeanPostSelectCb cb,
													void *user_data );

G_END_DECLS

#endif /* __OFA_PAIMEAN_EDITABLE_H__ */
