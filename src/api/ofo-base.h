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

#include "api/ofa-boxed.h"
#include "api/ofa-dbms-def.h"
#include "api/ofo-base-def.h"

G_BEGIN_DECLS

/**
 * ofsBaseGlobal:
 * @dataset: maintains the list of all the #ofoBase -derived objects.
 * @dossier: a pointer to the #ofoDossier object.
 * @send_signal_new: whether to send the corresponding signal when
 *  inserting a new record in DBMS.
 *
 * This structure is used by every derived class (apart from
 * #ofoDossier which doesn't need it), in order to store its own global
 * data.
 *
 * It is the responsability of the user child class to manage its own
 * version of this structure, usually through a static pointer to a
 * dynamically allocated structure (but see the macros below which may
 * greatly help in this matter).
 *
 * Data types are:
 *   Specifications  SQL            C                   Max value
 *   --------------  -------------  ------------------  --------------------------
 *   INTEGER         INTEGER        int
 *   NUMBER          BIGINT         gint64 aka ofxCounter  +9,223,372,036,854,775,807
 *   MONTANT         DECIMAL(20,5)  gdouble aka ofxAmount  1.79769e+308
 *
 */

typedef struct {
	GList    *dataset;
	ofoBase  *dossier;
	gboolean  send_signal_new;
}
	ofsBaseGlobal;

/**
 * OFO_BASE_DEFINE_GLOBAL:
 *
 * ex: OFO_BASE_DEFINE_GLOBAL( st_global, class )
 *
 * a) defines a static pointer 'V' to a dynamically allocated
 *    ofsBaseGlobal structure. This structure will be actually
 *    allocated and initialised by the #OFO_BASE_SET_GLOBAL macro.
 *
 * b) defines a 'T'_clear_global static function which fully clears the
 *    above ofsBaseGlobal structure and its data.
 *
 * This macro is to be invoked at the toplevel, once in each source
 * file.
 */
#define OFO_BASE_DEFINE_GLOBAL( V,T )	\
		static ofsBaseGlobal *(V)=NULL; static void T ## _clear_global( gpointer \
		user_data, GObject *finalizing_dossier ){ \
		g_debug( "ofo_" #T "_clear_global: " #V "=%p, count=%u", ( void * )(V), \
		((V) && (V)->dataset) ? g_list_length((V)->dataset) : 0 ); \
		if(V){ g_list_foreach((V)->dataset, (GFunc) g_object_unref, NULL ); \
		g_list_free((V)->dataset ); g_free(V); (V)=NULL; }}

/**
 * OFO_BASE_SET_GLOBAL:
 * @V: the name of the global variable
 *     e.g. 'st_global'
 * @D: the name of the #ofoDossier variable
 *     e.g. 'dossier'
 * @T: both the name of the class and the name of the object
 *     e.g. 'class', 'account', etc.
 *
 * ex: OFO_BASE_SET_GLOBAL( st_global, dossier, class )
 *
 * a) makes sure that the global ofsBaseGlobal structure is allocated
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
#define OFO_BASE_SET_GLOBAL( V,D,T )	\
		({ (V)=ofo_base_get_global((V),OFO_BASE(D),(GWeakNotify)(T ## _clear_global), \
		NULL); if(!(V)->dataset){ (V)->dataset=(T ## _load_dataset)();} })

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
 *    global dataset, keeping it sorted by calling the <T>_cmp_by_ptr()
 *    method
 *
 * b) send a OFA_SIGNAL_NEW_OBJECT signal to the opened dossier,
 *    associated to the <T> object
 */
#define OFO_BASE_ADD_TO_DATASET( P,T )	\
		({ (P)->dataset=g_list_insert_sorted((P)->dataset,(T), (GCompareFunc) T ## _cmp_by_ptr); \
		if((P)->send_signal_new) \
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
 * a) remove the 'T' object (named <T> and of type OFO_TYPE_<T>) from
 *    the global dataset.
 *
 * b) send a OFA_SIGNAL_DELETED_OBJECT signal to the opened dossier,
 *    associated to the <T> object
 */
#define OFO_BASE_REMOVE_FROM_DATASET( P,T )	\
		({ (P)->dataset=g_list_remove((P)->dataset,(T)); g_signal_emit_by_name( \
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
#define OFO_BASE_UPDATE_DATASET( P,T,I )	\
		({ g_signal_emit_by_name( G_OBJECT((P)->dossier), OFA_SIGNAL_UPDATED_OBJECT, \
		g_object_ref(T),(I)); })

#define OFO_BASE_UNSET_ID                   -1

ofsBaseGlobal *ofo_base_get_global( ofsBaseGlobal *ptr,
										ofoBase *dossier, GWeakNotify fn, gpointer user_data );

/**
 * ofo_base_getter:
 * @C: the class mnemonic (e.g. 'ACCOUNT')
 * @V: the variable name (e.g. 'account')
 * @T: the type of required data (e.g. 'amount')
 * @R: the returned data in case of an error (e.g. '0')
 * @I: the identifier of the required field (e.g. 'ACC_DEB_AMOUNT')
 *
 * A convenience macro to get the value of an identified field from an
 * #ofoBase object.
 */
#define ofo_base_getter(C,V,T,R,I)			\
		g_return_val_if_fail( OFO_IS_ ## C(V),(R)); \
		if( OFO_BASE(V)->prot->dispose_has_run) return(R); \
		return(ofa_boxed_get_ ## T(OFO_BASE(V)->prot->fields,(I)))

/**
 * ofo_base_setter:
 * @C: the class mnemonic (e.g. 'ACCOUNT')
 * @V: the variable name (e.g. 'account')
 * @T: the type of required data (e.g. 'amount')
 * @I: the identifier of the required field (e.g. 'ACC_DEB_AMOUNT')
 * @D: the data value to be set
 *
 * A convenience macro to set the value of an identified field from an
 * #ofoBase object.
 */
#define ofo_base_setter(C,V,T,I,D)			\
		g_return_if_fail( OFO_IS_ ## C(V)); \
		if( OFO_BASE(V)->prot->dispose_has_run ) return; \
		ofa_boxed_set_ ## T(OFO_BASE(V)->prot->fields,(I),(D))

void   ofo_base_init_fields_list(const ofsBoxedDef *defs, ofoBase *object );

GList *ofo_base_load_dataset    ( const ofsBoxedDef *defs, const ofaDbms *dbms, const gchar *from, GType type );

G_END_DECLS

#endif /* __OFO_BASE_H__ */
