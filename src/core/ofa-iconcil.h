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

#include "api/ofo-dossier-def.h"

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
	/**
	 * get_interface_version:
	 * @instance: the #ofaIConcil instance.
	 *
	 * The application calls this method each time it needs to know
	 * which version of this interface the plugin implements.
	 *
	 * If this method is not implemented by the plugin,
	 * the application considers that the plugin only implements
	 * the version 1 of the ofaIConcil interface.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint         ( *get_interface_version )( const ofaIConcil *instance );

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

GType      ofa_iconcil_get_type                  ( void );

guint      ofa_iconcil_get_interface_last_version( void );

guint      ofa_iconcil_get_interface_version     ( const ofaIConcil *instance );

ofoConcil *ofa_iconcil_get_concil                ( const ofaIConcil *instance,
														ofoDossier *dossier );

ofoConcil *ofa_iconcil_new_concil                ( ofaIConcil *instance,
														const GDate *dval,
														ofoDossier *dossier );

void       ofa_iconcil_add_to_concil             ( ofaIConcil *instance,
														ofoConcil *concil,
														ofoDossier *dossier );

void       ofa_iconcil_remove_concil             ( ofoConcil *concil,
														ofoDossier *dossier );

G_END_DECLS

#endif /* __OFA_ICONCIL_H__ */
