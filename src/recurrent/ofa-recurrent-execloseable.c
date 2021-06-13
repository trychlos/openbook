/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2020 Pierre Wieser (see AUTHORS)
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

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-iexe-closer.h"
#include "api/ofa-igetter.h"

#include "ofa-recurrent-execloseable.h"
#include "ofo-recurrent-run.h"

/* a dedicated structure to hold needed datas
 */
typedef struct {

	/* initialization
	 */
	const ofaIExeCloseable *instance;
	ofaIGetter             *getter;
	const ofaIDBConnect    *connect;

	/* progression bar
	 */
	GtkWidget              *bar;
	gulong                  total;
	gulong                  current;
}
	sUpdate;

static guint    iexe_closeable_get_interface_version( void );
static gchar   *iexe_closeable_add_row( ofaIExeCloseable *instance, ofaIExeCloser *closer, guint rowtype );
static gboolean iexe_closeable_do_task( ofaIExeCloseable *instance, ofaIExeCloser *closer, guint rowtype, GtkWidget *box, ofaIGetter *getter );
static gboolean do_task_opening( ofaIExeCloseable *instance, ofaIExeCloser *closer, GtkWidget *box, ofaIGetter *getter );

/*
 * #ofaIExeCloseable interface setup
 */
void
ofa_recurrent_execloseable_iface_init( ofaIExeCloseableInterface *iface )
{
	static const gchar *thisfn = "ofa_recurrent_execloseable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexe_closeable_get_interface_version;
	iface->add_row = iexe_closeable_add_row;
	iface->do_task = iexe_closeable_do_task;
}

/*
 * the version of the #ofaIExeCloseable interface implemented by the module
 */
static guint
iexe_closeable_get_interface_version( void )
{
	return( 2 );
}

static gchar *
iexe_closeable_add_row( ofaIExeCloseable *instance, ofaIExeCloser *closer, guint rowtype )
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
iexe_closeable_do_task( ofaIExeCloseable *instance, ofaIExeCloser *closer, guint rowtype, GtkWidget *box, ofaIGetter *getter )
{
	gboolean ok;

	ok = FALSE;

	switch( rowtype ){
		case EXECLOSE_OPENING:
			ok = do_task_opening( instance, closer, box, getter );
			break;
		default:
			break;;
	}

	return( ok );
}

/*
 * Archive the cancelled and validated operations records, keeping
 * the pushed ones, to the ARCHIVE_T_REC_RUN table
 *
 * #1545 - the GtkBox may hosts a myProgressBar, but we prefer just a 'Done' label.
 */
static gboolean
do_task_opening( ofaIExeCloseable *instance, ofaIExeCloser *closer, GtkWidget *box, ofaIGetter *getter )
{
	gboolean ok;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query, *where, *str;
	const gchar *dbms_status;
	const GDate *prev_end;
	GtkWidget *label;

	label = gtk_label_new( "" );
	gtk_container_add( GTK_CONTAINER( box ), label );
	gtk_widget_show_all( box );

	ok = TRUE;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );

	/* cleanup obsolete tables
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHREC_T_DELETED_RECORDS" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	/* archive records
	 * note that we archive records as part of the opening exercice so that these
	 * archived data are kept in the newly opened database
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_REC_RUN" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	dbms_status = ofo_recurrent_run_status_get_dbms( REC_STATUS_WAITING );
	prev_end = ofa_iexe_closer_get_prev_end_date( closer );
	where = g_strdup_printf( "WHERE REC_STATUS!='%s'", dbms_status );
	if( prev_end ){
		str = g_strdup_printf( "%s AND REC_DATE<='%s'", where, my_date_to_str( prev_end, MY_DATE_SQL ));
		g_free( where );
		where = str;
	}

	if( ok ){
		query = g_strdup_printf( "CREATE TABLE ARCHIVE_T_REC_RUN "
					"SELECT * FROM REC_T_RUN %s", where );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	if( ok ){
		query = g_strdup_printf( "DELETE FROM REC_T_RUN %s", where );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	g_free( where );

	gtk_label_set_text( GTK_LABEL( label ), ok ? _( "Done" ) : _( "Error" ));

	return( ok );
}
