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
 * @include: api/ofo-base.h
 *
 * The ofoBase class is the class base for application objects.
 */

#include "api/ofo-base-def.h"

G_BEGIN_DECLS

/**
 * ofoBaseGlobal:
 *
 * This structure is used by every derived class (but ofoDossier which
 * doesn't need it), in order to store its own global data.
 *
 * It is the responsability of the user child class to manage its own
 * version of this structure, usually through a static pointer to a
 * dynamically allocated structure (but see the macros below which may
 * greatly help in this matter).
 */
typedef struct {
	GList    *dataset;
	ofoBase  *dossier;
	gboolean  send_signal_new;
}
	ofoBaseGlobal;

/**
 * OFO_BASE_DEFINE_GLOBAL:
 *
 * ex: OFO_BASE_DEFINE_GLOBAL( st_global, class )
 *
 * a) defines a static pointer 'V' to a dynamically allocated
 *    ofoBaseGlobal structure. This structure will be actually
 *    allocated and initialised by the #OFO_BASE_SET_GLOBAL macro.
 *
 * b) defines a 'T'_clear_global static function which fully clears the
 *    above ofoBaseGlobal structure and its data.
 *
 * This macro is to be invoked at the toplevel, once in each source
 * file.
 */
#define OFO_BASE_DEFINE_GLOBAL( V,T )   static ofoBaseGlobal *(V)=NULL; static void T ## _clear_global( gpointer \
											user_data, GObject *finalizing_dossier ){ g_debug( "ofo_" #T "_clear_global:" ); \
											if(V){ g_list_foreach((V)->dataset, (GFunc) g_object_unref, NULL ); \
											g_list_free((V)->dataset ); g_free(V); (V)=NULL; }}

/**
 * OFO_BASE_SET_GLOBAL:
 * @P: the name of the global variable
 *     e.g. 'st_global'
 * @D: the name of the #ofoDossier variable
 *     e.g. 'dossier'
 * @T: both the name of the class and the name of the object
 *     e.g. 'class', 'account', etc.
 *
 * ex: OFO_BASE_SET_GLOBAL( st_global, dossier, class )
 *
 * a) makes sure that the global ofoBaseGlobal structure is allocated
 *    and initialized
 *
 * b) auto attach to the 'dossier', so that we will clear the global
 *    structure when the dossier be finalized
 *
 * c) invokes the local 'T'_load_dataset function which is supposed to
 *    load the data, populating the dataset structure member.
 *
 * This macro is supposed to be invoked from each global function, in
 * order to be sure the global structure is available.
 * It is safe to invoke it many times, as it is auto-protected.
 */
#define OFO_BASE_SET_GLOBAL( P,D,T )    ({ (P)=ofo_base_get_global((P),OFO_BASE(D),(GWeakNotify)(T ## _clear_global), \
											NULL); if(!(P)->dataset){ (P)->dataset=(T ## _load_dataset)();} })

/**
 * OFO_BASE_ADD_TO_DATASET:
 * @P: the name of the global variable
 *     e.g. 'st_global'
 * @T: both the name of the class and the name of the object
 *     e.g. 'class', 'account', etc.
 *
 * ex: OFO_BASE_ADD_TO_DATASET( st_global, class )
 *
 * a) insert the 'T' object (named <T> and of type OFO_TYPE_<T>) in the
 *    global dataset
 *
 * b) send a OFA_SIGNAL_NEW_OBJECT signal to the opened dossier,
 *    associated to the <T> object
 */
#define OFO_BASE_ADD_TO_DATASET( P,T )  ({ (P)->dataset=g_list_prepend((P)->dataset,(T)); if((P)->send_signal_new) \
											{ g_signal_emit_by_name( G_OBJECT((P)->dossier), OFA_SIGNAL_NEW_OBJECT, \
											g_object_ref(T)); }})

/**
 * OFO_BASE_REMOVE_FROM_DATASET:
 * @P: the name of the global variable
 *     e.g. 'st_global'
 * @T: both the name of the class and the name of the object
 *     e.g. 'class', 'account', etc.
 *
 * ex: OFO_BASE_REMOVE_FROM_DATASET( st_global, class )
 *
 * a) insert the 'T' object (named <T> and of type OFO_TYPE_<T>) in the
 *    global dataset, keeping it sorted by calling the <T>_cmp_by_ptr()
 *    method
 *
 * b) send a OFA_SIGNAL_DELETED_OBJECT signal to the opened dossier,
 *    associated to the <T> object
 */
#define OFO_BASE_REMOVE_FROM_DATASET( P,T ) ({ (P)->dataset=g_list_remove((P)->dataset,(T)); g_signal_emit_by_name( \
											G_OBJECT((P)->dossier), OFA_SIGNAL_DELETED_OBJECT, (T)); })

/**
 * OFO_BASE_UPDATE_DATASET:
 * @P: the name of the global variable
 *     e.g. 'st_global'
 * @T: both the name of the class and the name of the object
 *     e.g. 'class', 'account', etc.
 * @I: the previous string identifier
 *
 * ex: OFO_BASE_UPDATE_DATASET( st_global, class, prev_number )
 *
 * a) send a OFA_SIGNAL_UPDATED_OBJECT signal to the opened dossier,
 *    associated to the <T> object; the previous identifier is passed
 *    as an argument for tree views updates
 */
#define OFO_BASE_UPDATE_DATASET( P,T,I )    ({ g_signal_emit_by_name( G_OBJECT((P)->dossier), OFA_SIGNAL_UPDATED_OBJECT, \
												g_object_ref(T),(I)); })

#define OFO_BASE_UNSET_ID                   -1

GType          ofo_base_get_type  ( void ) G_GNUC_CONST;

ofoBaseGlobal *ofo_base_get_global( ofoBaseGlobal *ptr,
										ofoBase *dossier, GWeakNotify fn, gpointer user_data );

G_END_DECLS

#endif /* __OFO_BASE_H__ */
