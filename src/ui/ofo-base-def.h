/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014 Pierre Wieser (see AUTHORS)
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
 *
 * $Id: ofo-base.h 3494 2014-05-24 18:50:40Z  $
 */

#ifndef __OFO_BASE_DEF_H__
#define __OFO_BASE_DEF_H__

/**
 * SECTION: ofo_base
 * @short_description: #ofoBase class definition.
 * @include: ui/ofo-base.h
 *
 * The ofoBase class is the class base for application objects.
 */

#include <glib-object.h>

G_BEGIN_DECLS

#define OFO_TYPE_BASE                ( ofo_base_get_type())
#define OFO_BASE( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFO_TYPE_BASE, ofoBase ))
#define OFO_BASE_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFO_TYPE_BASE, ofoBaseClass ))
#define OFO_IS_BASE( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFO_TYPE_BASE ))
#define OFO_IS_BASE_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFO_TYPE_BASE ))
#define OFO_BASE_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFO_TYPE_BASE, ofoBaseClass ))

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofoBaseClass;

typedef struct _ofoBasePrivate       ofoBasePrivate;
typedef struct _ofoBaseProtected     ofoBaseProtected;

typedef struct {
	/*< private >*/
	GObject           parent;
	ofoBasePrivate   *private;
	ofoBaseProtected *prot;
}
	ofoBase;

/**
 * OFA_SIGNAL_NEW_OBJECT:
 * OFA_SIGNAL_UPDATED_OBJECT:
 * OFA_SIGNAL_DELETED_OBJECT:
 * OFA_SIGNAL_RELOADED_DATASET:
 */
#define OFA_SIGNAL_NEW_OBJECT           "ofa-signal-new-object"
#define OFA_SIGNAL_UPDATED_OBJECT       "ofa-signal-updated-object"
#define OFA_SIGNAL_DELETED_OBJECT       "ofa-signal-deleted-object"
#define OFA_SIGNAL_RELOADED_DATASET     "ofa-signal-reloaded-dataset"
#define OFA_SIGNAL_VALIDATED_ENTRY      "ofa-signal-validated-entry"

G_END_DECLS

#endif /* __OFO_BASE_DEF_H__ */
