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

#ifndef __OPENBOOK_API_OFA_EXTENSION_H__
#define __OPENBOOK_API_OFA_EXTENSION_H__

/**
 * SECTION: extension
 * @title: Plugins
 * @short_description: The application extension interface definition v 1
 * @include: openbook/ofa-extension.h
 *
 * &prodname; accepts extensions as dynamically loadable libraries
 * (aka plugins).
 *
 * In order to be recognized as a valid &prodname; plugin, the library
 * must at least export the functions described as mandatory in this
 * extension API.
 */

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * ofa_extension_startup:
 * @module: the #GTypeModule of the plugin library being loaded.
 *
 * This function is called by the plugin manager when
 * the plugin library is first loaded in memory. The library may so take
 * advantage of this call by initializing itself, registering its
 * internal #GType types, etc.
 *
 * An Openbook extension MUST implement this function in order
 * to be considered as a valid candidate to dynamic load.
 *
 * <example>
 *   <programlisting>
 *     static GType st_module_type = 0;
 *
 *     gboolean
 *     ofa_extension_startup( GTypeModule *plugin )
 *     {
 *         static GTypeInfo info = {
 *             sizeof( NadpDesktopProviderClass ),
 *             NULL,
 *             NULL,
 *             ( GClassInitFunc ) class_init,
 *             NULL,
 *             NULL,
 *             sizeof( NadpDesktopProvider ),
 *             0,
 *             ( GInstanceInitFunc ) instance_init
 *         };
 *
 *         static const GInterfaceInfo iio_provider_iface_info = {
 *             ( GInterfaceInitFunc ) iio_provider_iface_init,
 *             NULL,
 *             NULL
 *         };
 *
 *         st_module_type = g_type_module_register_type( plugin, G_TYPE_OBJECT, "NadpDesktopProvider", &amp;info, 0 );
 *
 *         g_type_module_add_interface( plugin, st_module_type, NA_TYPE_IIO_PROVIDER, &amp;iio_provider_iface_info );
 *
 *         return( TRUE );
 *     }
 *   </programlisting>
 * </example>
 *
 * Returns: %TRUE if the initialization is successful, %FALSE else.
 * In this later case, the library is unloaded and no more considered.
 */
gboolean     ofa_extension_startup           ( GTypeModule *module );

/**
 * ofa_extension_get_api_version:
 *
 * This function is called by the &prodname; program each time
 * it needs to know which version of this API the plugin
 * implements.
 *
 * If this function is not exported by the library,
 * the plugin manager considers that the library only implements the
 * version 1 of this extension API.
 *
 * Returns: the version of this API supported by the module.
 */
guint        ofa_extension_get_api_version   ( void );

/**
 * ofa_extension_get_name:
 *
 * Returns: the name of the extension, or %NULL.
 */
const gchar *ofa_extension_get_name          ( void );

/**
 * ofa_extension_get_version_number:
 *
 * Returns: the version number of the extension, or %NULL.
 */
const gchar *ofa_extension_get_version_number( void );

/**
 * ofa_extension_list_types:
 * @types: the address where to store the zero-terminated array of
 *  instantiable #GType types this library implements.
 *
 * Returned #GType types must already have been registered in the
 * #GType system (e.g. at #ofa_extension_startup() time), and the objects
 * they describe may implement one or more of the interfaces defined in
 * this Nautilus-Actions public API.
 *
 * The  plugin manager will instantiate one #GTypeInstance-
 * derived object for each returned #GType type, and associate these objects
 * to this library.
 *
 * An Openbook extension MUST implement this function in order
 * to be considered as a valid candidate to dynamic load.
 *
 * <example>
 *   <programlisting>
 *     &lcomment; the count of GType types provided by this extension
 *      * each new GType type must
 *      * - be registered in ofa_extension_startup()
 *      * - be addressed in ofa_extension_list_types().
 *      &rcomment;
 *     #define NADP_TYPES_COUNT    1
 *
 *     guint
 *     ofa_extension_list_types( const GType **types )
 *     {
 *          static GType types_list [1+NADP_TYPES_COUNT];
 *
 *          &lcomment; NADP_TYPE_DESKTOP_PROVIDER has been previously
 *           * registered in ofa_extension_startup function
 *           &rcomment;
 *          types_list[0] = NADP_TYPE_DESKTOP_PROVIDER;
 *
 *          types_list[NADP_TYPES_COUNT] = 0;
 *          *types = types_list;
 *
 *          return( NADP_TYPES_COUNT );
 *     }
 *   </programlisting>
 * </example>
 *
 * Returns: the number of #GType types returned in the @types array, not
 * counting the terminating zero item.
 */
guint        ofa_extension_list_types        ( const GType **types );

/**
 * ofa_extension_shutdown:
 *
 * If defined, this function is called by the plugin manager when it is
 * about to shutdown itself.
 *
 * The dynamically loaded library may take advantage of this call to
 * release any resource, handle, and so on, it may have previously
 * allocated.
 */
void         ofa_extension_shutdown          ( void );

/**
 * ofa_extension_preferences_run:
 *
 * If defined, this function should let the user configure his
 * preferences. Preferences may be written in the user configuration
 * file via the ofa_settings_xxx() API.
 */
void         ofa_extension_preferences_run   ( void );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_EXTENSION_H__ */
