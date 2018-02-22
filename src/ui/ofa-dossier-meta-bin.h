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

#ifndef __OFA_DOSSIER_META_BIN_H__
#define __OFA_DOSSIER_META_BIN_H__

/**
 * SECTION: ofa_dossier_meta_bin
 * @short_description: #ofaDossierMetaBin class definition.
 * @include: ui/ofa-dossier-new-bin.h
 *
 * Let the user define a new dossier, selecting the DBMS provider and
 * its connection properties, registering it in the settings.
 *
 * Development rules:
 * - type:       bin (parent='top')
 * - validation: yes (has 'my-ibin-changed' signal)
 * - settings:   no
 * - current:    no
 */

#include <gtk/gtk.h>

#include "api/ofa-idbdossier-meta-def.h"
#include "api/ofa-idbprovider-def.h"
#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_META_BIN                ( ofa_dossier_meta_bin_get_type())
#define OFA_DOSSIER_META_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_META_BIN, ofaDossierMetaBin ))
#define OFA_DOSSIER_META_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_META_BIN, ofaDossierMetaBinClass ))
#define OFA_IS_DOSSIER_META_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_META_BIN ))
#define OFA_IS_DOSSIER_META_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_META_BIN ))
#define OFA_DOSSIER_META_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_META_BIN, ofaDossierMetaBinClass ))

typedef struct {
	/*< public members >*/
	GtkBin      parent;
}
	ofaDossierMetaBin;

typedef struct {
	/*< public members >*/
	GtkBinClass parent;
}
	ofaDossierMetaBinClass;

GType              ofa_dossier_meta_bin_get_type        ( void ) G_GNUC_CONST;

ofaDossierMetaBin *ofa_dossier_meta_bin_new             ( ofaIGetter *getter,
																const gchar *settings_prefix,
																guint rule );

ofaIDBDossierMeta *ofa_dossier_meta_bin_apply           ( ofaDossierMetaBin *bin );

ofaIDBProvider    *ofa_dossier_meta_bin_get_provider    ( ofaDossierMetaBin *bin );

G_END_DECLS

#endif /* __OFA_DOSSIER_META_BIN_H__ */
