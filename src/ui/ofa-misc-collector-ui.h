/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_MISC_COLLECTOR_UI_H__
#define __OFA_MISC_COLLECTOR_UI_H__

/**
 * SECTION: ofa-misccollector_ui
 * @include: ui/ofa-misc-collector-ui.h
 *
 * Display #myICollector content.
 *
 * Note that has to be a dialog as ofaPage are only available inside
 * of the main notebook, which itself is only created when a dossier
 * is opened.
 *
 * Development rules:
 * - type:       non-modal dialog
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_MISC_COLLECTOR_UI                ( ofa_misc_collector_ui_get_type())
#define OFA_MISC_COLLECTOR_UI( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_MISC_COLLECTOR_UI, ofaMiscCollectorUI ))
#define OFA_MISC_COLLECTOR_UI_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_MISC_COLLECTOR_UI, ofaMiscCollectorUIClass ))
#define OFA_IS_MISC_COLLECTOR_UI( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_MISC_COLLECTOR_UI ))
#define OFA_IS_MISC_COLLECTOR_UI_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_MISC_COLLECTOR_UI ))
#define OFA_MISC_COLLECTOR_UI_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_MISC_COLLECTOR_UI, ofaMiscCollectorUIClass ))

typedef struct {
	/*< public members >*/
	GtkDialog         parent;
}
	ofaMiscCollectorUI;

typedef struct {
	/*< public members >*/
	GtkDialogClass    parent;
}
	ofaMiscCollectorUIClass;

GType ofa_misc_collector_ui_get_type( void ) G_GNUC_CONST;

void  ofa_misc_collector_ui_run     ( ofaIGetter *getter );

G_END_DECLS

#endif /* __OFA_MISC_COLLECTOR_UI_H__ */
