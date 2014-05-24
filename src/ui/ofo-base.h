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
 * $Id$
 */

#ifndef __OFO_BASE_H__
#define __OFO_BASE_H__

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

typedef struct {
	/*< private >*/
	GObject         parent;
	ofoBasePrivate *priv;
}
	ofoBase;

/**
 * ofoBaseStatic:
 *
 * This structure is used by every derived class (but ofoDossier which
 * doesn't need it), in order to store its own global data.
 * It is the responsability of the user child class to manage its own
 * version of this structure, usually through a static pointer to a
 * dynamically allocated structure.
 */
typedef struct {
	GList   *dataset;
	ofoBase *dossier;
}
	ofoBaseGlobal;

#define OFO_BASE_DEFINE_GLOBAL( V,T )       static ofoBaseGlobal *(V)=NULL; static void T ## _clear_global( gpointer user_data, GObject *finalizing_dossier ) \
												{ g_debug( #T "_clear_global:" ); if(V){ g_list_foreach((V)->dataset, (GFunc) g_object_unref, NULL ); \
												g_list_free((V)->dataset ); g_free(V); (V)=NULL; }}

#define OFO_BASE_SET_GLOBAL( P,D,T )        ({ (P)=ofo_base_get_global((P),OFO_BASE(D),(GWeakNotify)(T ## _clear_global),NULL); if(!(P)->dataset){ (P)->dataset=(T ## _load_dataset)();} })

#define OFO_BASE_ADD_TO_DATASET( P,T )      ({ (P)->dataset=g_list_insert_sorted((P)->dataset,(T),(GCompareFunc)(T ## _cmp_by_ptr)); })
#define OFO_BASE_REMOVE_FROM_DATASET( P,T ) ({ (P)->dataset=g_list_remove((P)->dataset,(T)); g_object_unref(T); })
#define OFO_BASE_UPDATE_DATASET( P,T )      ({ g_object_ref(T); OFO_BASE_REMOVE_FROM_DATASET((P),(T)); OFO_BASE_ADD_TO_DATASET((P),T); })

#define OFO_BASE_UNSET_ID                   -1

GType          ofo_base_get_type  ( void ) G_GNUC_CONST;

ofoBaseGlobal *ofo_base_get_global( ofoBaseGlobal *ptr,
									ofoBase *dossier, GWeakNotify fn, gpointer user_data );

G_END_DECLS

#endif /* __OFO_BASE_H__ */
