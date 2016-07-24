/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_ISORTABLE_H__
#define __OPENBOOK_API_OFA_ISORTABLE_H__

/**
 * SECTION: isortable
 * @title: ofaISortable
 * @short_description: The ISortable Interface
 * @include: openbook/ofa-isortable.h
 *
 * The #ofaISortable interface should be implemented by any GtkTreeView-
 * derived class to make it sortable.
 */

#include "api/ofa-istore.h"

G_BEGIN_DECLS

#define OFA_TYPE_ISORTABLE                      ( ofa_isortable_get_type())
#define OFA_ISORTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ISORTABLE, ofaISortable ))
#define OFA_IS_ISORTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ISORTABLE ))
#define OFA_ISORTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ISORTABLE, ofaISortableInterface ))

typedef struct _ofaISortable                    ofaISortable;

/**
 * ofaISortableInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaISortable may/should/must
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
	/**
	 * sort_model:
	 * @instance: this #ofaISortable instance.
	 * @tmodel: the current #GtkTreeModel model.
	 * @a: an item on the @tmodel.
	 * @b: another item on the @tmodel.
	 * @column_id: the identifier of the sorted column.
	 *
	 * Returns: -1 if @a < @b, 1 if @a > @b, 0 if they are equal.
	 *
	 * Since: version 1.
	 */
	gint  ( *sort_model )           ( const ofaISortable *instance,
											GtkTreeModel *tmodel,
											GtkTreeIter *a,
											GtkTreeIter *b,
											gint column_id );
}
	ofaISortableInterface;

/*
 * Interface-wide
 */
GType ofa_isortable_get_type                  ( void );

guint ofa_isortable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint ofa_isortable_get_interface_version     ( GType type );

gint  ofa_isortable_sort_png                  ( const GdkPixbuf *a, const GdkPixbuf *b );

gint  ofa_isortable_sort_str_amount           ( const gchar *a, const gchar *b );

gint  ofa_isortable_sort_str_int              ( const gchar *a, const gchar *b );

/*
 * Instance-wide
 */
void  ofa_isortable_add_sortable_column       ( ofaISortable *instance,
														gint column_id );

void  ofa_isortable_set_default_sort          ( ofaISortable *instance,
														gint column_id,
														GtkSortType order );

void  ofa_isortable_set_settings_key          ( ofaISortable *instance,
														const gchar *key );

void  ofa_isortable_set_store                 ( ofaISortable *instance,
														ofaIStore *store );

void  ofa_isortable_set_treeview              ( ofaISortable *instance,
														GtkTreeView *tview );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ISORTABLE_H__ */
