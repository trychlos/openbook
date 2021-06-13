/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#ifndef __OPENBOOK_API_OFA_IEXECLOSER_H__
#define __OPENBOOK_API_OFA_IEXECLOSER_H__

/**
 * SECTION: iexe_closer
 * @title: ofaIExeCloser
 * @short_description: The Exercice Closing Interface
 * @include: openbook/ofa-iexe-closer.h
 *
 * The #ofaIExeCloser interfaces is intended to be implemented by the Exercice
 * Closing assistant.
 * Il will let the #ofaIExeCloseable implementations dialog with their calling
 * assistant.
 */

#include <gtk/gtk.h>

#include "api/ofa-igetter-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IEXECLOSER                      ( ofa_iexe_closer_get_type())
#define OFA_IEXECLOSER( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IEXECLOSER, ofaIExeCloser ))
#define OFA_IS_IEXECLOSER( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IEXECLOSER ))
#define OFA_IEXECLOSER_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IEXECLOSER, ofaIExeCloserInterface ))

typedef struct _ofaIExeCloser                    ofaIExeCloser;

/**
 * ofaIExeCloserInterface:
 * @get_interface_version: [should]: returns the implemented version number.
 * @add_row: [should]: insert a row on the page when closing the exercice.
 * @do_task: [should]: do the task.
 *
 * This defines the interface that an #ofaIExeCloser may/should implement.
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
	guint    ( *get_interface_version )( void );

	/*** instance-wide ***/
	/**
	 * get_prev_begin_date:
	 * @instance: the #ofaIExeCloser instance.
	 *
	 * Returns: the beginning date of the exercice N, or %NULL if
	 * it has noot been set yet.
	 *
	 * Since: version 1
	 */
	const GDate * ( *get_prev_begin_date )( ofaIExeCloser *instance );

	/**
	 * get_prev_end_date:
	 * @instance: the #ofaIExeCloser instance.
	 *
	 * Returns: the ending date of the exercice N, or %NULL if
	 * it has noot been set yet.
	 *
	 * Since: version 1
	 */
	const GDate * ( *get_prev_end_date ) ( ofaIExeCloser *instance );

	/**
	 * get_next_begin_date:
	 * @instance: the #ofaIExeCloser instance.
	 *
	 * Returns: the beginning date of the exercice N+1, or %NULL if
	 * it has noot been set yet.
	 *
	 * Since: version 1
	 */
	const GDate * ( *get_next_begin_date )( ofaIExeCloser *instance );

	/**
	 * get_next_end_date:
	 * @instance: the #ofaIExeCloser instance.
	 *
	 * Returns: the ending date of the exercice N+1, or %NULL if
	 * it has noot been set yet.
	 *
	 * Since: version 1
	 */
	const GDate * ( *get_next_end_date ) ( ofaIExeCloser *instance );
}
	ofaIExeCloserInterface;

/*
 * Interface-wide
 */
GType        ofa_iexe_closer_get_type                  ( void );

guint        ofa_iexe_closer_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint        ofa_iexe_closer_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
const GDate *ofa_iexe_closer_get_prev_begin_date       ( ofaIExeCloser *instance );

const GDate *ofa_iexe_closer_get_prev_end_date         ( ofaIExeCloser *instance );

const GDate *ofa_iexe_closer_get_next_begin_date       ( ofaIExeCloser *instance );

const GDate *ofa_iexe_closer_get_next_end_date         ( ofaIExeCloser *instance );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IEXECLOSER_H__ */
