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

#ifndef __OFA_RECURRENT_H__
#define __OFA_RECURRENT_H__

/**
 * SECTION: ofa_recurrent
 * @short_description: #ofaRecurrent class definition.
 * @include: openbook/ofa-recurrent.h
 *
 * Manage the recurrent operations.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFA_TYPE_RECURRENT                ( ofa_recurrent_get_type())
#define OFA_RECURRENT( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_RECURRENT, ofaRecurrent ))
#define OFA_RECURRENT_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_RECURRENT, ofaRecurrentClass ))
#define OFA_IS_RECURRENT( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_RECURRENT ))
#define OFA_IS_RECURRENT_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_RECURRENT ))
#define OFA_RECURRENT_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_RECURRENT, ofaRecurrentClass ))

typedef struct _ofaRecurrentPrivate       ofaRecurrentPrivate;

typedef struct {
	/*< public members >*/
	GObject              parent;

	/*< private members >*/
	ofaRecurrentPrivate *priv;
}
	ofaRecurrent;

typedef struct {
	/*< public members >*/
	GObjectClass         parent;
}
	ofaRecurrentClass;

GType  ofa_recurrent_get_type     ( void );

void   ofa_recurrent_register_type( GTypeModule *module );

G_END_DECLS

#endif /* __OFA_RECURRENT_H__ */
