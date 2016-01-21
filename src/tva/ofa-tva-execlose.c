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
			text = g_strdup( _( "VAT tasks on opening exercice N+1 :" ));
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
 * archive the validated forms
 */
static gboolean
do_task_opening( ofaIExeCloseClose *instance, GtkWidget *box, ofaHub *hub )
{
	GtkWidget *label;

	label = gtk_label_new( _( "DO SOMETHING HERE" ));
	gtk_label_set_xalign( GTK_LABEL( label ), 0 );
	gtk_container_add( GTK_CONTAINER( box ), label );
	gtk_widget_show_all( box );

	return( TRUE );
}
