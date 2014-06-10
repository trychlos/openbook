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

#ifndef __OFO_SGBD_H__
#define __OFO_SGBD_H__

/**
 * SECTION: ofo_sgbd
 * @short_description: An object which handles the SGBD connexion
 * @include: ui/ofo-sgbd.h
 */

#include "ui/ofo-sgbd-def.h"

G_BEGIN_DECLS

/**
 * Known SGBD providers
 */
#define SGBD_PROVIDER_MYSQL   "MySQL"

GType    ofo_sgbd_get_type    ( void ) G_GNUC_CONST;

ofoSgbd *ofo_sgbd_new         ( const gchar *provider );

gboolean ofo_sgbd_connect     ( ofoSgbd *sgbd,
									const gchar *host, gint port, const gchar *socket,
									const gchar *dbname,
									const gchar *account, const gchar *password );

gboolean ofo_sgbd_query       ( const ofoSgbd *sgbd, const gchar *query );

gboolean ofo_sgbd_query_ignore( const ofoSgbd *sgbd, const gchar *query );

GSList  *ofo_sgbd_query_ex    ( const ofoSgbd *sgbd, const gchar *query );

void     ofo_sgbd_free_result ( GSList *result );

G_END_DECLS

#endif /* __OFO_SGBD_H__ */
