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
#include <gtk/gtk.h>
#include <stdlib.h>

#include "my/my-date.h"
#include "my/my-isettings.h"
#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbdossier-meta.h"
#include "api/ofa-idbexercice-meta.h"
#include "api/ofa-igetter.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-backup.h"

/* private instance data
 */
typedef struct {
	gboolean             dispose_has_run;

	/* initialization
	 */
	ofaIGetter          *getter;
	GtkWindow           *parent;

	/* UI
	 */
	GtkWidget           *dialog;

	/* runtime
	 */
	ofaHub              *hub;
	const ofaIDBConnect *connect;			/* its user connection */
	ofaIDBDossierMeta   *dossier_meta;		/* its meta datas */
}
	ofaBackupPrivate;

static const gchar *st_dialog_name      = "BackupDlg";
static const gchar *st_backup_folder    = "ofa-LastBackupFolder";

static void       init_dialog( ofaBackup *self );
static gchar     *get_default_name( ofaBackup *self );
static gboolean   do_backup( ofaBackup *self );

G_DEFINE_TYPE_EXTENDED( ofaBackup, ofa_backup, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaBackup ))

static void
backup_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_backup_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BACKUP( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_parent_class )->finalize( instance );
}

static void
backup_dispose( GObject *instance )
{
	ofaBackupPrivate *priv;
	myISettings *settings;

	g_return_if_fail( instance && OFA_IS_BACKUP( instance ));

	priv = ofa_backup_get_instance_private( OFA_BACKUP( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		settings = ofa_hub_get_user_settings( priv->hub );
		my_utils_window_position_save( GTK_WINDOW( priv->dialog ), settings, st_dialog_name );

		gtk_widget_destroy( priv->dialog );

		g_clear_object( &priv->dossier_meta );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_parent_class )->dispose( instance );
}

static void
ofa_backup_init( ofaBackup *self )
{
	static const gchar *thisfn = "ofa_backup_init";
	ofaBackupPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BACKUP( self ));

	priv = ofa_backup_get_instance_private( self );

	priv->dispose_has_run = FALSE;
}

static void
ofa_backup_class_init( ofaBackupClass *klass )
{
	static const gchar *thisfn = "ofa_backup_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = backup_dispose;
	G_OBJECT_CLASS( klass )->finalize = backup_finalize;
}

/**
 * ofa_backup_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 *
 * Backup a dossier.
 */
void
ofa_backup_run( ofaIGetter *getter, GtkWindow *parent )
{
	static const gchar *thisfn = "ofa_backup_run";
	ofaBackup *self;
	ofaBackupPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p", thisfn, ( void * ) getter, ( void * ) parent );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));

	self = g_object_new( OFA_TYPE_BACKUP, NULL );

	priv = ofa_backup_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;

	init_dialog( self );

	if( gtk_dialog_run( GTK_DIALOG( priv->dialog )) == GTK_RESPONSE_OK ){
		do_backup( self );
	}

	g_object_unref( self );
}

static void
init_dialog( ofaBackup *self )
{
	ofaBackupPrivate *priv;
	gchar *last_folder, *def_name;
	myISettings *settings;
	const gchar *group;

	priv = ofa_backup_get_instance_private( self );

	priv->hub = ofa_igetter_get_hub( priv->getter );
	g_return_if_fail( priv->hub && OFA_IS_HUB( priv->hub ));

	priv->connect = ofa_hub_get_connect( priv->hub );
	priv->dossier_meta = g_object_ref( ofa_idbconnect_get_dossier_meta( priv->connect ));

	priv->dialog = gtk_file_chooser_dialog_new(
							_( "Backup the dossier" ),
							priv->parent,
							GTK_FILE_CHOOSER_ACTION_SAVE,
							_( "_Cancel" ), GTK_RESPONSE_CANCEL,
							_( "_Save" ), GTK_RESPONSE_OK,
							NULL );

	settings = ofa_hub_get_user_settings( priv->hub );
	my_utils_window_position_restore( GTK_WINDOW( priv->dialog ), settings, st_dialog_name );

	gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER( priv->dialog ), TRUE );

	def_name = get_default_name( self );
	gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->dialog ), def_name );
	g_free( def_name );

	settings = ofa_hub_get_dossier_settings( priv->hub );
	group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );
	last_folder = my_isettings_get_string( settings, group, st_backup_folder );
	if( my_strlen( last_folder )){
		gtk_file_chooser_set_current_folder_uri( GTK_FILE_CHOOSER( priv->dialog ), last_folder );
	}
	g_free( last_folder );
}

static gchar *
get_default_name( ofaBackup *self )
{
	ofaBackupPrivate *priv;
	ofaIDBExerciceMeta *exercice_meta;
	GRegex *regex;
	gchar *name, *fname, *sdate, *result;
	GDate date;

	priv = ofa_backup_get_instance_private( self );

	/* get name without spaces */
	exercice_meta = ofa_idbconnect_get_exercice_meta( priv->connect );
	name = ofa_idbexercice_meta_get_name( exercice_meta );

	regex = g_regex_new( " ", 0, 0, NULL );
	fname = g_regex_replace_literal( regex, name, -1, 0, "", 0, NULL );

	g_free( name );

	my_date_set_now( &date );
	sdate = my_date_to_str( &date, MY_DATE_YYMD );
	result = g_strdup_printf( "%s-%s.gz", fname, sdate );
	g_free( sdate );

	g_free( fname );

	return( result );
}

static gboolean
do_backup( ofaBackup *self )
{
	ofaBackupPrivate *priv;
	gchar *folder, *uri;
	gboolean ok;
	myISettings *settings;
	const gchar *group;
	GtkWindow *toplevel;

	priv = ofa_backup_get_instance_private( self );

	/* folder is null while the user has not make it the current folder
	 * by entering into the folder */
	/*folder = gtk_file_chooser_get_current_folder_uri( GTK_FILE_CHOOSER( priv->dialog ));*/
	uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( priv->dialog ));

	folder = g_path_get_dirname( uri );
	settings = ofa_hub_get_dossier_settings( priv->hub );
	group = ofa_idbdossier_meta_get_settings_group( priv->dossier_meta );
	my_isettings_set_string( settings, group, st_backup_folder, folder );
	g_free( folder );

	if( 1 ){
		toplevel = my_utils_widget_get_toplevel( priv->dialog );
		ok = ofa_idbconnect_backup_db( priv->connect, NULL, uri, toplevel );
	} else {
		ok = ofa_idbconnect_backup( priv->connect, uri );
	}

	g_free( uri );

	return( ok );
}
