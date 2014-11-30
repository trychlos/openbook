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

#ifndef __OPENBOOK_API_OFA_IDATASET_H__
#define __OPENBOOK_API_OFA_IDATASET_H__

/**
 * SECTION: idataset
 * @title: ofaIDataset
 * @short_description: The IDataset Interface
 * @include: api/ofa-idataset.h
 *
 * The #ofaIDataset interface is implemented by the #ofoDossier class,
 * so that it is able to manage the datasets of some reference classes
 * (e.g.accounts, and so on) by associating the GType of the class to
 * a #GList of all objects of the class, loaded at once on demand.
 */

#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_IDATASET                      ( ofa_idataset_get_type())
#define OFA_IDATASET( instance )               ( G_TYPE_CHECK_INSTANCE_CAST( instance, OFA_TYPE_IDATASET, ofaIDataset ))
#define OFA_IS_IDATASET( instance )            ( G_TYPE_CHECK_INSTANCE_TYPE( instance, OFA_TYPE_IDATASET ))
#define OFA_IDATASET_GET_INTERFACE( instance ) ( G_TYPE_INSTANCE_GET_INTERFACE(( instance ), OFA_TYPE_IDATASET, ofaIDatasetInterface ))

typedef struct _ofaIDataset                    ofaIDataset;

/**
 * ofaIDatasetInterface:
 *
 * This defines the interface that an #ofaIDataset should implement.
 */
typedef struct {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	/**
	 * get_interface_version:
	 * @instance: the #ofaIDataset provider.
	 *
	 * The interface calls this method each time it need to know which
	 * version is implented.
	 *
	 * Return value: if implemented, this method must return the version
	 * number of this interface the provider is supporting.
	 *
	 * Defaults to 1.
	 */
	guint    ( *get_interface_version )( const ofaIDataset *instance );

	/**
	 * get_datasets:
	 * @instance: the #ofaIDataset instance.
	 *
	 * Returns: the list of held datasets.
	 */
	GList *  ( *get_datasets )          ( const ofaIDataset *instance );

	/**
	 * set_datasets:
	 * @instance: the #ofaIDataset instance.
	 * @list: the new list of datasets.
	 */
	void     ( *set_datasets )          ( ofaIDataset *instance,
													GList *datasets );
}
	ofaIDatasetInterface;

/**
 * OFA_IDATASET_GET:
 * @D: the #ofoDossier dossier variable (e.g. 'dossier')
 * @T: the GType mnemonic name of the loaded class (e.g. 'ACCOUNT')
 * @P: the prefix used when naming the internal class methods
 *  (e.g. 'account')
 *
 * ex: dataset = OFA_IDATASET_GET( dossier, ACCOUNT, account );
 *
 * This macro returns the dataset, loading it if not already done.
 */
#define OFA_IDATASET_GET( D,T,P ) \
		GList *P ## _dataset = ofa_idataset_get_dataset((D), OFO_TYPE_ ## T ); \
			if( !P ## _dataset ){ \
				P ## _dataset = P ## _load_dataset( D ); \
				ofa_idataset_set_dataset((D), OFO_TYPE_ ## T, P ## _dataset ); \
			}

/**
 * OFA_IDATASET_LOAD:
 * @T: the GType mnemonic name of the loaded class (e.g. 'ACCOUNT')
 * @P: the prefix used when naming the internal class methods
 *  (e.g. 'account')
 *
 * ex: dataset = OFA_IDATASET_LOAD( dossier, ACCOUNT, account );
 *
 * This macro defines a public function named ofo_<T>_get_dataset().
 * This function loads the dataset if not already done, and returns it.
 *
 * This macro takes care of loading the dataset for the specified class,
 * recording it in the #ofoDossier @dossier private data, and then
 * returning it as a #GList.
 *
 * The #ofoDossier takes care of freeing the list on dispose time.
 */
#define OFA_IDATASET_LOAD( T,P ) \
		GList *ofo_ ## P ## _get_dataset( ofoDossier *dossier ) { \
		static const gchar *thisfn = "ofo_" #P "_get_dataset"; \
		g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL ); \
		g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier ); \
		OFA_IDATASET_GET( dossier,T,P ); \
		return( P ## _dataset ); }

/**
 * OFA_IDATASET_ADD:
 * @D: the dossier
 * @T: the GType mnemonic name of the loaded class (e.g. 'ACCOUNT')
 * @P: both the prefix used when naming the internal class methods
 *  and the name of the variable to be added (e.g. 'account')
 *
 * ex: OFA_IDATASET_ADD( dossier, ACCOUNT, account )
 *
 * a) insert the 'P' object (named <P> and of type OFO_TYPE_<T>) in the
 *    global dataset, keeping it sorted by calling the <P>_cmp_by_ptr()
 *    method
 *
 * b) send a SIGNAL_DOSSIER_NEW_OBJECT signal to the opened dossier,
 *    associated to the <P> object
 */
#define OFA_IDATASET_ADD( D,T,P ) \
		OFA_IDATASET_GET(D,T,P); \
		P ## _dataset = g_list_insert_sorted( P ## _dataset, (P), ( GCompareFunc ) P ## _cmp_by_ptr ); \
		ofa_idataset_set_dataset((D), OFO_TYPE_ ## T, P ## _dataset ); \
		if( ofa_idataset_is_signal_new_allowed((D), OFO_TYPE_ ## T )){ \
			g_signal_emit_by_name( G_OBJECT(D), SIGNAL_DOSSIER_NEW_OBJECT, g_object_ref(P)); }

/**
 * OFA_IDATASET_UPDATE:
 * @D: the dossier,
 * @T: the GType mnemonic name of the loaded class (e.g. 'ACCOUNT')
 * @P: both the prefix used when naming the internal class methods
 *  and the name of the variable to be added (e.g. 'account')
 * @I: the previous string identifier which may have been modified
 *
 * ex: OFA_IDATASET_UPDATE( dossier, ACCOUNT, account, prev_number )
 *
 * a) send a SIGNAL_DOSSIER_UPDATED_OBJECT signal to the opened dossier,
 *    associated to the <T> object; the previous identifier is passed
 *    as an argument for tree views updates
 */
#define OFA_IDATASET_UPDATE( D,T,P,I ) \
		OFA_IDATASET_GET(D,T,P); \
		P ## _dataset = g_list_sort( P ## _dataset, ( GCompareFunc ) P ## _cmp_by_ptr ); \
		ofa_idataset_set_dataset((D), OFO_TYPE_ ## T, P ## _dataset ); \
		g_signal_emit_by_name( G_OBJECT(D), SIGNAL_DOSSIER_UPDATED_OBJECT, g_object_ref(P), (I));

/**
 * OFA_IDATASET_REMOVE:
 * @D: the dossier,
 * @T: the GType mnemonic name of the loaded class (e.g. 'ACCOUNT')
 * @P: both the prefix used when naming the internal class methods
 *  and the name of the variable to be added (e.g. 'account')
 *
 * ex: OFA_IDATASET_REMOVE( dossier, ACCOUNT, account )
 *
 * a) remove the 'P' object (named <P> and of type OFO_TYPE_<T>) from
 *    its dataset in the dossier.
 *
 * b) send a SIGNAL_DOSSIER_DELETED_OBJECT signal to the opened dossier,
 *    associated to the <P> object
 */
#define OFA_IDATASET_REMOVE( D,T,P ) \
		OFA_IDATASET_GET(D,T,P); \
		P ## _dataset = g_list_remove( P ## _dataset, (P)); \
		ofa_idataset_set_dataset((D), OFO_TYPE_ ## T, P ## _dataset ); \
		g_signal_emit_by_name( G_OBJECT(D), SIGNAL_DOSSIER_DELETED_OBJECT, (P));

GType    ofa_idataset_get_type                  ( void );

guint    ofa_idataset_get_interface_last_version( void );

GList   *ofa_idataset_get_dataset               ( ofoDossier *dossier, GType type );

void     ofa_idataset_free_dataset              ( ofoDossier *dossier, GType type );

void     ofa_idataset_set_dataset               ( ofoDossier *dossier, GType type, GList *dataset );

gboolean ofa_idataset_is_signal_new_allowed     ( ofoDossier *dossier, GType type );

void     ofa_idataset_set_signal_new_allowed    ( ofoDossier *dossier, GType type, gboolean allowed );

void     ofa_idataset_free_full                 ( void *data );

G_END_DECLS

#endif /* __OPENBOOK_API_OFA_IDATASET_H__ */
