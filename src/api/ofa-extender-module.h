/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_EXTENDER_MODULE_H__
#define __OPENBOOK_API_OFA_EXTENDER_MODULE_H__

/* @title: ofaExtenderModule
 * @short_description: the ofaExtenderModule class definition
 * @include: my/my-extender-module.h
 *
 * The #ofaExtenderModule class represents the loaded module.
 */

#include <gio/gio.h>
#include <glib-object.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_EXTENDER_MODULE                ( ofa_extender_module_get_type())
#define OFA_EXTENDER_MODULE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_EXTENDER_MODULE, ofaExtenderModule ))
#define OFA_EXTENDER_MODULE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_EXTENDER_MODULE, ofaExtenderModuleClass ))
#define OFA_IS_EXTENDER_MODULE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_EXTENDER_MODULE ))
#define OFA_IS_EXTENDER_MODULE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_EXTENDER_MODULE ))
#define OFA_EXTENDER_MODULE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_EXTENDER_MODULE, ofaExtenderModuleClass ))

typedef struct {
	/*< public members >*/
	GTypeModule      parent;
}
	ofaExtenderModule;

typedef struct {
	/*< public members >*/
	GTypeModuleClass parent;
}
	ofaExtenderModuleClass;

GType              ofa_extender_module_get_type        ( void ) G_GNUC_CONST;

ofaExtenderModule *ofa_extender_module_new             ( ofaIGetter *getter,
																const gchar *filename );

void               ofa_extender_module_free            ( ofaExtenderModule *module,
																void *user_data );

const GList       *ofa_extender_module_get_objects     ( const ofaExtenderModule *module );

GList             *ofa_extender_module_get_for_type    ( ofaExtenderModule *module,
															GType type );

gchar             *ofa_extender_module_get_canon_name  ( const ofaExtenderModule *module );

gchar             *ofa_extender_module_get_display_name( const ofaExtenderModule *module );

gchar             *ofa_extender_module_get_version     ( const ofaExtenderModule *module );

gboolean           ofa_extender_module_has_object      ( const ofaExtenderModule *module,
																GObject *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_EXTENDER_MODULE_H__ */
