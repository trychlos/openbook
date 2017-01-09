/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "my/my-progress-bar.h"
#include "my/my-utils.h"

#include "api/ofa-idbconnect.h"

#include "ofa-recurrent-execlose.h"

/* a dedicated structure to hold needed datas
 */
typedef struct {

	/* initialization
	 */
	const ofaIExeClose *instance;
	ofaHub                  *hub;
	const ofaIDBConnect     *connect;

	/* progression bar
	 */
	GtkWidget               *bar;
	gulong                   total;
	gulong                   current;
}
	sUpdate;

static guint    iexe_close_get_interface_version( void );
static gchar   *iexe_close_add_row( ofaIExeClose *instance, guint rowtype );
static gboolean iexe_close_do_task( ofaIExeClose *instance, guint rowtype, GtkWidget *box, ofaHub *hub );
static gboolean do_task_opening( ofaIExeClose *instance, GtkWidget *box, ofaHub *hub );
static void     update_bar( myProgressBar *bar, guint *count, guint total );

/*
 * #ofaIExeClose interface setup
 */
void
ofa_recurrent_execlose_iface_init( ofaIExeCloseInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_execlose_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexe_close_get_interface_version;
	iface->add_row = iexe_close_add_row;
	iface->do_task = iexe_close_do_task;
}

/*
 * the version of the #ofaIExeClose interface implemented by the module
 */
static guint
iexe_close_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iexe_close_add_row( ofaIExeClose *instance, guint rowtype )
{
	gchar *text;

	text = NULL;

	switch( rowtype ){
		case EXECLOSE_OPENING:
			text = g_strdup( _( "Recurrent tasks on N+1 period opening :" ));
			break;
		default:
			break;
	}

	return( text );
}

static gboolean
iexe_close_do_task( ofaIExeClose *instance, guint rowtype, GtkWidget *box, ofaHub *hub )
{
	gboolean ok;

	ok = FALSE;

	switch( rowtype ){
		case EXECLOSE_OPENING:
			ok = do_task_opening( instance, box, hub );
			break;
		default:
			break;;
	}

	return( ok );
}

/*
 * archive the cancelled and validated operations records, keeping
 * the pushed ones, to the ARCHREC_T_DELETED_RECORDS table
 */
static gboolean
do_task_opening( ofaIExeClose *instance, GtkWidget *box, ofaHub *hub )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	gchar *query;
	myProgressBar *bar;
	guint count, total;

	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( box ), GTK_WIDGET( bar ));
	gtk_widget_show_all( box );

	total = 3;							/* queries count */
	count = 0;
	ok = TRUE;
	connect = ofa_hub_get_connect( hub );

	/* cleanup obsolete tables
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHREC_T_DELETED_RECORDS" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}

	/* archive records
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_REC_RUN" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_REC_RUN "
					"SELECT * FROM REC_T_RUN "
					"	WHERE REC_STATUS!='%s'" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}

	return( ok );
}

static void
update_bar( myProgressBar *bar, guint *count, guint total )
{
	gdouble progress;

	*count += 1;
	progress = ( gdouble ) *count / ( gdouble ) total;
	g_signal_emit_by_name( bar, "my-double", progress );
	g_signal_emit_by_name( bar, "my-text", NULL );			/* shows a percentage */
}
