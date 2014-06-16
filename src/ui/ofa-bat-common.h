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

#ifndef __OFA_BAT_COMMON_H__
#define __OFA_BAT_COMMON_H__

/**
 * SECTION: ofa_bat_common
 * @short_description: #ofaBatCommon class definition.
 * @include: ui/ofa-bat-common.h
 *
 * A convenience class which displays :
 * - in a treeview, the list of imported Bank Account Transaction (BAT)
 *   lists
 * - in a notebook, the properties either of the currently selected
 *   BAT file (if with tree view), or of the provided BAT file.
 */

#include "api/ofo-bat-def.h"
#include "api/ofo-dossier-def.h"

G_BEGIN_DECLS

#define OFA_TYPE_BAT_COMMON                ( ofa_bat_common_get_type())
#define OFA_BAT_COMMON( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_BAT_COMMON, ofaBatCommon ))
#define OFA_BAT_COMMON_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_BAT_COMMON, ofaBatCommonClass ))
#define OFA_IS_BAT_COMMON( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_BAT_COMMON ))
#define OFA_IS_BAT_COMMON_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_BAT_COMMON ))
#define OFA_BAT_COMMON_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_BAT_COMMON, ofaBatCommonClass ))

typedef struct _ofaBatCommonPrivate        ofaBatCommonPrivate;

typedef struct {
	/*< private >*/
	GObject            parent;
	ofaBatCommonPrivate *private;
}
	ofaBatCommon;

typedef struct {
	/*< private >*/
	GObjectClass parent;
}
	ofaBatCommonClass;

/**
 * ofaBatCommonCb:
 *
 * A callback to be triggered when a new row is selected, or a row is
 * activated.
 *
 * Passed parameters are:
 * - #ofoBat object
 * - user_data provided at init_dialog() time
 */
typedef void ( *ofaBatCommonCb )( const ofoBat *, gpointer );

/**
 * ofaBatCommonParms
 *
 * The structure passed to the init_dialog() function.
 *
 * @container: the parent container of the target view
 * @dossier: the currently opened ofoDossier
 * @with_tree_view: whether we have to manage the tree view, or not
 *  (see e.g. ofaBatSelect vs. ofaBatProperties classes)
 * @editable: whether user is allowed to edit the notes
 * @pfnSelection: [allow-none]: a user-provided callback which will be
 *  triggered on each selection change
 * @pfnActivation: [allow-none]: a user-provided callback which will be
 *  triggered on row activation (Enter key or double-clic)
 * @user_data: user-data passed to the callback(s)
 */
typedef struct {
	GtkContainer     *container;
	ofoDossier       *dossier;
	gboolean          with_tree_view;
	gboolean          editable;
	ofaBatCommonCb      pfnSelection;
	ofaBatCommonCb      pfnActivation;
	gpointer          user_data;
}
	ofaBatCommonParms;

GType         ofa_bat_common_get_type     ( void ) G_GNUC_CONST;

ofaBatCommon   *ofa_bat_common_init_dialog  ( const ofaBatCommonParms *parms );

void          ofa_bat_common_set_bat      ( const ofaBatCommon *self, const ofoBat *bat );

const ofoBat *ofa_bat_common_get_selection( const ofaBatCommon *self );

G_END_DECLS

#endif /* __OFA_BAT_COMMON_H__ */
