/*
 * Open Freelance Accounting
 * A double-entry accounting application for freelances.
 *
 * Copyright (C) 2014,2015,2016 Pierre Wieser (see AUTHORS)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "api/my-progress-bar.h"
#include "api/my-utils.h"

#include "ofa-tva.h"
#include "ofa-tva-execlose.h"

/* a dedicated structure to hold needed datas
 */
typedef struct {

	/* initialization
	 */
	const ofaIExeCloseClose   *instance;
	ofaHub              *hub;
	const ofaIDBConnect *connect;

	/* progression bar
	 */
	GtkWidget           *bar;
	gulong               total;
	gulong               current;
}
	sUpdate;

static guint    iexeclose_close_get_interface_version( const ofaIExeCloseClose *instance );
static gchar   *iexeclose_close_add_row( ofaIExeCloseClose *instance, guint rowtype );
static gboolean iexeclose_close_do_task( ofaIExeCloseClose *instance, guint rowtype, GtkWidget *box, ofaHub *hub );
static gboolean do_task_closing( ofaIExeCloseClose *instance, GtkWidget *box, ofaHub *hub );
static gboolean do_task_opening( ofaIExeCloseClose *instance, GtkWidget *box, ofaHub *hub );
static void     update_bar( myProgressBar *bar, guint *count, guint total );

/*
 * #ofaIExeCloseClose interface setup
 */
void
ofa_tva_execlose_iface_init( ofaIExeCloseCloseInterface *iface )
{
	static const gchar *thisfn = "ofa_tva_dbmodel_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexeclose_close_get_interface_version;
	iface->add_row = iexeclose_close_add_row;
	iface->do_task = iexeclose_close_do_task;
}

/*
 * the version of the #ofaIExeCloseClose interface implemented by the module
 */
static guint
iexeclose_close_get_interface_version( const ofaIExeCloseClose *instance )
{
	return( 1 );
}

static gchar *
iexeclose_close_add_row( ofaIExeCloseClose *instance, guint rowtype )
{
	gchar *text;

	switch( rowtype ){
		case EXECLOSE_CLOSING:
			//text = g_strdup( _( "VAT task on closing exercice N :" ));
			text = NULL;
			break;
		case EXECLOSE_OPENING:
			text = g_strdup( _( "VAT tasks on N+1 period opening :" ));
			break;
		default:
			g_return_val_if_reached( NULL );
	}

	return( text );
}

static gboolean
iexeclose_close_do_task( ofaIExeCloseClose *instance, guint rowtype, GtkWidget *box, ofaHub *hub )
{
	gboolean ok;

	switch( rowtype ){
		case EXECLOSE_CLOSING:
			ok = do_task_closing( instance, box, hub );
			break;
		case EXECLOSE_OPENING:
			ok = do_task_opening( instance, box, hub );
			break;
		default:
			g_return_val_if_reached( FALSE );
	}

	return( ok );
}

/*
 * This task is expected not to be called since we are returning a
 * %NULL label from add_row() method
 */
static gboolean
do_task_closing( ofaIExeCloseClose *instance, GtkWidget *box, ofaHub *hub )
{
	GtkWidget *label;

	label = gtk_label_new( _( "Nothing to do" ));
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_container_add( GTK_CONTAINER( box ), label );
	gtk_widget_show_all( box );

	return( TRUE );
}

/*
 * archive the validated VAT declaration records
 * the identifier of the deleted records are stored in
 * ARCHTVA_T_DELETED_RECORDS table
 */
static gboolean
do_task_opening( ofaIExeCloseClose *instance, GtkWidget *box, ofaHub *hub )
{
	gboolean ok;
	const ofaIDBConnect *connect;
	gchar *query;
	myProgressBar *bar;
	guint count, total;

	bar = my_progress_bar_new();
	gtk_container_add( GTK_CONTAINER( box ), GTK_WIDGET( bar ));
	gtk_widget_show_all( box );

	total = 5;							/* queries count */
	count = 0;
	ok = TRUE;
	connect = ofa_hub_get_connect( hub );

	if( ok ){
		query = g_strdup( "DROP TABLE IF EXISTS ARCHTVA_T_DELETED_RECORDS" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}
	if( ok ){
		query = g_strdup( "CREATE TABLE ARCHTVA_T_DELETED_RECORDS "
					"SELECT TFO_MNEMO,TFO_LABEL,TFO_NOTES,TFO_BEGIN,"
					"TFO_END,TFO_VALIDATED FROM TVA_T_RECORDS "
					"	WHERE TFO_VALIDATED='Y'" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}
	if( ok ){
		query = g_strdup( "DELETE FROM TVA_T_RECORDS "
					"	WHERE TFO_VALIDATED='Y'" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}
	if( ok ){
		query = g_strdup( "DELETE FROM TVA_T_RECORDS_BOOL "
				"WHERE EXISTS( SELECT 1 FROM ARCHTVA_T_DELETED_RECORDS "
				"	WHERE TVA_T_RECORDS_BOOL.TFO_MNEMO=ARCHTVA_T_DELETED_RECORDS.TFO_MNEMO "
				"	AND TVA_T_RECORDS_BOOL.TFO_END=ARCHTVA_T_DELETED_RECORDS.TFO_END)" );
		ok = ofa_idbconnect_query( connect, query, TRUE );
		g_free( query );
		update_bar( bar, &count, total );
	}
	if( ok ){
		query = g_strdup( "DELETE FROM TVA_T_RECORDS_DET "
				"WHERE EXISTS( SELECT 1 FROM ARCHTVA_T_DELETED_RECORDS "
				"	WHERE TVA_T_RECORDS_DET.TFO_MNEMO=ARCHTVA_T_DELETED_RECORDS.TFO_MNEMO "
				"	AND TVA_T_RECORDS_DET.TFO_END=ARCHTVA_T_DELETED_RECORDS.TFO_END)" );
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
	g_signal_emit_by_name( bar, "ofa-double", progress );
	g_signal_emit_by_name( bar, "ofa-text", NULL );			/* shows a percentage */
}
