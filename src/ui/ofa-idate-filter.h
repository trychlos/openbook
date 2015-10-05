/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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

#ifndef __OFA_IDATE_FILTER_H__
#define __OFA_IDATE_FILTER_H__

/**
 * SECTION: idate_filter
 * @title: ofaIDateFilter
 * @short_description: The IDateFilter Interface
 * @include: ui/ofa-idate-filter.h
 *
 * The #ofaIDateFilter interface is implemented by #ofaDateFilterBin
 * class. It implements all needed methods to manage the composite
 * widget.
 */

G_BEGIN_DECLS

#define OFA_TYPE_IDATE_FILTER                      ( ofa_idate_filter_get_type())
#define OFA_IDATE_FILTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDATE_FILTER, ofaIDateFilter ))
#define OFA_IS_IDATE_FILTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDATE_FILTER ))
#define OFA_IDATE_FILTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDATE_FILTER, ofaIDateFilterInterface ))

typedef struct _ofaIDateFilter                     ofaIDateFilter;

/**
 * ofaIDateFilterInterface:
 *
 * This defines the interface that an #ofaIDateFilter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDateFilter instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIDateFilter interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIDateFilter *instance );

	/**
	 * add_widget:
	 * @instance: the #ofaIDateFilter instance.
	 * @widget: the widget to be added.
	 * @where: where the widget is to be added.
	 *
	 * Let the application customize the composite widget by adding its
	 * own.
	 */
	void  ( *add_widget )           ( ofaIDateFilter *instance,
												GtkWidget *widget,
												gint where );
}
	ofaIDateFilterInterface;

/**
 * Whether we are addressing the "From:" date or the "To:" one
 */
enum {
	IDATE_FILTER_FROM = 1,
	IDATE_FILTER_TO
};

/**
 * Where the added widget is to be inserted
 */
enum {
	IDATE_FILTER_BEFORE = 1,
	IDATE_FILTER_BETWEEN,
	IDATE_FILTER_AFTER
};

GType        ofa_idate_filter_get_type             ( void );

guint        ofa_idate_filter_get_interface_last_version
                                                    ( const ofaIDateFilter *filter );

void         ofa_idate_filter_setup_bin            ( ofaIDateFilter *filter,
															const gchar *xml_name );

void         ofa_idate_filter_add_widget           ( ofaIDateFilter *filter,
															GtkWidget *widget,
															gint where );

void         ofa_idate_filter_set_prefs            ( ofaIDateFilter *filter,
															const gchar *prefs_key );

const GDate *ofa_idate_filter_get_date             ( ofaIDateFilter *filter,
															gint who );

void         ofa_idate_filter_set_date             ( ofaIDateFilter *filter,
															gint who,
															const GDate *date );

gboolean     ofa_idate_filter_is_valid             ( ofaIDateFilter *filter,
															gint who,
															gchar **message );

GtkWidget   *ofa_idate_filter_get_entry            ( ofaIDateFilter *filter,
															gint who );

GtkWidget   *ofa_idate_filter_get_frame_label      ( ofaIDateFilter *filter );

GtkWidget   *ofa_idate_filter_get_prompt           ( ofaIDateFilter *filter,
															gint who );

G_END_DECLS

#endif /* __OFA_IDATE_FILTER_H__ */
