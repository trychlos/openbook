/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_EXTENSION_H__
#define __OPENBOOK_API_OFA_EXTENSION_H__

/**
 * SECTION: extension
 * @title: Extensions
 * @short_description: The application extension interface definition v 2
 * @include: openbook/ofa-extension.h
 *
 * &prodname; accepts extensions as dynamically loadable libraries
 * (aka plugins, aka modules, aka extenders).
 *
 * In order to be recognized as a valid &prodname; plugin, the library
 * must at least export the functions described as mandatory in this
 * extension API.
 *
 * From the application point of view, this extension API is managed
 * through the #ofaExtenderCollection and the #ofaExtenderModule classes,
 * where the #ofaExtenderCollection defines and manages a singleton,
 * which itself maintains the list of #ofaExtenderModule loaded modules.
 *
 * From the loadable module point of view, defining such a plugin
 * implies some rather simple steps:
 * - define (at least) the mandatory functions described here;
 * - if implementing the version 1 of this API, return on demand the
 *   list of primary #GType's defined by the module (where of course
 *   each primary #GType may itself support other types through the use
 *   of #GInterface's).
 *
 * That is.
 *
 * When the application later wants a particular GObject type, it is
 * enough to scan the #ofaExtenderCollection, which holds the list of
 * #ofaExtenderModule, which themselves hold the list of defined primary
 * GType's.
 *
 * See also #ofaExtenderModule documentation.
 *
 * It is suggested that loadable modules implement the #myIIdent
 * interface, in order to be able to provide some identification
 * informations to the application.
 *
 * See also http://www.lanedo.com/users/mitch/module-system-talk-guadec-2006/Module-System-Talk-Guadec-2006.pdf
 */

#include <gio/gio.h>
#include <glib-object.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

/**
 * ofa_extension_startup:
 * @module: the #GTypeModule of the plugin library being loaded.
 * @getter: [allow-none]: the main #ofaIGetter of the application.
 *
 * This function is called by the plugin manager when
 * the plugin library is first loaded in memory. The library may so take
 * advantage of this call by initializing itself, registering its
 * internal #GType types, etc.
 *
 * An Openbook extension MUST implement this function in order
 * to be considered as a valid candidate to dynamic load.
 *
 * Returns: %TRUE if the initialization is successful, %FALSE else.
 * In this later case, the library is unloaded and no more considered.
 *
 * Since: v 1.
 */
gboolean     ofa_extension_startup           ( GTypeModule *module, ofaIGetter *getter );

/**
 * ofa_extension_list_types:
 * @types: the address where to store the zero-terminated array of
 *  instantiable #GType types this library implements.
 *
 * Returned #GType types must already have been registered in the
 * #GType system (e.g. at #ofa_extension_startup() time), and the objects
 * they describe may implement one or more of the interfaces defined in
 * the application public API.
 *
 * The  plugin manager will instantiate one object for each returned
 * #GType type, and associate these objects to this library.
 *
 * An Openbook extension implementing the version 1 of this API MUST
 * implement this function in order to be considered as a valid candidate
 * to dynamic load.
 *
 * Returns: the number of #GType types returned in the @types array, not
 * counting the terminating zero item.
 *
 * Since: v 1.
 * Obsoleted: v 2.
 */
guint        ofa_extension_list_types        ( const GType **types );

/**
 * ofa_extension_enum_types:
 * @module: the #GTypeModule of the plugin library being loaded.
 * @cb: an application-provided #ofaExtensionEnumTypesCb callback.
 * @user_data: user data to be passed to the @cb.
 *
 * The plugin manager call this function after successful
 * #ofa_extension_startup().
 *
 * It is expected that the dynamic extension calls the @cb callback
 * once for each managed #GType object. Each one of these objects will
 * be instanciated once by the plugin manager.
 *
 * The #ofaIExtenderSetter interface is called on those of these objects
 * which implement this interface. This interface aims to provide some
 * initial pointers to plugin objects.
 *
 * An Openbook extension implementing the version 2 of this API MUST
 * implement this function in order to be considered as a valid candidate
 * to dynamic load.
 *
 * Since: v 2.
 */
typedef void ( *ofaExtensionEnumTypesCb )    ( GType, void * );

void         ofa_extension_enum_types        ( GTypeModule *module, ofaExtensionEnumTypesCb cb, void *user_data );

/**
 * ofa_extension_shutdown:
 * @module: the #GTypeModule of the plugin library being loaded.
 *
 * If defined, this function is called by the plugin manager when it is
 * about to shutdown itself.
 *
 * The dynamically loaded library may take advantage of this call to
 * release any resource, handle, and so on, it may have previously
 * allocated.
 *
 * Since: v 1.
 */
void         ofa_extension_shutdown          ( GTypeModule *module );

/**
 * ofa_extension_get_version_number:
 *
 * Returns: the version number of this API that the plugin implements.
 *
 * Defaults to 1.
 *
 * Since: v 2.
 */
guint        ofa_extension_get_version_number( void );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_EXTENSION_H__ */
