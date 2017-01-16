/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_DOSSIER_PROPS_H__
#define __OPENBOOK_API_OFA_DOSSIER_PROPS_H__

/**
 * SECTION: ofahub
 * @title: ofaDossierProps
 * @short_description: The #ofaDossierProps Class Definition
 * @include: openbook/ofa-dossier-props.h
 *
 * The #ofaDossierProps class manages the current properties of the
 * dossier.
 *
 * The #ofaDossierProps class implements the ofaIJson interface, and
 * can thus be exported as a JSON string.
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"
#include "api/ofo-dossier.h"

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_PROPS                ( ofa_dossier_props_get_type())
#define OFA_DOSSIER_PROPS( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_PROPS, ofaDossierProps ))
#define OFA_DOSSIER_PROPS_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_PROPS, ofaDossierPropsClass ))
#define OFA_IS_DOSSIER_PROPS( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_PROPS ))
#define OFA_IS_DOSSIER_PROPS_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_PROPS ))
#define OFA_DOSSIER_PROPS_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_PROPS, ofaDossierPropsClass ))

typedef struct _ofaDossierProps               ofaDossierProps;

struct _ofaDossierProps {
	/*< public members >*/
	GObject      parent;
};

typedef struct {
	/*< public members >*/
	GObjectClass parent;
}
	ofaDossierPropsClass;

GType            ofa_dossier_props_get_type            ( void ) G_GNUC_CONST;

ofaDossierProps *ofa_dossier_props_new                 ( void );

ofaDossierProps *ofa_dossier_props_new_from_dossier    ( ofoDossier *dossier );

ofaDossierProps *ofa_dossier_props_new_from_archive    ( const gchar *uri );

ofaDossierProps *ofa_dossier_props_new_from_string     ( const gchar *string );

gboolean         ofa_dossier_props_get_is_current      ( ofaDossierProps *props );

void             ofa_dossier_props_set_is_current      ( ofaDossierProps *props,
																gboolean is_current );

const GDate     *ofa_dossier_props_get_begin_date      ( ofaDossierProps *props );

void             ofa_dossier_props_set_begin_date      ( ofaDossierProps *props,
																const GDate *date );

const GDate     *ofa_dossier_props_get_end_date        ( ofaDossierProps *props );

void             ofa_dossier_props_set_end_date        ( ofaDossierProps *props,
																const GDate *date );

const gchar     *ofa_dossier_props_get_rpid            ( ofaDossierProps *props );

void             ofa_dossier_props_set_rpid            ( ofaDossierProps *props,
																const gchar *rpid );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_DOSSIER_PROPS_H__ */
