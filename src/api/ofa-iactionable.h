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

#ifndef __OPENBOOK_API_OFA_IACTIONABLE_H__
#define __OPENBOOK_API_OFA_IACTIONABLE_H__

/**
 * SECTION: iactionable
 * @title: ofaIActionable
 * @short_description: The IActionable Interface
 * @include: api/ofa-iactionable.h
 *
 * The #ofaIActionable interface is used to let a class manages its
 * actions via buttons and/or context menu items.
 *
 * The #ofaIActionable interface automatically defines a #GActionGroup
 * for each group of actions, as identified by their group names.
 *
 * Though the interface is able to simultaneously manage several action
 * groups, most of implementation will have only one.
 *
 * Each action group may be displayed as a #GMenu which contains items
 * defined with #ofa_iactionable_set_menu_item(). The #ofaIActionable
 * interface defines one menu for each action group.
 *
 * Each action may be activated via a #GtkButton as long as one has been
 * defined with #ofa_iactionable_set_button().
 *
 * #GAction actions and their respective handlers have to be handled by
 * the implementation class.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_IACTIONABLE                      ( ofa_iactionable_get_type())
#define OFA_IACTIONABLE( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IACTIONABLE, ofaIActionable ))
#define OFA_IS_IACTIONABLE( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IACTIONABLE ))
#define OFA_IACTIONABLE_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IACTIONABLE, ofaIActionableInterface ))

typedef struct _ofaIActionable                      ofaIActionable;

/**
 * ofaIActionableInterface:
 *
 * This defines the interface that an #ofaIActionable should implement.
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
	ofaIActionableInterface;

/**
 * An effort to homogeneïze the label of some common actions.
 */
#define OFA_IACTIONABLE_DELETE_BTN                  _( "_Delete..." )
#define OFA_IACTIONABLE_DELETE_ITEM                 _( "Delete this" )

#define OFA_IACTIONABLE_IMPORT_BTN                  _( "_Import..." )
#define OFA_IACTIONABLE_IMPORT_ITEM                 _( "Import" )

#define OFA_IACTIONABLE_NEW_BTN                     _( "_New..." )
#define OFA_IACTIONABLE_NEW_ITEM                    _( "New" )

#define OFA_IACTIONABLE_PROPERTIES_BTN              _( "_Properties..." )
#define OFA_IACTIONABLE_PROPERTIES_ITEM_DISPLAY     _( "Display properties" )
#define OFA_IACTIONABLE_PROPERTIES_ITEM_EDIT        _( "Edit properties" )

#define OFA_IACTIONABLE_VISIBLE_COLUMNS_ITEM        _( "Visible columns" )

/**
 * The prototype of the enum_group callback function
 */
typedef void ( *ofaIActionableEnumCb )( ofaIActionable *, const gchar *, GActionGroup *, void * );

/*
 * Interface-wide
 */
GType         ofa_iactionable_get_type                  ( void );

guint         ofa_iactionable_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint         ofa_iactionable_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void          ofa_iactionable_enum_action_groups        ( ofaIActionable *instance,
																ofaIActionableEnumCb cb,
																void *user_data );

GActionGroup *ofa_iactionable_get_action_group          ( ofaIActionable *instance,
																const gchar *group_name );

GMenu        *ofa_iactionable_get_menu                  ( ofaIActionable *instance,
																const gchar *group_name );

void          ofa_iactionable_set_action                ( ofaIActionable *instance,
																const gchar *group_name,
																GAction *action );

GtkWidget    *ofa_iactionable_set_button                ( ofaIActionable *instance,
																const gchar *group_name,
																GAction *action,
																const gchar *button_label );

GMenuItem    *ofa_iactionable_set_menu_item             ( ofaIActionable *instance,
																const gchar *group_name,
																GAction *action,
																const gchar *item_label );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IACTIONABLE_H__ */
