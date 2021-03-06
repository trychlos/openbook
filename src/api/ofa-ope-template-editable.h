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

#ifndef __OPENBOOK_API_UI_OFA_OPE_TEMPLATE_EDITABLE_H__
#define __OPENBOOK_API_UI_OFA_OPE_TEMPLATE_EDITABLE_H__

/**
 * SECTION: ope_template_editable
 * @title: ofaOpeTemplateEditable
 * @short_description: The ofa_ope_template_editable set of functions.
 * @include: openbook/ofa-ope-template-editable.h
 *
 * The ofa_ope_template_editable set of functions lets the user enter
 * and select operation templates in the provided GtkEntry.
 *
 * Just call #ofa_ope_template_editable_init() with each GtkEntry you want
 * set, and the function will take care of setting an icon, triggering
 * #ofaOpeTemplateSelect dialog for selection.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

void  ofa_ope_template_editable_init( GtkEditable *editable,
											ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_EDITABLE_H__ */
