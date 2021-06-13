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
#include "api/ofo-dossier.h"

#include "tva/ofa-tva-execloseable.h"
#include "tva/ofo-tva-record.h"

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
static gboolean do_task_closing( ofaIExeCloseable *instance, GtkWidget *box, ofaIGetter *getter );
static gboolean do_task_opening( ofaIExeCloseable *instance, GtkWidget *box, ofaIGetter *getter );

/*
 * #ofaIExeCloseable interface setup
 */
void
ofa_tva_execloseable_iface_init( ofaIExeCloseableInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_execloseable_iface_init";

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
		case EXECLOSE_CLOSING:
			text = g_strdup( _( "Validating remaining VAT declarations :" ));
			break;
		case EXECLOSE_OPENING:
			text = g_strdup( _( "VAT tasks on N+1 period opening :" ));
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

	switch( rowtype ){
		case EXECLOSE_CLOSING:
			ok = do_task_closing( instance, box, getter );
			break;
		case EXECLOSE_OPENING:
			ok = do_task_opening( instance, box, getter );
			break;
		default:
			g_return_val_if_reached( FALSE );
	}

	return( ok );
}

/*
 * Before closing a period, have to validate the VAT declarations which
 * end until the closing date
 */
static gboolean
do_task_closing( ofaIExeCloseable *instance, GtkWidget *box, ofaIGetter *getter )
{
	ofaHub *hub;
	ofoDossier *dossier;
	const GDate *end_date;
	guint count;
	gchar *str;
	GtkWidget *label;

	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	end_date = ofo_dossier_get_exe_end( dossier );

	count = ofo_tva_record_validate_all( getter, end_date );

	if( count == 0 ){
		str = g_strdup_printf( _( "Nothing to do" ));
	} else if( count == 1 ){
		str = g_strdup_printf( _( "Done: one VAT declaration remained unvalidated" ));
	} else {
		str = g_strdup_printf( _( "Done: %u VAT declarations remained unvalidated" ), count );
	}

	label = gtk_label_new( str );
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_container_add( GTK_CONTAINER( box ), label );
	gtk_widget_show_all( box );

	g_free( str );

	return( TRUE );
}

/*
 * Archive the validated VAT declaration records which end on the
 * previous exercice.
 *
 * The identifier of the deleted records are stored in
 * ARCHTVA_T_DELETED_RECORDS table
 */
static gboolean
do_task_opening( ofaIExeCloseable *instance, GtkWidget *box, ofaIGetter *getter )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	gchar *query;
	ofoDossier *dossier;
	ofaHub *hub;
	gchar *sbegin;
	GtkWidget *label;

	label = gtk_label_new( "" );
	gtk_container_add( GTK_CONTAINER( box ), label );
	gtk_widget_show_all( box );

	ok = TRUE;
	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	dossier = ofa_hub_get_dossier( hub );
	sbegin = my_date_to_str( ofo_dossier_get_exe_begin( dossier ), MY_DATE_SQL );

	/* cleanup obsolete tables
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHTVA_T_DELETED_RECORDS" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	/* archive records
	 * keep all record of the new exercice
	 * + records which not have been validated (but there should be none)
	 */
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_TVA_KEEP_RECORDS" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup_printf(
					"CREATE TABLE ARCHIVE_T_TVA_KEEP_RECORDS "
					"	SELECT TFO_MNEMO,TFO_END FROM TVA_T_RECORDS "
					"		WHERE TFO_END>='%s'"
					"		OR TFO_STATUS='%s'",
								sbegin, ofo_tva_record_status_get_dbms( VAT_STATUS_NO ));
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_TVA_RECORDS" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_TVA_RECORDS "
					"SELECT * FROM TVA_T_RECORDS a "
					"	WHERE NOT EXISTS"
					"		( SELECT 1 FROM ARCHIVE_T_TVA_KEEP_RECORDS b "
					"			WHERE a.TFO_MNEMO=b.TFO_MNEMO "
					"			AND a.TFO_END=b.TFO_END)" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_TVA_RECORDS_BOOL" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_TVA_RECORDS_BOOL "
					"SELECT * FROM TVA_T_RECORDS_BOOL a "
					"	WHERE NOT EXISTS"
					"		( SELECT 1 FROM ARCHIVE_T_TVA_KEEP_RECORDS b "
					"			WHERE a.TFO_MNEMO=b.TFO_MNEMO "
					"			AND a.TFO_END=b.TFO_END)" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHIVE_T_TVA_RECORDS_DET" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHIVE_T_TVA_RECORDS_DET "
					"SELECT * FROM TVA_T_RECORDS_DET a "
					"	WHERE NOT EXISTS"
					"		( SELECT 1 FROM ARCHIVE_T_TVA_KEEP_RECORDS b "
					"			WHERE a.TFO_MNEMO=b.TFO_MNEMO "
					"			AND a.TFO_END=b.TFO_END)" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}
	if( ok ){
		query = g_strdup( "DELETE a,b,c FROM TVA_T_RECORDS a, TVA_T_RECORDS_BOOL b, TVA_T_RECORDS_DET c "
					"	WHERE a.TFO_MNEMO=b.TFO_MNEMO AND b.TFO_MNEMO=c.TFO_MNEMO "
					"		AND a.TFO_END=b.TFO_END AND b.TFO_END=c.TFO_END"
					"		AND NOT EXISTS"
					"		( SELECT 1 FROM ARCHIVE_T_TVA_KEEP_RECORDS d "
					"			WHERE a.TFO_MNEMO=d.TFO_MNEMO "
					"			AND a.TFO_END=d.TFO_END)" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
	}

	gtk_label_set_text( GTK_LABEL( label ), ok ? _( "Done" ) : _( "Error" ));

	return( ok );
}
