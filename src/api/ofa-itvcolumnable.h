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

#ifndef __OPENBOOK_API_OFA_ITVCOLUMNABLE_H__
#define __OPENBOOK_API_OFA_ITVCOLUMNABLE_H__

/**
 * SECTION: itvcolumnable
 * @title: ofaITVColumnable
 * @short_description: The ITVColumnable Interface
 * @include: api/ofa-tvcolumnable.h
 *
 * The #ofaITVColumnable interface is used to dynamically display
 * treeview columns. It sends a 'ofa-toggled' signal when the visibility
 * status of a column changes.
 *
 * The #ofaITVColumnable is expected to be implemented by a #GtkTreeView.
 * Besides of just appending the column to the treeview, it also provides
 * following features:
 * - columns are dynamically displayable via the context menu of the view,
 * - order and size of the columns are saved in the user settings.
 *
 * When a column is added to the treeview, an action is created for
 * toggling its visibility state, and grouped together in a dedicated
 * action group.
 * The actions group namespace is <name>.itvcolumnable_<n>,
 * where:
 * - <name> is the identifier name of the instance as provided in
 *   #ofa_itvcolumnable_set_name(); it defaults to the instance class
 *   name.
 * - <n> is the column identifier, as provided in
 *   #ofa_itvcolumnable_add_column().
 *
 * This action is materialized by a menu item which is added to a popup
 * menu, which itself will be later added to the context menu of the
 * view.
 *
 * The class which implements the #ofaITVColumnable interface must
 * also implement the #ofaIActionable interface. This later interface
 * is used to defined actions which toggle the visibility state of each
 * column.
 *
 * Signals:
 * - 'ofa-toggled': the visibility of a column changes.
 * - 'ofa-twinwidth': the width of a twin group column has changed.
 *
 * In Openbook, most of the implementation is assured by the #ofaTVBin
 * base class, which also implements #ofaIFilterable and #ofaISortable
 * interfaces.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ITVCOLUMNABLE                      ( ofa_itvcolumnable_get_type())
#define OFA_ITVCOLUMNABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITVCOLUMNABLE, ofaITVColumnable ))
#define OFA_IS_ITVCOLUMNABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITVCOLUMNABLE ))
#define OFA_ITVCOLUMNABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITVCOLUMNABLE, ofaITVColumnableInterface ))

typedef struct _ofaITVColumnable                     ofaITVColumnable;

/**
 * ofaITVColumnableInterface:
 *
 * This defines the interface that an #ofaITVColumnable should implement.
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
}
	ofaITVColumnableInterface;

/*
 * Interface-wide
 */
GType              ofa_itvcolumnable_get_type                  ( void );

guint              ofa_itvcolumnable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint              ofa_itvcolumnable_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void               ofa_itvcolumnable_set_name                  ( ofaITVColumnable *instance,
																		const gchar *name );

void               ofa_itvcolumnable_set_getter                ( ofaITVColumnable *instance,
																		ofaIGetter *getter );

void               ofa_itvcolumnable_set_treeview              ( ofaITVColumnable *instance,
																		GtkTreeView *treeview );

void               ofa_itvcolumnable_add_column                ( ofaITVColumnable *instance,
																		GtkTreeViewColumn *column,
																		gint column_id,
																		const gchar *menu_label );

GtkTreeViewColumn *ofa_itvcolumnable_get_column                ( ofaITVColumnable *instance,
																		gint column_id );

gint               ofa_itvcolumnable_get_column_id             ( ofaITVColumnable *instance,
																		GtkTreeViewColumn *column );

gint               ofa_itvcolumnable_get_column_id_renderer    ( ofaITVColumnable *instance,
																		GtkCellRenderer *renderer );

guint              ofa_itvcolumnable_get_columns_count         ( ofaITVColumnable *instance );

GMenu             *ofa_itvcolumnable_get_menu                  ( ofaITVColumnable *instance );

void               ofa_itvcolumnable_set_default_column        ( ofaITVColumnable *instance,
																		gint column_id );

void               ofa_itvcolumnable_enable_column             ( ofaITVColumnable *instance,
																		gint column_id,
																		gboolean enable );

void               ofa_itvcolumnable_show_columns              ( ofaITVColumnable *instance );

void               ofa_itvcolumnable_show_columns_all          ( ofaITVColumnable *instance );

void               ofa_itvcolumnable_propagate_visible_columns ( ofaITVColumnable *instance,
																		GList *pages_list );

void               ofa_itvcolumnable_write_columns_settings    ( ofaITVColumnable *instance );

gboolean           ofa_itvcolumnable_twins_group_new           ( ofaITVColumnable *instance,
																		const gchar *name,
																		... );

gboolean           ofa_itvcolumnable_twins_group_add_widget    ( ofaITVColumnable *instance,
																		const gchar *name,
																		GtkWidget *widget );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ITVCOLUMNABLE_H__ */
