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

#ifndef __OFA_DOSSIER_NEW_BIN_H__
#define __OFA_DOSSIER_NEW_BIN_H__

/**
 * SECTION: ofa_dossier_new_bin
 * @short_description: #ofaDossierNewBin class definition.
 * @include: core/ofa-dossier-new-bin.h
 *
 * Let the user define a new dossier, selecting the DBMS provider and
 * its connection properties, registering it in the settings.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_DOSSIER_NEW_BIN                ( ofa_dossier_new_bin_get_type())
#define OFA_DOSSIER_NEW_BIN( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBin ))
#define OFA_DOSSIER_NEW_BIN_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBinClass ))
#define OFA_IS_DOSSIER_NEW_BIN( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_DOSSIER_NEW_BIN ))
#define OFA_IS_DOSSIER_NEW_BIN_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_DOSSIER_NEW_BIN ))
#define OFA_DOSSIER_NEW_BIN_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_DOSSIER_NEW_BIN, ofaDossierNewBinClass ))

typedef struct _ofaDossierNewBinPrivate         ofaDossierNewBinPrivate;

typedef struct {
	/*< public members >*/
	GtkBin                   parent;

	/*< private members >*/
	ofaDossierNewBinPrivate *priv;
}
	ofaDossierNewBin;

typedef struct {
	/*< public members >*/
	GtkBinClass              parent;
}
	ofaDossierNewBinClass;

GType             ofa_dossier_new_bin_get_type       ( void ) G_GNUC_CONST;

ofaDossierNewBin *ofa_dossier_new_bin_new            ( void );

void              ofa_dossier_new_bin_attach_to      ( ofaDossierNewBin *piece,
																	GtkContainer *parent,
																	GtkSizeGroup *group );

void              ofa_dossier_new_bin_set_frame      ( ofaDossierNewBin *piece,
																	gboolean visible );

void              ofa_dossier_new_bin_set_provider   ( ofaDossierNewBin *piece,
																	const gchar *provider );

gboolean          ofa_dossier_new_bin_is_valid       ( const ofaDossierNewBin *piece );

gboolean          ofa_dossier_new_bin_apply          ( const ofaDossierNewBin *piece );

void              ofa_dossier_new_bin_get_dname      ( const ofaDossierNewBin *piece,
																	gchar **dname );

void              ofa_dossier_new_bin_get_database   ( const ofaDossierNewBin *piece,
																	gchar **database );

void              ofa_dossier_new_bin_get_credentials( const ofaDossierNewBin *piece,
																	gchar **account,
																	gchar **password );

G_END_DECLS

#endif /* __OFA_DOSSIER_NEW_BIN_H__ */
