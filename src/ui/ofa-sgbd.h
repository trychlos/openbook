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

#ifndef __OFA_SGBD_H__
#define __OFA_SGBD_H__

/**
 * SECTION: ofa_sgbd
 * @short_description: An object which handles the SGBD connexion
 * @include: ui/ofa-sgbd.h
 */

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OFA_TYPE_SGBD                ( ofa_sgbd_get_type())
#define OFA_SGBD( object )           ( G_TYPE_CHECK_INSTANCE_CAST( object, OFA_TYPE_SGBD, ofaSgbd ))
#define OFA_SGBD_CLASS( klass )      ( G_TYPE_CHECK_CLASS_CAST( klass, OFA_TYPE_SGBD, ofaSgbdClass ))
#define OFA_IS_SGBD( object )        ( G_TYPE_CHECK_INSTANCE_TYPE( object, OFA_TYPE_SGBD ))
#define OFA_IS_SGBD_CLASS( klass )   ( G_TYPE_CHECK_CLASS_TYPE(( klass ), OFA_TYPE_SGBD ))
#define OFA_SGBD_GET_CLASS( object ) ( G_TYPE_INSTANCE_GET_CLASS(( object ), OFA_TYPE_SGBD, ofaSgbdClass ))

typedef struct _ofaSgbdPrivate       ofaSgbdPrivate;

typedef struct {
	/*< private >*/
	GObject         parent;
	ofaSgbdPrivate *private;
}
	ofaSgbd;

typedef struct _ofaSgbdClassPrivate  ofaSgbdClassPrivate;

typedef struct {
	/*< private >*/
	GObjectClass         parent;
	ofaSgbdClassPrivate *private;
}
	ofaSgbdClass;

/**
 * Known SGBD providers
 */
#define SGBD_PROVIDER_MYSQL          "MySQL"

GType    ofa_sgbd_get_type( void );

ofaSgbd *ofa_sgbd_new     ( const gchar *provider );

gboolean ofa_sgbd_connect ( ofaSgbd *sgbd, GtkWindow *parent, const gchar *host, gint port, const gchar *socket, const gchar *dbname, const gchar *account, const gchar *password );

gboolean ofa_sgbd_query   ( ofaSgbd *sgbd, GtkWindow *parent, const gchar *query );

GSList  *ofa_sgbd_query_ex( ofaSgbd *sgbd, GtkWindow *parent, const gchar *query );

G_END_DECLS

#endif /* __OFA_SGBD_H__ */
