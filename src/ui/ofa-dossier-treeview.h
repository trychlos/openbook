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

#ifndef __OFA_DOSSIER_TREEVIEW_H__
#define __OFA_DOSSIER_TREEVIEW_H__

/**
 * SECTION: ofa_dossier_treeview
 * @short_description: #ofaDossierTreeview class definition.
 * @include: ui/ofa-dossier-treeview.h
 *
 * Manage a treeview with the list of the dossiers which are defined
 * in the settings.
 *
 * The class provides the following signals, which are proxyed from
 * #ofaTVBin base class:
 *    +------------------+--------------+
 *    | Signal           | Argument may |
 *    |                  | be %NULL     |
 *    +------------------+--------------+
 *    | ofa-doschanged   |      Yes     |
 *    | ofa-dosactivated |       No     |
 *    | ofa-dosdelete    |       No     |
 *    +------------------+--------------+
 */

#include "api/ofa-idbmeta-def.h"
#include "api/ofa-idbperiod.h"
#include "api/ofa-tvbin.h"

#include "ui/ofa-dossier-store.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_TREEVIEW                ( ofa_dossier_treeview_get_type())
#define OFA_DOSSIER_TREEVIEW( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeview ))
#define OFA_DOSSIER_TREEVIEW_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeviewClass ))
#define OFA_IS_DOSSIER_TREEVIEW( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_TREEVIEW ))
#define OFA_IS_DOSSIER_TREEVIEW_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_TREEVIEW ))
#define OFA_DOSSIER_TREEVIEW_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_TREEVIEW, ofaDossierTreeviewClass ))

typedef struct {
	/*< public members >*/
	ofaTVBin      parent;
}
	ofaDossierTreeview;

typedef struct {
	/*< public members >*/
	ofaTVBinClass parent;
}
	ofaDossierTreeviewClass;

GType               ofa_dossier_treeview_get_type        ( void ) G_GNUC_CONST;

ofaDossierTreeview *ofa_dossier_treeview_new             ( void );

void                ofa_dossier_treeview_set_settings_key( ofaDossierTreeview *view,
																const gchar *key );

void                ofa_dossier_treeview_setup_columns   ( ofaDossierTreeview *view );

gboolean            ofa_dossier_treeview_get_selected    ( ofaDossierTreeview *view,
																ofaIDBMeta **meta,
																ofaIDBPeriod **period );

void                ofa_dossier_treeview_set_selected    ( ofaDossierTreeview *view,
																const gchar *dname );

void                ofa_dossier_treeview_set_show_all    ( ofaDossierTreeview *view,
																gboolean show_all );

void                ofa_dossier_treeview_setup_store     ( ofaDossierTreeview *view );

G_END_DECLS

#endif /* __OFA_DOSSIER_TREEVIEW_H__ */
