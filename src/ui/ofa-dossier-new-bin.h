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
 */

#ifndef __OFA_DOSSIER_NEW_BIN_H__
#define __OFA_DOSSIER_NEW_BIN_H__

/**
 * SECTION: ofa_dossier_new_bin
 * @short_description: #ofaDossierNewBin class definition.
 * @include: ui/ofa-dossier-new-bin.h
 *
 * Let the user define a new dossier, selecting the DBMS provider and
 * its connection properties, registering it in the settings.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'ofa-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbeditor.h"
#include "api/ofa-idbmeta-def.h"

#include "core/ofa-dbms-root-bin.h"

#include "ofa-main-window-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW_BIN                ( ofa_dossier_new_bin_get_type())
#define OFA_DOSSIER_NEW_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBin ))
#define OFA_DOSSIER_NEW_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBinClass ))
#define OFA_IS_DOSSIER_NEW_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW_BIN ))
#define OFA_IS_DOSSIER_NEW_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW_BIN ))
#define OFA_DOSSIER_NEW_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBinClass ))

typedef struct _ofaDossierNewBinPrivate         ofaDossierNewBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                   parent;

	/*< private members >*/
	ofaDossierNewBinPrivate *priv;
}
	ofaDossierNewBin;

typedef struct {
	/*< public members >*/
	GtkBinClass              parent;
}
	ofaDossierNewBinClass;

GType             ofa_dossier_new_bin_get_type             ( void ) G_GNUC_CONST;

ofaDossierNewBin *ofa_dossier_new_bin_new                  ( ofaMainWindow *main_window );

GtkSizeGroup     *ofa_dossier_new_bin_get_size_group       ( const ofaDossierNewBin *bin,
																	guint column );

void              ofa_dossier_new_bin_set_default_provider ( ofaDossierNewBin *bin );

void              ofa_dossier_new_bin_set_provider         ( ofaDossierNewBin *bin,
																	const gchar *provider );

gboolean          ofa_dossier_new_bin_get_valid            ( const ofaDossierNewBin *bin,
																	gchar **error_message );

ofaIDBMeta       *ofa_dossier_new_bin_apply                ( const ofaDossierNewBin *bin );

void              ofa_dossier_new_bin_get_dname            ( const ofaDossierNewBin *bin,
																	gchar **dname );

ofaDBMSRootBin   *ofa_dossier_new_bin_get_dbms_root_bin    ( const ofaDossierNewBin *bin );

ofaIDBEditor     *ofa_dossier_new_bin_get_editor           ( const ofaDossierNewBin *bin );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_BIN_H__ */
