/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 * $Id$
 */

#ifndef __OFA_PLUGIN_H__
#define __OFA_PLUGIN_H__

/* @title: ofaPlugin
 * @short_description: The #ofaPlugin Class Definition
 * @include: ui/ofa-plugin.h
 *
 * The ofaPlugin class manages the application extensions as
 * dynamically loadable modules (thus 'plugins').
 *
 * ofaPlugin
 *  +- is derived from GTypeModule
 *      +- which itself implements GTypePlugin
 *
 * Each #ofaPlugin physically corresponds to a dynamically loadable
 * library (i.e. a plugin). An #ofaPlugin implements one or more
 * interfaces, thus providing one or more services.
 *
 * Interfaces (resp. services) are implemented (resp. provided) by
 * GObjects which are dynamically instantiated at plugin initial-load
 * time.
 *
 * So the dynamic is as follows:
 * - #ofaApplication scans for the PKGLIBDIR directory, trying to
 *   dynamically load all found libraries
 * - to be considered as an OFA plugin, a library must implement some
 *   functions (see api/ofa-extension.h)
 * - for each found plugin, #ofaApplication calls ofa_api_list_types()
 *   which returns the list of the types of the #GObject objects
 *   implemented in the plugin
 * - #ofaApplication then dynamically instantiates a GObject for each
 *   returned GType.
 *
 * After that, when someone wants to access an interface, it asks each
 * module for its list of objects which implement this given interface.
 * Interface API is then called against the returned GObject.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_PLUGIN                ( ofa_plugin_get_type())
#define OFA_PLUGIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_PLUGIN, ofaPlugin ))
#define OFA_PLUGIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_PLUGIN, ofaPluginClass ))
#define OFA_IS_PLUGIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_PLUGIN ))
#define OFA_IS_PLUGIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_PLUGIN ))
#define OFA_PLUGIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_PLUGIN, ofaPluginClass ))

typedef struct _ofaPluginPrivate       ofaPluginPrivate;

typedef struct {
	/*< private >*/
	GTypeModule       parent;
	ofaPluginPrivate *private;
}
	ofaPlugin;

typedef struct {
	/*< private >*/
	GTypeModuleClass parent;
}
	ofaPluginClass;

GType    ofa_plugin_get_type               ( void );

void     ofa_plugin_dump                   ( const ofaPlugin *plugin );

gint     ofa_plugin_load_modules           ( void );
void     ofa_plugin_release_modules        ( void );

GList   *ofa_plugin_get_extensions_for_type( GType type );
void     ofa_plugin_free_extensions_list   ( GList *extensions );

gboolean ofa_plugin_has_id                 ( ofaPlugin *module, const gchar *id );


G_END_DECLS

#endif /* __OFA_PLUGIN_H__ */
