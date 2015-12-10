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

#ifndef __OPENBOOK_API_OFA_IDBPERIOD_H__
#define __OPENBOOK_API_OFA_IDBPERIOD_H__

/**
 * SECTION: idbperiod
 * @title: ofaIDBPeriod
 * @short_description: An interface to manage the financial periods of a dossier
 * @include: openbook/ofa-idbperiod.h
 *
 * The #ofaIDBPeriod interface manages the financial periods of a
 * dossier, and any other external properties.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_IDBPERIOD                      ( ofa_idbperiod_get_type())
#define OFA_IDBPERIOD( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDBPERIOD, ofaIDBPeriod ))
#define OFA_IS_IDBPERIOD( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDBPERIOD ))
#define OFA_IDBPERIOD_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDBPERIOD, ofaIDBPeriodInterface ))

typedef struct _ofaIDBPeriod                    ofaIDBPeriod;

/**
 * ofaIDBPeriodInterface:
 * @get_interface_version: [should]: returns the version of this
 *                         interface that the plugin implements.
 * @compare: [should]: compare two objects.
 * @dump: [should]: dump the object.
 *
 * This defines the interface that an #ofaIDBPeriod should/must
 * implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDBPeriod instance.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implemented by the instance.
	 *
	 * Returns: the version number of this interface that the @instance
	 *  is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIDBPeriod *instance );

	/**
	 * compare:
	 * @a: the #ofaIDBPeriod instance.
	 * @b: another #ofaIDBPeriod instance.
	 *
	 * Returns: 0 if @a and @b have the same content, -1 if @a < @b,
	 *  +1 if @a > @b.
	 */
	gint     ( *compare )              ( const ofaIDBPeriod *a,
												const ofaIDBPeriod *b );

	/**
	 * dump:
	 * @instance: the #ofaIDBPeriod instance.
	 *
	 * Dump the object.
	 */
	void     ( *dump )                 ( const ofaIDBPeriod *instance );
}
	ofaIDBPeriodInterface;

GType        ofa_idbperiod_get_type                  ( void );

guint        ofa_idbperiod_get_interface_last_version( void );

guint        ofa_idbperiod_get_interface_version     ( const ofaIDBPeriod *period );

const GDate *ofa_idbperiod_get_begin_date            ( const ofaIDBPeriod *period );

void         ofa_idbperiod_set_begin_date            ( ofaIDBPeriod *period,
																const GDate *date );

const GDate *ofa_idbperiod_get_end_date              ( const ofaIDBPeriod *period );

void         ofa_idbperiod_set_end_date              ( ofaIDBPeriod *period,
																const GDate *date );

gboolean     ofa_idbperiod_get_current               ( const ofaIDBPeriod *period );

void         ofa_idbperiod_set_current               ( ofaIDBPeriod *period,
																gboolean current );

gchar       *ofa_idbperiod_get_status                ( const ofaIDBPeriod *period );

gchar       *ofa_idbperiod_get_label                 ( const ofaIDBPeriod *period );

gint         ofa_idbperiod_compare                   ( const ofaIDBPeriod *a,
																const ofaIDBPeriod *b );

void         ofa_idbperiod_dump                      ( const ofaIDBPeriod *period );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDBPERIOD_H__ */
