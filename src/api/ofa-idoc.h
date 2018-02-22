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

#ifndef __OPENBOOK_API_OFA_IDOC_H__
#define __OPENBOOK_API_OFA_IDOC_H__

/**
 * SECTION: idoc
 * @title: ofaIDoc
 * @short_description: The IDoc Interface
 * @include: openbook/ofa-idoc.h
 *
 * The #ofaIDoc interface provides documents management.
 *
 * Documents may be added and stored, queried or deleted. There is no
 * update facility.
 *
 * The #ofaIDoc interface is to be implemented by each ofoBase-derived
 * class which want to implement IDoc document management.
 */

#include <glib-object.h>

#include "api/ofa-box.h"
#include "api/ofo-doc-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDOC                      ( ofa_idoc_get_type())
#define OFA_IDOC( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDOC, ofaIDoc ))
#define OFA_IS_IDOC( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDOC ))
#define OFA_IDOC_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDOC, ofaIDocInterface ))

typedef struct _ofaIDoc                    ofaIDoc;

/**
 * ofaIDocEnumerateCb:
 *
 * The callback to be used when enumerating the documents.
 *
 * Returns: %TRUE to continue the enumeration, %FALSE to stop it now.
 */
typedef gboolean ( *ofaIDocEnumerateCb )( ofaIDoc *, ofoDoc *, void * );

/**
 * ofaIDocInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_count: [should] returns count of documents attached to the instance.
 * @foreach: [should] enumerates the documents.
 * @get_non_exist: [should] returns documents which are referenced but do not exist.
 *
 * This defines the interface that an #ofaIDoc should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/*** implementation-wide ***/
	/**
	 * get_interface_version:
	 *
	 * Returns: the version number of this interface which is managed
	 * by the implementation.
	 *
	 * Defaults to 1.
	 *
	 * Since: version 1.
	 */
	guint             ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_count:
	 * @instance: the #ofaIDoc provider.
	 *
	 * Returns: the count of documents attached to the instance.
	 *
	 * Since: version 1.
	 */
	ofxCounter        ( *get_count )            ( const ofaIDoc *instance );

	/**
	 * foreach:
	 * @instance: the #ofaIDoc provider.
	 * @cb: a #ofaIDocEnumerateCb callback.
	 * @user_data: user data to be provided to the callback.
	 *
	 * Enumerates the documents attached to the @instance.
	 *
	 * Since: version 1.
	 */
	void              ( *foreach )              ( const ofaIDoc *instance,
														ofaIDocEnumerateCb cb,
														void *user_data );

	/**
	 * get_orphans:
	 * @instance: the #ofaIDoc provider.
	 *
	 * Returns: the list of referenced documents which actually do not
	 * exist.
	 *
	 * Since: version 1.
	 */
	GList *           ( *get_orphans )          ( const ofaIDoc *instance );
}
	ofaIDocInterface;

/*
 * Interface-wide
 */
GType      ofa_idoc_get_type                  ( void );

guint      ofa_idoc_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint      ofa_idoc_get_interface_version     ( GType type );

GList     *ofa_idoc_get_class_orphans         ( GType type );

#define    ofa_idoc_free_class_orphans( L )   ( g_list_free( L ))

/*
 * Instance-wide
 */
ofxCounter ofa_idoc_get_count                 ( const ofaIDoc *instance );

void       ofa_idoc_foreach                   ( const ofaIDoc *instance,
														ofaIDocEnumerateCb cb,
														void *user_data );

GList     *ofa_idoc_get_orphans               ( const ofaIDoc *instance );

#define    ofa_idoc_free_orphans( L )         ( g_list_free( L ))

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDOC_H__ */
