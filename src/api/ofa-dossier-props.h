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
 * dossier and of the Openbook software build and runtime at time it
 * is built.
 *
 * The #ofaDossierProps object can then be exported as a JSON string.
 */

#include <glib-object.h>

#include "api/ofa-hub-def.h"

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

const gchar     *ofa_dossier_props_get_title           ( void );

gchar           *ofa_dossier_props_get_json_string_ex  ( ofaHub *hub,
																const gchar *comment );

GType            ofa_dossier_props_get_type            ( void ) G_GNUC_CONST;

ofaDossierProps *ofa_dossier_props_new                 ( void );

ofaDossierProps *ofa_dossier_props_new_from_uri        ( const gchar *uri );

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

const gchar     *ofa_dossier_props_get_openbook_version( ofaDossierProps *props );

void             ofa_dossier_props_set_openbook_version( ofaDossierProps *props,
																const gchar *version );

void             ofa_dossier_props_set_plugin          ( ofaDossierProps *props,
																const gchar *canon_name,
																const gchar *display_name,
																const gchar *version );

void             ofa_dossier_props_set_dbmodel         ( ofaDossierProps *props,
																const gchar *id,
																const gchar *version );

const gchar     *ofa_dossier_props_get_comment         ( ofaDossierProps *props );

void             ofa_dossier_props_set_comment         ( ofaDossierProps *props,
																const gchar *comment );

const GTimeVal  *ofa_dossier_props_get_current_stamp   ( ofaDossierProps *props );

void             ofa_dossier_props_set_current_stamp   ( ofaDossierProps *props,
																const GTimeVal *stamp );

const gchar     *ofa_dossier_props_get_current_user    ( ofaDossierProps *props );

void             ofa_dossier_props_set_current_user    ( ofaDossierProps *props,
																const gchar *userid );

gchar           *ofa_dossier_props_get_json_string     ( ofaDossierProps *props );

const gchar     *ofa_dossier_props_get_name            ( ofaDossierProps *props );

void             ofa_dossier_props_set_name            ( ofaDossierProps *props,
																const gchar *name );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_DOSSIER_PROPS_H__ */
