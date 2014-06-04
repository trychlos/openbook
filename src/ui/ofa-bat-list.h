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

#ifndef __OFA_BAT_LIST_H__
#define __OFA_BAT_LIST_H__

/**
 * SECTION: ofa_bat_list
 * @short_description: #ofaBatList class definition.
 * @include: ui/ofa-bat-list.h
 *
 * A convenience class which displays in a treeview the list of imported
 * Bank Account Transaction (BAT) lists.
 */

#include "ui/ofo-bat-def.h"
#include "ui/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_LIST                ( ofa_bat_list_get_type())
#define OFA_BAT_LIST( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_LIST, ofaBatList ))
#define OFA_BAT_LIST_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_LIST, ofaBatListClass ))
#define OFA_IS_BAT_LIST( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_LIST ))
#define OFA_IS_BAT_LIST_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_LIST ))
#define OFA_BAT_LIST_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_LIST, ofaBatListClass ))

typedef struct _ofaBatListPrivate        ofaBatListPrivate;

typedef struct {
	/*< private >*/
	GObject            parent;
	ofaBatListPrivate *private;
}
	ofaBatList;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaBatListClass;

/**
 * ofaBatListCb:
 *
 * A callback to be triggered when a new row is selected, or a row is
 * activated.
 *
 * Passed parameters are:
 * - #ofoBat object
 * - user_data provided at init_dialog() time
 */
typedef void ( *ofaBatListCb )( const ofoBat *, gpointer );

/**
 * ofaBatListParms
 *
 * The structure passed to the init_dialog() function.
 *
 * @container: the parent container of the target view
 * @dossier: the currently opened ofoDossier
 * @pfn: [allow-none]: a user-provided callback which will be triggered
 *  on each selection change
 * @user_data: user-data passed to the callback
 */
typedef struct {
	GtkContainer     *container;
	ofoDossier       *dossier;
	ofaBatListCb      pfnSelection;
	ofaBatListCb      pfnActivation;
	gpointer          user_data;
}
	ofaBatListParms;

GType         ofa_bat_list_get_type     ( void ) G_GNUC_CONST;

ofaBatList   *ofa_bat_list_init_dialog  ( const ofaBatListParms *parms );

const ofoBat *ofa_bat_list_get_selection( const ofaBatList *self );

G_END_DECLS

#endif /* __OFA_BAT_LIST_H__ */
