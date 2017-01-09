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

#ifndef __OPENBOOK_API_OFA_ITVFILTERABLE_H__
#define __OPENBOOK_API_OFA_ITVFILTERABLE_H__

/**
 * SECTION: isortable
 * @title: ofaITVFilterable
 * @short_description: The ITVFilterable Interface
 * @include: openbook/ofa-itvfilterable.h
 *
 * The #ofaITVFilterable interface should be implemented by any GtkTreeView-
 * derived class to make it sortable.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_ITVFILTERABLE                      ( ofa_itvfilterable_get_type())
#define OFA_ITVFILTERABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITVFILTERABLE, ofaITVFilterable ))
#define OFA_IS_ITVFILTERABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITVFILTERABLE ))
#define OFA_ITVFILTERABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITVFILTERABLE, ofaITVFilterableInterface ))

typedef struct _ofaITVFilterable                    ofaITVFilterable;

/**
 * ofaITVFilterableInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @filter_model: [should] filter the model.
 *
 * This defines the interface that an #ofaITVFilterable may/should/must
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
	guint         ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * filter_model:
	 * @instance: this #ofaITVFilterable instance.
	 * @tmodel: the current #GtkTreeModel model.
	 * @iter: an item on the @tmodel.
	 *
	 * Returns: %TRUE if the item must be displayed.
	 *
	 * Since: version 1.
	 */
	gboolean      ( *filter_model )         ( const ofaITVFilterable *instance,
													GtkTreeModel *tmodel,
													GtkTreeIter *iter );
}
	ofaITVFilterableInterface;

/*
 * Interface-wide
 */
GType         ofa_itvfilterable_get_type                  ( void );

guint         ofa_itvfilterable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint         ofa_itvfilterable_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
GtkTreeModel *ofa_itvfilterable_set_child_model            ( ofaITVFilterable *instance,
																	GtkTreeModel *model );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITVFILTERABLE_H__ */
