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

#ifndef __OFA_ITREEVIEW_DISPLAY_H__
#define __OFA_ITREEVIEW_DISPLAY_H__

/**
 * SECTION: itreeview_display
 * @title: ofaITreeviewDisplay
 * @short_description: The ITreeviewDisplay Interface
 * @include: ui/ofa-itreeview-display.h
 *
 * The #ofaITreeviewDisplay interface manages displayed column in a
 * #GtkTreeView.
 * The list of displayed columns is saved as a user preference.
 * The interface is able to attach a popup-menu to a provided parent,
 * letting the user select which columns he wants be displayed.
 *
 * The interface sends a 'ofa-toggled' message for each toggled column.
 */

G_BEGIN_DECLS

#define OFA_TYPE_ITREEVIEW_DISPLAY                      ( ofa_itreeview_display_get_type())
#define OFA_ITREEVIEW_DISPLAY( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITREEVIEW_DISPLAY, ofaITreeviewDisplay ))
#define OFA_IS_ITREEVIEW_DISPLAY( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITREEVIEW_DISPLAY ))
#define OFA_ITREEVIEW_DISPLAY_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITREEVIEW_DISPLAY, ofaITreeviewDisplayInterface ))

typedef struct _ofaITreeviewDisplay                     ofaITreeviewDisplay;

/**
 * ofaITreeviewDisplayInterface:
 *
 * This defines the interface that an #ofaITreeviewDisplay should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaITreeviewDisplay instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaITreeviewDisplay interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaITreeviewDisplay *instance );

	/**
	 * get_label:
	 * @instance: the #ofaITreeviewDisplay instance.
	 * @id: the column identifier.
	 *
	 * Returns: the localized label of the column which is to be
	 * displayed in the menu. May be %NULL if the column displayability
	 * cannot be toggled by the user.
	 *
	 * When non-null, the interface takes care of g_free-ing the
	 * returned string at instance finalization.
	 *
	 * This method is called at column definition time.
	 */
	gchar *  ( *get_label )            ( const ofaITreeviewDisplay *instance,
													guint column_id );

	/**
	 * get_def_visible:
	 * @instance: the #ofaITreeviewDisplay instance.
	 * @id: the column identifier.
	 *
	 * Returns: whether the @id column defaults to be visible.
	 *
	 * This method is called at column definition time.
	 * It is only called if above #get_label() method has returned a
	 * non-null string. Else, the column is assumed to be visible.
	 */
	gboolean ( *get_def_visible )      ( const ofaITreeviewDisplay *instance,
													guint column_id );
}
	ofaITreeviewDisplayInterface;

GType      ofa_itreeview_display_get_type                  ( void );

guint      ofa_itreeview_display_get_interface_last_version( void );

guint      ofa_itreeview_display_get_interface_version     ( const ofaITreeviewDisplay *instance );

void       ofa_itreeview_display_add_column                ( ofaITreeviewDisplay *instance,
																	GtkTreeViewColumn *column,
																	guint column_id );

void       ofa_itreeview_display_init_visible              ( const ofaITreeviewDisplay *instance,
																	const gchar *key );

gboolean   ofa_itreeview_display_get_visible               ( const ofaITreeviewDisplay *instance,
																	guint column_id );

void       ofa_itreeview_display_set_visible               ( const ofaITreeviewDisplay *instance,
																	guint column_id,
																	gboolean visible );

GtkWidget *ofa_itreeview_display_attach_menu_button        ( const ofaITreeviewDisplay *instance,
																	GtkContainer *parent );

G_END_DECLS

#endif /* __OFA_ITREEVIEW_DISPLAY_H__ */
