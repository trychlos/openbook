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

#ifndef __OPENBOOK_API_OFA_IFILE_PERIOD_H__
#define __OPENBOOK_API_OFA_IFILE_PERIOD_H__

/**
 * SECTION: ifile_period
 * @title: ofaIFilePeriod
 * @short_description: An interface to manage the financial periods of a dossier
 * @include: openbook/ofa-ifile-period.h
 *
 * The #ofaIFilePeriod interface manages the financial periods of a
 * dossier, and any other external properties.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IFILE_PERIOD                      ( ofa_ifile_period_get_type())
#define OFA_IFILE_PERIOD( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IFILE_PERIOD, ofaIFilePeriod ))
#define OFA_IS_IFILE_PERIOD( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IFILE_PERIOD ))
#define OFA_IFILE_PERIOD_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IFILE_PERIOD, ofaIFilePeriodInterface ))

typedef struct _ofaIFilePeriod                     ofaIFilePeriod;

/**
 * ofaIFilePeriodInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @get_begin_date: [should]: returns the beginning date.
 * @get_end_date: [should]: returns the ending date.
 * @get_status: [should]: returns the localized status.
 *
 * This defines the interface that an #ofaIFilePeriod should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIFilePeriod instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implemented by the instance.
	 *
	 * Returns: the version number of this interface that the @instance
	 *  is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIFilePeriod *instance );

	/**
	 * get_begin_date:
	 * @instance: the #ofaIFilePeriod instance.
	 * @date: a GDate
	 *
	 * Returns: the beginning date of the period.
	 */
	GDate *  ( *get_begin_date )       ( const ofaIFilePeriod *instance,
													GDate *date );

	/**
	 * get_end_date:
	 * @instance: the #ofaIFilePeriod instance.
	 * @date: a GDate
	 *
	 * Returns: the ending date of the period.
	 */
	GDate *  ( *get_end_date )         ( const ofaIFilePeriod *instance,
													GDate *date );

	/**
	 * get_current:
	 * @instance: the #ofaIFilePeriod instance.
	 *
	 * Returns: whether the @instance period is current (can be
	 * modified) or not.
	 */
	gboolean ( *get_current )          ( const ofaIFilePeriod *instance );

	/**
	 * get_status:
	 * @instance: the #ofaIFilePeriod instance.
	 *
	 * Returns: the status of the period as a newly allocated localized
	 * string which should be g_free() by the caller.
	 */
	gchar *  ( *get_status )           ( const ofaIFilePeriod *instance );
}
	ofaIFilePeriodInterface;

GType    ofa_ifile_period_get_type                  ( void );

guint    ofa_ifile_period_get_interface_last_version( void );

guint    ofa_ifile_period_get_interface_version     ( const ofaIFilePeriod *instance );

GDate   *ofa_ifile_period_get_begin_date            ( const ofaIFilePeriod *instance,
																GDate *date );

GDate   *ofa_ifile_period_get_end_date              ( const ofaIFilePeriod *instance,
																GDate *date );

gboolean ofa_ifile_period_get_current               ( const ofaIFilePeriod *instance );

gchar   *ofa_ifile_period_get_status                ( const ofaIFilePeriod *instance );


G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IFILE_PERIOD_H__ */