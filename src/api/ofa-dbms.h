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
 *
 * $Id$
 */

#ifndef __OFA_DBMS_H__
#define __OFA_DBMS_H__

/**
 * SECTION: ofa_dbms
 * @short_description: An object which handles the DBMS connexion
 * @include: api/ofa-dbms.h
 */

#include "api/ofa-dbms-def.h"
#include "api/ofa-idbms.h"

G_BEGIN_DECLS

ofaDbms  *ofa_dbms_new                 ( void );

gboolean  ofa_dbms_connect             ( ofaDbms *dbms,
												const gchar *dname, const gchar *dbname,
												const gchar *account, const gchar *password,
												gboolean display_error );

gboolean  ofa_dbms_query               ( const ofaDbms *dbms,
												const gchar *query,
												gboolean display_error );

gboolean  ofa_dbms_query_ex            ( const ofaDbms *dbms,
												const gchar *query,
												GSList **result,
												gboolean display_error );

gboolean  ofa_dbms_query_int           ( const ofaDbms *dbms,
												const gchar *query,
												gint *ivalue,
												gboolean display_error );

#define   ofa_dbms_free_results(R)     g_debug( "ofa_dbms_free_results" ); \
												g_slist_foreach(( R ), ( GFunc ) g_slist_free_full, g_free ); \
												g_slist_free( R )

gboolean  ofa_dbms_backup              ( const ofaDbms *dbms,
												const gchar *fname,
												gboolean verbose );

G_END_DECLS

#endif /* __OFA_DBMS_H__ */
