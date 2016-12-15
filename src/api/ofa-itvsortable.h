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

#ifndef __OPENBOOK_API_OFA_ITVSORTABLE_H__
#define __OPENBOOK_API_OFA_ITVSORTABLE_H__

/**
 * SECTION: isortable
 * @title: ofaITVSortable
 * @short_description: The ITVSortable Interface
 * @include: openbook/ofa-itvsortable.h
 *
 * The #ofaITVSortable interface should be implemented by any GtkTreeView-
 * derived class to make it sortable.
 */

#include <gtk/gtk.h>

#include "api/ofa-hub-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ITVSORTABLE                      ( ofa_itvsortable_get_type())
#define OFA_ITVSORTABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITVSORTABLE, ofaITVSortable ))
#define OFA_IS_ITVSORTABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITVSORTABLE ))
#define OFA_ITVSORTABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITVSORTABLE, ofaITVSortableInterface ))

typedef struct _ofaITVSortable                    ofaITVSortable;

/**
 * ofaITVSortableInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 * @get_column_id: [must] returns the identifier of a column.
 * @has_sort_model: [should] returns whether the model is sortable.
 * @sort_model: [should] sort the model.
 *
 * This defines the interface that an #ofaITVSortable may/should/must
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
	 * get_column_id:
	 * @instance: this #ofaITVSortable instance.
	 * @column: a #GtkTreeViewColumn column.
	 *
	 * Returns: the identifier of the @column.
	 *
	 * Defaults to -1.
	 *
	 * Since: version 1.
	 */
	gint          ( *get_column_id )        ( ofaITVSortable *instance,
													GtkTreeViewColumn *column );

	/**
	 * has_sort_model:
	 * @instance: this #ofaITVSortable instance.
	 *
	 * Returns: %TRUE if the implementation (or one of its derived
	 * class) is able to provide a sort() method.
	 *
	 * Default to %FALSE (model is not sortable).
	 *
	 * This method is needed because the #ofaTVBin which implements this
	 * interface always provides a #sort_model() method in order to
	 * transform the interface method into a virtual method for its
	 * derived classes.
	 * So only the #ofaTVBin class knows if a derived class provides
	 * such a virtual method.
	 *
	 * Since: version 1.
	 */
	gboolean      ( *has_sort_model )       ( const ofaITVSortable *instance );

	/**
	 * sort_model:
	 * @instance: this #ofaITVSortable instance.
	 * @tmodel: the current #GtkTreeModel model.
	 * @a: an item on the @tmodel.
	 * @b: another item on the @tmodel.
	 * @column_id: the identifier of the sorted column.
	 *
	 * Returns: -1 if @a < @b, 1 if @a > @b, 0 if they are equal.
	 *
	 * Since: version 1.
	 */
	gint          ( *sort_model )           ( const ofaITVSortable *instance,
													GtkTreeModel *tmodel,
													GtkTreeIter *a,
													GtkTreeIter *b,
													gint column_id );
}
	ofaITVSortableInterface;

/*
 * Interface-wide
 */
GType         ofa_itvsortable_get_type                  ( void );

guint         ofa_itvsortable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint         ofa_itvsortable_get_interface_version     ( GType type );

gint          ofa_itvsortable_sort_png                  ( const GdkPixbuf *a, const GdkPixbuf *b );

gint          ofa_itvsortable_sort_str_amount           ( ofaITVSortable *instance,
																const gchar *a, const gchar *b );

gint          ofa_itvsortable_sort_str_int              ( const gchar *a, const gchar *b );

/*
 * Instance-wide
 */
void          ofa_itvsortable_set_name                  ( ofaITVSortable *instance,
																const gchar *name );

void          ofa_itvsortable_set_hub                   ( ofaITVSortable *instance,
																ofaHub *hub );

void          ofa_itvsortable_set_treeview              ( ofaITVSortable *instance,
																GtkTreeView *treeview );

void          ofa_itvsortable_set_default_sort          ( ofaITVSortable *instance,
																gint column_id,
																GtkSortType order );

GtkTreeModel *ofa_itvsortable_set_child_model           ( ofaITVSortable *instance,
																GtkTreeModel *model );

gboolean      ofa_itvsortable_get_is_sortable           ( ofaITVSortable *instance );

void          ofa_itvsortable_show_sort_indicator       ( ofaITVSortable *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITVSORTABLE_H__ */
