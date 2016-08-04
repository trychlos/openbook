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

#ifndef __OPENBOOK_API_OFA_ICONTEXT_H__
#define __OPENBOOK_API_OFA_ICONTEXT_H__

/**
 * SECTION: icontext
 * @title: ofaIContext
 * @short_description: The IContext Interface
 * @include: api/ofa-icontext.h
 *
 * The #ofaIContext interface is used to let a widget display a
 * contextual popup menu.
 */

#include "api/ofa-iactionable.h"

G_BEGIN_DECLS

#define OFA_TYPE_ICONTEXT                      ( ofa_icontext_get_type())
#define OFA_ICONTEXT( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ICONTEXT, ofaIContext ))
#define OFA_IS_ICONTEXT( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ICONTEXT ))
#define OFA_ICONTEXT_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ICONTEXT, ofaIContextInterface ))

typedef struct _ofaIContext                      ofaIContext;

/**
 * ofaIContextInterface:
 *
 * This defines the interface that an #ofaIContext should implement.
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
	 * get_focused_widget:
	 *
	 * Returns: the widget which will get the focus (and so which will
	 * receive 'button-press-event' signals.
	 *
	 * Defaults to %NULL.
	 *
	 * Since: version 1.
	 */
	GtkWidget *   ( *get_focused_widget )   ( ofaIContext *instance );
}
	ofaIContextInterface;

/*
 * Interface-wide
 */
GType      ofa_icontext_get_type                  ( void );

guint      ofa_icontext_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint      ofa_icontext_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
void       ofa_icontext_append_submenu            ( ofaIContext *instance,
															ofaIActionable *actionable,
															const gchar *label,
															GMenu *menu );

void       ofa_icontext_set_menu                  ( ofaIContext *instance,
															ofaIActionable *actionable,
															GMenu *menu );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_ICONTEXT_H__ */
