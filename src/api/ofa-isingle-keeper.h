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

#ifndef __OPENBOOK_API_OFA_ISINGLE_KEEPER_H__
#define __OPENBOOK_API_OFA_ISINGLE_KEEPER_H__

/**
 * SECTION: isingle_keeper
 * @title: ofaISingleKeeper
 * @short_description: The ISingleKeeper Interface
 * @include: openbook/ofa-isingle-keeper.h
 *
 * The #ofaISingleKeeper interface lets an implementor object associate
 * a GType with a single, global, GObject.
 *
 * Rather see #myICollector interface to associate a GType with a
 * GList of objects.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_ISINGLE_KEEPER                      ( ofa_isingle_keeper_get_type())
#define OFA_ISINGLE_KEEPER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISINGLE_KEEPER, ofaISingleKeeper ))
#define OFA_IS_ISINGLE_KEEPER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISINGLE_KEEPER ))
#define OFA_ISINGLE_KEEPER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISINGLE_KEEPER, ofaISingleKeeperInterface ))

typedef struct _ofaISingleKeeper                     ofaISingleKeeper;

/**
 * ofaISingleKeeperInterface:
 * @get_interface_version: [should]: get the version number of the
 *                                   interface implementation.
 *
 * This defines the interface that an #ofaISingleKeeper may/should
 * implement.
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
	guint ( *get_interface_version )( void );

	/*** instance-wide ***/
}
	ofaISingleKeeperInterface;

/*
 * Interface-wide
 */
GType    ofa_isingle_keeper_get_type                  ( void );

guint    ofa_isingle_keeper_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint    ofa_isingle_keeper_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GObject *ofa_isingle_keeper_get_object                ( const ofaISingleKeeper *instance,
															GType type );

void     ofa_isingle_keeper_set_object                ( ofaISingleKeeper *instance,
															void *object );

void     ofa_isingle_keeper_free_all                  ( ofaISingleKeeper *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISINGLE_KEEPER_H__ */
