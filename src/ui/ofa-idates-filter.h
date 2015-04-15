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

#ifndef __OFA_IDATES_FILTER_H__
#define __OFA_IDATES_FILTER_H__

/**
 * SECTION: idates_filter
 * @title: ofaIDatesFilter
 * @short_description: The IDatesFilter Interface
 * @include: ui/ofa-idates-filter.h
 *
 * The #ofaIDatesFilter interface is implemented by #ofaDatesFilterBin
 * class. It implements all needed methods to manage the composite
 * widget.
 */

G_BEGIN_DECLS

#define OFA_TYPE_IDATES_FILTER                      ( ofa_idates_filter_get_type())
#define OFA_IDATES_FILTER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDATES_FILTER, ofaIDatesFilter ))
#define OFA_IS_IDATES_FILTER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDATES_FILTER ))
#define OFA_IDATES_FILTER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDATES_FILTER, ofaIDatesFilterInterface ))

typedef struct _ofaIDatesFilter                     ofaIDatesFilter;

/**
 * ofaIDatesFilterInterface:
 *
 * This defines the interface that an #ofaIDatesFilter should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDatesFilter instance.
	 *
	 * The interface code calls this method each time it needs to know
	 * which version of this interface the application implements.
	 *
	 * If this method is not implemented by the application, then the
	 * interface code considers that the application only implements
	 * the version 1 of the ofaIDatesFilter interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the application is supporting.
	 *
	 * Defaults to 1.
	 */
	guint ( *get_interface_version )( const ofaIDatesFilter *instance );

	/**
	 * add_widget:
	 * @instance: the #ofaIDatesFilter instance.
	 * @widget: the widget to be added.
	 * @where: where the widget is to be added.
	 *
	 * Let the application customize the composite widget by adding its
	 * own.
	 */
	void  ( *add_widget )           ( ofaIDatesFilter *instance,
												GtkWidget *widget,
												gint where );
}
	ofaIDatesFilterInterface;

/**
 * Whether we are addressing the "From:" date or the "To:" one
 */
enum {
	IDATES_FILTER_FROM = 1,
	IDATES_FILTER_TO
};

/**
 * Where the added widget is to be inserted
 */
enum {
	IDATES_FILTER_BEFORE = 1,
	IDATES_FILTER_BETWEEN,
	IDATES_FILTER_AFTER
};

GType        ofa_idates_filter_get_type             ( void );

guint        ofa_idates_filter_get_interface_last_version
                                                    ( const ofaIDatesFilter *filter );

void         ofa_idates_filter_setup_bin            ( ofaIDatesFilter *filter,
															const gchar *xml_name );

void         ofa_idates_filter_add_widget           ( ofaIDatesFilter *filter,
															GtkWidget *widget,
															gint where );

void         ofa_idates_filter_set_prefs            ( ofaIDatesFilter *filter,
															const gchar *prefs_key );

const GDate *ofa_idates_filter_get_date             ( ofaIDatesFilter *filter,
															gint who );

void         ofa_idates_filter_set_date             ( ofaIDatesFilter *filter,
															gint who,
															const GDate *date );

gboolean     ofa_idates_filter_is_valid             ( ofaIDatesFilter *filter,
															gint who,
															gchar **message );

GtkWidget   *ofa_idates_filter_get_entry            ( ofaIDatesFilter *filter,
															gint who );

GtkWidget   *ofa_idates_filter_get_frame_label      ( ofaIDatesFilter *filter );

GtkWidget   *ofa_idates_filter_get_prompt           ( ofaIDatesFilter *filter,
															gint who );

G_END_DECLS

#endif /* __OFA_IDATES_FILTER_H__ */
