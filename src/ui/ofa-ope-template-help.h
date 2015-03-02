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
 */

#ifndef __OFA_OPE_TEMPLATE_HELP_H__
#define __OFA_OPE_TEMPLATE_HELP_H__

/**
 * SECTION: ofa_ope_template_help
 * @short_description: #ofaOpeTemplateHelp class definition.
 * @include: ui/ofa-ope-template-help.h
 *
 * A non-modal dialog box which display an help about operation
 * templates.
 */

#include "core/ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_OPE_TEMPLATE_HELP                ( ofa_ope_template_help_get_type())
#define OFA_OPE_TEMPLATE_HELP( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelp ))
#define OFA_OPE_TEMPLATE_HELP_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelpClass ))
#define OFA_IS_OPE_TEMPLATE_HELP( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_OPE_TEMPLATE_HELP ))
#define OFA_IS_OPE_TEMPLATE_HELP_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_OPE_TEMPLATE_HELP ))
#define OFA_OPE_TEMPLATE_HELP_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_OPE_TEMPLATE_HELP, ofaOpeTemplateHelpClass ))

typedef struct _ofaOpeTemplateHelpPrivate         ofaOpeTemplateHelpPrivate;

typedef struct {
	/*< public members >*/
	myDialog                   parent;

	/*< private members >*/
	ofaOpeTemplateHelpPrivate *priv;
}
	ofaOpeTemplateHelp;

typedef struct {
	/*< public members >*/
	myDialogClass              parent;
}
	ofaOpeTemplateHelpClass;

GType               ofa_ope_template_help_get_type( void ) G_GNUC_CONST;

ofaOpeTemplateHelp *ofa_ope_template_help_run     ( const ofaMainWindow *main_window );

void                ofa_ope_template_help_close   ( ofaOpeTemplateHelp *help );

G_END_DECLS

#endif /* __OFA_OPE_TEMPLATE_HELP_H__ */
