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

#ifndef __OFA_OPE_TEMPLATE_HELP_H__
#define __OFA_OPE_TEMPLATE_HELP_H__

/**
 * SECTION: ofa_ope_template_help
 * @short_description: #ofaOpeTemplateHelp class definition.
 * @include: core/ofa-ope-template-help.h
 *
 * A non-modal dialog box which display an help about operation
 * templates.
 *
 * Each ofaOpeTemplateProperties dialog is able to ask for opening this
 * dialog. This dialog makes itself unique (is managed as a singleton),
 * and takes care of auto-closing itself when the last
 * ofaOpeTemplateProperties dialog is closed.
 *
 * See api/ofo-ope-template.h for a full description of the model language.
 *
 * Development rules:
 * - type:       unique non-modal dialog
 * - settings:   yes
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_HELP                ( ofa_ope_template_help_get_type())
#define OFA_OPE_TEMPLATE_HELP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelp ))
#define OFA_OPE_TEMPLATE_HELP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelpClass ))
#define OFA_IS_OPE_TEMPLATE_HELP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_HELP ))
#define OFA_IS_OPE_TEMPLATE_HELP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_HELP ))
#define OFA_OPE_TEMPLATE_HELP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelpClass ))

typedef struct {
	/*< public members >*/
	GtkDialog      parent;
}
	ofaOpeTemplateHelp;

typedef struct {
	/*< public members >*/
	GtkDialogClass parent;
}
	ofaOpeTemplateHelpClass;

GType ofa_ope_template_help_get_type( void ) G_GNUC_CONST;

void  ofa_ope_template_help_run     ( ofaIGetter *getter,
											GtkWindow *parent );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_HELP_H__ */
