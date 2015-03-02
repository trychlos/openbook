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

#ifndef __OFA_PLUGIN_H__
#define __OFA_PLUGIN_H__

/* @title: ofaPlugin
 * @short_description: The #ofaPlugin Class Definition
 * @include: core/ofa-plugin.h
 *
 * The ofaPlugin class manages the application extensions as
 * dynamically loadable modules (thus 'plugins').
 *
 * ofaPlugin
 *  +- is derived from GTypeModule
 *      +- which itself implements GTypePlugin
 *
 * Model view:
 * [plugin]<-1,1->[dynamic libray]<-1,N->[internal_types]
 *         <-1,N->[objects_of_internal_type]
 *
 * So the dynamic is as follows:
 *
 * 1. the application (ofa_plugin_load_plugins()) scans the PKGLIBDIR
 *    directory, trying to dynamically load all found libraries ;
 *    in order to to be considered as a valid OFA plugin, the library
 *    must implement some mandatory functions (see api/ofa-extension.h)
 *
 * 2. the library is asked for its internal types, each of these types
 *    being supposed to implement one or more of the application
 *    interfaces
 *
 * 3. for each internal type, a new object is instanciated and reffed
 *    by the application ; this new object will so become the
 *    'go-between' between the application and the library, because it
 *    is knowned to implement some given interfaces.
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
	/*< public members >*/
	GTypeModule       parent;

	/*< private members >*/
	ofaPluginPrivate *priv;
}
	ofaPlugin;

typedef struct {
	/*< public members >*/
	GTypeModuleClass parent;
}
	ofaPluginClass;

GType        ofa_plugin_get_type               ( void );

void         ofa_plugin_dump                   ( const ofaPlugin *plugin );

/* start/end of the application */
gint         ofa_plugin_load_modules           ( void );
void         ofa_plugin_release_modules        ( void );

/* each time we are searching for a given interface */
GList       *ofa_plugin_get_extensions_for_type( GType type );
void         ofa_plugin_free_extensions        ( GList *extensions );

/* returning all instanciated plugins */
const GList *ofa_plugin_get_modules            ( void );

/* requesting a plugin */
gboolean     ofa_plugin_implements_type        ( const ofaPlugin *plugin, GType type );
gboolean     ofa_plugin_has_object             ( const ofaPlugin *plugin, GObject *instance );

const gchar *ofa_plugin_get_name               ( ofaPlugin *plugin );

const gchar *ofa_plugin_get_version_number     ( ofaPlugin *plugin );

G_END_DECLS

#endif /* __OFA_PLUGIN_H__ */
