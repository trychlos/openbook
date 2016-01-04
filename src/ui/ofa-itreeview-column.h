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

#ifndef __OFA_ITREEVIEW_COLUMN_H__
#define __OFA_ITREEVIEW_COLUMN_H__

/**
 * SECTION: itreeview_column
 * @title: ofaITreeviewColumn
 * @short_description: The ITreeviewColumn Interface
 * @include: ui/ofa-itreeview-column.h
 *
 * The #ofaITreeviewColumn interface is used to homogeneize and
 * mutualize the build and the appearance of GtkTreeView's. This is
 * mostly used - but not exclusive to - for #ofoEntry -based views.
 *
 * The interface works by associating an identifier provided by the
 * implementation to an internal identifier of the data to be
 * displayed.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_ITREEVIEW_COLUMN                      ( ofa_itreeview_column_get_type())
#define OFA_ITREEVIEW_COLUMN( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ITREEVIEW_COLUMN, ofaITreeviewColumn ))
#define OFA_IS_ITREEVIEW_COLUMN( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ITREEVIEW_COLUMN ))
#define OFA_ITREEVIEW_COLUMN_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ITREEVIEW_COLUMN, ofaITreeviewColumnInterface ))

typedef struct _ofaITreeviewColumn                     ofaITreeviewColumn;

/**
 * ofaITreeviewColumnInterface:
 *
 * This defines the interface that an #ofaITreeviewColumn should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaITreeviewColumn instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaITreeviewColumn interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaITreeviewColumn *instance );
}
	ofaITreeviewColumnInterface;

/*
 * The columns managed here, in alpha order
 */
enum {
	ITVC_ACC_ID = 0,
	ITVC_CONCIL_DATE,
	ITVC_CONCIL_ID,
	ITVC_CREDIT,
	ITVC_CUR_ID,
	ITVC_DEBIT,
	ITVC_DEFFECT,
	ITVC_DOPE,
	ITVC_ENT_ID,
	ITVC_ENT_LABEL,
	ITVC_ENT_REF,
	ITVC_ENT_STATUS,
	ITVC_LED_ID,
	ITVC_STLMT_NUMBER,
};


/**
 * ofsTreeviewColumnId:
 *
 * As a helper, this let the implementation associate its internal
 * column id (in the store) with the column identifier managed by this
 * ITreeview interface.
 *
 * Each time a method of the interface takes a column identifier as an
 * argument, it also takes a pointer to an array of #ofsTreeviewColumn
 * structures.
 * When non null, then the array is expected to associate store
 * identifiers managed by the implementation to interface identifiers,
 * and the identifier provided as an argument is expected to be a
 * store identifier.
 * When null, then the argument identifier is expected to be an
 * interface identifier.
 */
typedef struct {
	gint id_store;
	gint id_interface;
}
	ofsTreeviewColumnId;

GType    ofa_itreeview_column_get_type       ( void );

guint    ofa_itreeview_column_get_interface_last_version
                                             ( const ofaITreeviewColumn *instance );

gchar   *ofa_itreeview_column_get_menu_label ( const ofaITreeviewColumn *instance,
															guint column_id,
															const ofsTreeviewColumnId *sid );

gboolean ofa_itreeview_column_get_def_visible( const ofaITreeviewColumn *instance,
															guint column_id,
															const ofsTreeviewColumnId *sid );

G_END_DECLS

#endif /* __OFA_ITREEVIEW_COLUMN_H__ */
