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

#ifndef __OFA_ICONCIL_H__
#define __OFA_ICONCIL_H__

/**
 * SECTION: iconcil
 * @title: ofaIConcil
 * @short_description: The Reconciliation Interface
 * @include: core/ofa-iconcil.h
 *
 * This interface is implemented by classes which are involved in
 * reconcilition processus: the #ofoEntry and the #ofoBatLine.
 *
 * The #ofaIConcil lets the reconciliable objects (entry and bat line)
 * be managed together inside of the reconciliation group (#ofoConcil
 * object).
 */

#include "api/ofo-concil-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_ICONCIL                      ( ofa_iconcil_get_type())
#define OFA_ICONCIL( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_ICONCIL, ofaIConcil ))
#define OFA_IS_ICONCIL( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_ICONCIL ))
#define OFA_ICONCIL_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_ICONCIL, ofaIConcilInterface ))

typedef struct _ofaIConcil                    ofaIConcil;
typedef struct _ofaIConcilParms               ofaIConcilParms;

/**
 * ofaIConcilInterface:
 * @get_interface_version: [should] returns the version of this
 *                                  interface that the plugin implements.
 *
 * This defines the interface that an #ofaIConcil should implement.
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
	 * get_object_id:
	 * @instance: the #ofaIConcil instance.
	 *
	 * Return value: the implementation must return the internal
	 * identifier of the #ofoBase @instance.
	 *
	 * The returned identifier is the same that the one which is recorded
	 * in the OFA_T_CONCIL_IDS table.
	 */
	ofxCounter    ( *get_object_id )        ( const ofaIConcil *instance );

	/**
	 * get_object_type:
	 * @instance: the #ofaIConcil instance.
	 *
	 * Return value: the implementation must return its type from the
	 * #ofoConcil point of view.
	 */
	const gchar * ( *get_object_type )      ( const ofaIConcil *instance );
}
	ofaIConcilInterface;

/*
 * Interface-wide
 */
GType        ofa_iconcil_get_type                  ( void );

guint        ofa_iconcil_get_interface_last_version( void );

/*
 * Implementation-wide
 */
guint        ofa_iconcil_get_interface_version     ( GType type );

/*
 * Instance-wide
 */
ofoConcil   *ofa_iconcil_get_concil                ( const ofaIConcil *instance );

ofoConcil   *ofa_iconcil_new_concil                ( ofaIConcil *instance,
														const GDate *dval );

void         ofa_iconcil_new_concil_ex             ( ofaIConcil *instance,
														ofoConcil *concil );

void         ofa_iconcil_add_to_concil             ( ofaIConcil *instance,
														ofoConcil *concil );

void         ofa_iconcil_clear_data                ( ofaIConcil *instance );

void         ofa_iconcil_remove_concil             ( ofaIConcil *instance,
														ofoConcil *concil );

const gchar *ofa_iconcil_get_instance_type         ( const ofaIConcil *instance );

ofxCounter   ofa_iconcil_get_instance_id           ( const ofaIConcil *instance );

G_END_DECLS

#endif /* __OFA_ICONCIL_H__ */
