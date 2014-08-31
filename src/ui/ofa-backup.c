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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "api/my-utils.h"
#include "api/ofa-settings.h"
#include "api/ofo-dossier.h"

#include "ui/ofa-backup.h"
#include "ui/ofa-main-window.h"

/* private instance data
 */
struct _ofaBackupPrivate {
	gboolean       dispose_has_run;

	ofaMainWindow *main_window;
	GtkWidget     *dialog;
	ofoDossier    *dossier;
};

static const gchar *st_dialog_name   = "BackupDlg";
static const gchar *st_backup_folder = "LastBackupFolder";

G_DEFINE_TYPE( ofaBackup, ofa_backup, G_TYPE_OBJECT )

static void       init_dialog( ofaBackup *self );
static gboolean   do_backup( ofaBackup *self );

static void
backup_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_backup_finalize";
	ofaBackupPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BACKUP( instance ));

	priv = OFA_BACKUP( instance )->private;

	/* free data members here */
	g_free( priv );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_parent_class )->finalize( instance );
}

static void
backup_dispose( GObject *instance )
{
	ofaBackupPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BACKUP( instance ));

	priv = OFA_BACKUP( instance )->private;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		my_utils_window_save_position( GTK_WINDOW( priv->dialog ), st_dialog_name );
		gtk_widget_destroy( priv->dialog );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_parent_class )->dispose( instance );
}

static void
ofa_backup_init( ofaBackup *self )
{
	static const gchar *thisfn = "ofa_backup_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BACKUP( self ));

	self->private = g_new0( ofaBackupPrivate, 1 );

	self->private->dispose_has_run = FALSE;
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
 * @main: the main window of the application.
 *
 * Update the properties of an dossier
 */
void
ofa_backup_run( ofaMainWindow *main_window )
{
	static const gchar *thisfn = "ofa_backup_run";
	ofaBackup *self;
	ofaBackupPrivate *priv;

	g_return_if_fail( OFA_IS_MAIN_WINDOW( main_window ));

	g_debug( "%s: main_window=%p", thisfn, ( void * ) main_window );

	self = g_object_new( OFA_TYPE_BACKUP, NULL );
	priv = self->private;
	priv->main_window = main_window;

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
	gchar *last_folder;

	priv = self->private;

	priv->dialog = gtk_file_chooser_dialog_new(
							_( "Backup the dossier" ),
							GTK_WINDOW( priv->main_window ),
							GTK_FILE_CHOOSER_ACTION_SAVE,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							GTK_STOCK_SAVE, GTK_RESPONSE_OK,
							NULL );

	my_utils_window_restore_position( GTK_WINDOW( priv->dialog ), st_dialog_name );

	gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER( priv->dialog ), TRUE );
	gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( priv->dialog ), _( "Untitled" ));

	priv->dossier = ofa_main_window_get_dossier( priv->main_window );

	last_folder = ofa_settings_get_dossier_key_string(
						ofo_dossier_get_name( priv->dossier ), st_backup_folder );
	if( last_folder && g_utf8_strlen( last_folder, -1 )){
		gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( priv->dialog ), last_folder );
	}
	g_free( last_folder );
}

static gboolean
do_backup( ofaBackup *self )
{
	ofaBackupPrivate *priv;
	gchar *folder;
	gchar *fname;
	gboolean ok;

	priv = self->private;

	folder = gtk_file_chooser_get_current_folder( GTK_FILE_CHOOSER( priv->dialog ));
	ofa_settings_set_dossier_key_string(
				ofo_dossier_get_name( priv->dossier ), st_backup_folder, folder );
	g_free( folder );

	fname = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( priv->dialog ));
	ok = ofo_dossier_backup( priv->dossier, fname );
	g_free( fname );

	return( ok );
}
