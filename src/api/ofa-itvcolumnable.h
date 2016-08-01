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

#ifndef __OFA_ITVCOLUMNABLE_H__
#define __OFA_ITVCOLUMNABLE_H__

/**
 * SECTION: itvcolumnable
 * @title: ofaITVColumnable
 * @short_description: The ITVColumnable Interface
 * @include: ui/ofa-itreeview-column.h
 *
 * The #ofaITVColumnable interface is used to dynamically display
 * treeview columns. It sends a 'ofa-toggled' signal when the visibility
 * status of a column changes.
 */

#include <gtk/gtk.h>

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
	/**
	 * get_settings_key:
	 * @instance: this #ofaITVColumnable instance.
	 *
	 * Returns: the prefix of the settings key to be used.
	 *
	 * Defaults to class name.
	 *
	 * Since: version 1.
	 */
	const gchar * ( *get_settings_key )     ( const ofaITVColumnable *instance );
}
	ofaITVColumnableInterface;

/*
 * Interface-wide
 */
GType ofa_itvcolumnable_get_type                  ( void );

guint ofa_itvcolumnable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint ofa_itvcolumnable_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void  ofa_itvcolumnable_add_column                ( ofaITVColumnable *instance,
															GtkTreeViewColumn *column,
															const gchar *menu_label );

void  ofa_itvcolumnable_record_settings           ( ofaITVColumnable *instance );

void  ofa_itvcolumnable_set_context_menu          ( ofaITVColumnable *instance,
															GMenu *parent_menu,
															GActionGroup *action_group,
															const gchar *group_name );

void  ofa_itvcolumnable_set_default_column        ( ofaITVColumnable *instance,
															gint column_id );

void  ofa_itvcolumnable_show_columns              ( ofaITVColumnable *instance,
															GtkTreeView *treeview );

G_END_DECLS

#endif /* __OFA_ITVCOLUMNABLE_H__ */
