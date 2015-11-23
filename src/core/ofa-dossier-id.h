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

#ifndef __OFA_DOSSIER_ID_H__
#define __OFA_DOSSIER_ID_H__

/**
 * SECTION: ofa_dossier_id
 * @short_description: #ofaDossierId class definition.
 * @include: core/ofa-dossier-id.h
 *
 * A class to manage dossier identification and other external
 * properties. In particular, it implements #ofaIFileId interface.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_ID                ( ofa_dossier_id_get_type())
#define OFA_DOSSIER_ID( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_ID, ofaDossierId ))
#define OFA_DOSSIER_ID_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_ID, ofaDossierIdClass ))
#define OFA_IS_DOSSIER_ID( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_ID ))
#define OFA_IS_DOSSIER_ID_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_ID ))
#define OFA_DOSSIER_ID_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_ID, ofaDossierIdClass ))

typedef struct _ofaDossierIdPrivate        ofaDossierIdPrivate;

typedef struct {
	/*< public members >*/
	GObject              parent;

	/*< private members >*/
	ofaDossierIdPrivate *priv;
}
	ofaDossierId;

typedef struct {
	/*< public members >*/
	GObjectClass         parent;
}
	ofaDossierIdClass;

GType         ofa_dossier_id_get_type         ( void ) G_GNUC_CONST;

ofaDossierId *ofa_dossier_id_new              ( void );

void          ofa_dossier_id_set_dossier_name ( ofaDossierId *id,
															const gchar *name );

void          ofa_dossier_id_set_provider_name( ofaDossierId *id,
															const gchar *name );

G_END_DECLS

#endif /* __OFA_DOSSIER_ID_H__ */
