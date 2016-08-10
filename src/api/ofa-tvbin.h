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

#ifndef __OPENBOOK_API_OFA_TVBIN_H__
#define __OPENBOOK_API_OFA_TVBIN_H__

/**
 * SECTION: ofa_tvbin
 * @title: ofaTVBin
 * @short_description: The ofaTVBin application class definition
 * @include: api/ofa-bin.h
 *
 * This is just a GtkFrame which contains a GtkScrolledWindow which
 * contains a GtkTreeView.
 *
 * This class implements the needed interfaces  so that all treeviews
 * in the application will have the same behavior:
 * - the treeview is sortable by column
 * - columns may be added/removed/resized by the user
 * - columns may be added by code.
 *
 * The class provides the following signals
 *    +------------------+-------------------------+--------------+--------------------+
 *    | Signal           | Event                   |   Selection  |     Sent when      |
 *    |                  |                         | may be empty | selection is empty |
 *    +------------------+-------------------------+--------------+--------------------+
 *    | ofa-selchanged   | on selection change     |      Yes     |        Yes         |
 *    | ofa-selactivated | on selection activation |       No     |         -          |
 *    | ofa-insert       | on Insert key           |       -      |         -          |
 *    | ofa-seldelete    | on Delete key.          |      Yes     |         No         |
 *    +------------------+-------------------------+--------------+--------------------+
 *
 * ofaITVFilterable interface.
 * The treeview-derived class is filtered if and only if it
 * implements the 'ofaTVBin::filter()' virtual method.
 *
 * ofaITVSortable interface.
 * The treeview-derived class is sortable by column if and only if it
 * implements the 'ofaTVBin::sort()' virtual method.
 *
 * ofaITVColumnable interface.
 * Columns may be dynamically made visible/invisible.
 *
 * Properties:
 * - ofa-tvbin-hpolicy: horizontal scrollbar policy;
 *                      will typically be NEVER for pages, AUTOMATIC (default) for dialogs.
 * - ofa-tvbin-shadow: shadow type of the surrounding frame;
 *                      will typically be IN for pages, NONE (default) for dialogs.
 * - ofa-tvbin-settings: the prefix of the settings key to be used by interfaces;
 *                       defaults to the class name.
 * - ofa-tvbin-groupname: the name of the action group;
 *                        must be set before defining any column;
 *                        defaults to 'tvbin'.
 * - ofa-tvbin-colsettings: whether this class should write its column settings;
 *                          will typically be %TRUE (default), unless we manage
 *                          several views of the same class (e.g. ofaAccountTreeview)
 *                          in which case the derived class must choose itself the
 *                          view to be used to write its settings.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_TVBIN                ( ofa_tvbin_get_type())
#define OFA_TVBIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_TVBIN, ofaTVBin ))
#define OFA_TVBIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_TVBIN, ofaTVBinClass ))
#define OFA_IS_TVBIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_TVBIN ))
#define OFA_IS_TVBIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_TVBIN ))
#define OFA_TVBIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_TVBIN, ofaTVBinClass ))

typedef struct _ofaTVBin ofaTVBin;

struct _ofaTVBin {
	/*< public members >*/
	GtkBin      parent;
};

/**
 * ofaTVBinClass:
 */
typedef struct {
	/*< public members >*/
	GtkBinClass parent;

	/*< protected virtual functions >*/
	/**
	 * filter:
	 * @bin: this #ofaTVBin instance.
	 * @model: the #GtkTreeModel model.
	 * @iter: the #GtkTreeIter.
	 *
	 * This virtual function is called when filtering the treeview.
	 *
	 * If this virtual is not implemented by the derived class,
	 * then the ITVFilterable interface will be disabled.
	 */
	gboolean ( *filter )( const ofaTVBin *bin,
								GtkTreeModel *model,
								GtkTreeIter *iter );

	/**
	 * sort:
	 * @bin: this #ofaTVBin instance.
	 * @model: the #GtkTreeModel model.
	 * @a: the first #GtkTreeIter.
	 * @b: the second #GtkTreeIter.
	 * column_id: the identifier of the sortable column.
	 *
	 * This virtual function is called when sorting the treeview.
	 *
	 * If this virtual is not implemented by the derived class,
	 * then the ITVSortable interface will not be implemented by the
	 * #ofaTVBin class, and the sort order of the treeview will only
	 * rely on the sort order of the underlying #ofaListStore (resp.
	 * #ofaTreeStore).
	 */
	gint     ( *sort )  ( const ofaTVBin *bin,
								GtkTreeModel *model,
								GtkTreeIter *a,
								GtkTreeIter *b,
								gint column_id );
}
	ofaTVBinClass;

GType             ofa_tvbin_get_type              ( void ) G_GNUC_CONST;

void              ofa_tvbin_add_column_amount     ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_date       ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_int        ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_pixbuf     ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_stamp      ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_text       ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_text_lx    ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_text_rx    ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

void              ofa_tvbin_add_column_text_x     ( ofaTVBin *bin,
														gint column_id,
														const gchar *header,
														const gchar *menu );

GMenu            *ofa_tvbin_get_menu              ( ofaTVBin *bin );

GtkTreeSelection *ofa_tvbin_get_selection         ( ofaTVBin *bin );

GtkWidget        *ofa_tvbin_get_treeview          ( ofaTVBin *bin );

void              ofa_tvbin_set_selection_mode    ( ofaTVBin *bin,
														GtkSelectionMode mode );

void              ofa_tvbin_set_settings_key      ( ofaTVBin *bin,
														const gchar *key );

void              ofa_tvbin_set_store             ( ofaTVBin *bin,
														GtkTreeModel *store );

void              ofa_tvbin_set_selected          ( ofaTVBin *bin,
														GtkTreeIter *treeview_iter );

void              ofa_tvbin_set_columns_settings  ( ofaTVBin *bin,
														gboolean write );

void              ofa_tvbin_write_columns_settings( ofaTVBin *bin );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_TVBIN_H__ */
