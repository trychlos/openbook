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

#ifndef __OPENBOOK_API_OFA_EXTENSION_H__
#define __OPENBOOK_API_OFA_EXTENSION_H__

/**
 * SECTION: extension
 * @title: Extensions
 * @short_description: The application extension interface definition v 1
 * @include: openbook/ofa-extension.h
 *
 * &prodname; accepts extensions as dynamically loadable libraries
 * (aka plugins, aka extenders).
 *
 * In order to be recognized as a valid &prodname; plugin, the library
 * must at least export the functions described as mandatory in this
 * extension API.
 *
 * This header describes the extension in the C sens rather than in the
 * GObject one, that is there is no GType involved.
 *
 * From the application point of view, this extension API is managed
 * through the #ofaExtenderCollection and the #ofaExtenderModule classes,
 * where the #ofaExtenderCollection defines and manages a singleton,
 * which itself maintains the list of #ofaExtenderModule loaded modules.
 *
 * From the loadable module point of view, defining such a plugin
 * implies some rather simple steps:
 * - define (at least) the mandatory functions described here;
 * - returns on demand the list of primary GType defined by the module;
 *   each module may define several primary GType;
 *   note that a primary GType must be declared via
 *   #g_type_module_register_type();
 *   each primary GType may itself support other types through the use
 *   of GInterface;
 * - the application associates then each module with a newly allocated
 *   #ofaExtenderModule object
 * - each #ofaExtenderModule allocates a new GObject for each primary
 *   GType advertized by the module.
 *
 * That is.
 *
 * When the application later wants a particular GObject type, it is
 * enough to scan the #ofaExtenderCollection, which holds the list of
 * #ofaExtenderModule, which themselves hold the list of defined primary
 * GType, and asking this GObject if it knows such or such GType.
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
 * <example>
 *   <programlisting>
 *     static GType st_module_type = 0;
 *
 *     gboolean
 *     ofa_extension_startup( GTypeModule *plugin, GApplication *application )
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

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_EXTENSION_H__ */
