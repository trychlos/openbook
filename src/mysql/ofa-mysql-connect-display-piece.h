/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015 Pierre Wieser (see AUTHORS)
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
 */

#ifndef __OFA_MYSQL_CONNECT_DISPLAY_PIECE_H__
#define __OFA_MYSQL_CONNECT_DISPLAY_PIECE_H__

/**
 * SECTION: ofa_mysql_idbms
 * @short_description: #ofaMysql class definition.
 *
 * Display the connection informations read for the named dossier
 * from the settings.
 */

#include <gtk/gtk.h>

#include <api/ofa-idbms.h>

G_BEGIN_DECLS

void ofa_mysql_connect_display_piece_attach_to( const ofaIDbms *instance,
															const gchar *dname,
															GtkContainer *parent );

G_END_DECLS

#endif /* __OFA_MYSQL_CONNECT_DISPLAY_PIECE_H__ */
