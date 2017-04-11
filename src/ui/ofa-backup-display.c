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

#include <archive.h>
#include <archive_entry.h>
#include <json-glib/json-glib.h>

#include "my/my-idialog.h"
#include "my/my-iwindow.h"
#include "my/my-utils.h"

#include "api/ofa-backup-header.h"
#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-backup-display.h"

/* private instance data
 */
typedef struct {
	gboolean     dispose_has_run;

	/* initialization
	 */
	ofaIGetter  *getter;
	GtkWindow   *parent;
	gchar       *uri;

	/* runtime
	 */
	GtkWindow   *actual_parent;

	/* UI
	 */
	GtkWidget   *book;
}
	ofaBackupDisplayPrivate;

static const gchar *st_resource_ui      = "/org/trychlos/openbook/ui/ofa-backup-display.ui";

static void iwindow_iface_init( myIWindowInterface *iface );
static void iwindow_init( myIWindow *instance );
static void idialog_iface_init( myIDialogInterface *iface );
static void idialog_init( myIDialog *instance );
static void read_archive( ofaBackupDisplay *self );
static void add_archive_header( ofaBackupDisplay *self, struct archive *archive, struct archive_entry *entry, const gchar *name );

G_DEFINE_TYPE_EXTENDED( ofaBackupDisplay, ofa_backup_display, GTK_TYPE_DIALOG, 0,
		G_ADD_PRIVATE( ofaBackupDisplay )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IWINDOW, iwindow_iface_init )
		G_IMPLEMENT_INTERFACE( MY_TYPE_IDIALOG, idialog_iface_init ))

static void
backup_display_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_backup_display_finalize";
	ofaBackupDisplayPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BACKUP_DISPLAY( instance ));

	/* free data members here */
	priv = ofa_backup_display_get_instance_private( OFA_BACKUP_DISPLAY( instance ));

	g_free( priv->uri );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_display_parent_class )->finalize( instance );
}

static void
backup_display_dispose( GObject *instance )
{
	ofaBackupDisplayPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BACKUP_DISPLAY( instance ));

	priv = ofa_backup_display_get_instance_private( OFA_BACKUP_DISPLAY( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_display_parent_class )->dispose( instance );
}

static void
ofa_backup_display_init( ofaBackupDisplay *self )
{
	static const gchar *thisfn = "ofa_backup_display_init";
	ofaBackupDisplayPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BACKUP_DISPLAY( self ));

	priv = ofa_backup_display_get_instance_private( self );

	priv->dispose_has_run = FALSE;

	gtk_widget_init_template( GTK_WIDGET( self ));
}

static void
ofa_backup_display_class_init( ofaBackupDisplayClass *klass )
{
	static const gchar *thisfn = "ofa_backup_display_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = backup_display_dispose;
	G_OBJECT_CLASS( klass )->finalize = backup_display_finalize;

	gtk_widget_class_set_template_from_resource( GTK_WIDGET_CLASS( klass ), st_resource_ui );
}

/**
 * ofa_backup_display_run:
 * @getter: a #ofaIGetter instance.
 * @parent: [allow-none]: the #GtkWindow parent.
 * @uri: the URI of the archive file.
 *
 * Display the metadata of the @uri archive file.
 */
void
ofa_backup_display_run( ofaIGetter *getter, GtkWindow *parent, const gchar *uri )
{
	static const gchar *thisfn = "ofa_backup_display_run";
	ofaBackupDisplay *self;
	ofaBackupDisplayPrivate *priv;

	g_debug( "%s: getter=%p, parent=%p, rui=%s",
			thisfn, ( void * ) getter, ( void * ) parent, uri );

	g_return_if_fail( getter && OFA_IS_IGETTER( getter ));
	g_return_if_fail( !parent || GTK_IS_WINDOW( parent ));
	g_return_if_fail( my_strlen( uri ) > 0 );

	self = g_object_new( OFA_TYPE_BACKUP_DISPLAY, NULL );

	priv = ofa_backup_display_get_instance_private( self );

	priv->getter = getter;
	priv->parent = parent;
	priv->uri = g_strdup( uri );

	/* run modal or non-modal depending of the parent */
	my_idialog_run_maybe_modal( MY_IDIALOG( self ));
}

/*
 * myIWindow interface management
 */
static void
iwindow_iface_init( myIWindowInterface *iface )
{
	static const gchar *thisfn = "ofa_backup_display_iwindow_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = iwindow_init;
}

static void
iwindow_init( myIWindow *instance )
{
	static const gchar *thisfn = "ofa_backup_display_iwindow_init";
	ofaBackupDisplayPrivate *priv;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_backup_display_get_instance_private( OFA_BACKUP_DISPLAY( instance ));

	priv->actual_parent = priv->parent ? priv->parent : GTK_WINDOW( ofa_igetter_get_main_window( priv->getter ));
	my_iwindow_set_parent( instance, priv->actual_parent );

	my_iwindow_set_geometry_settings( instance, ofa_igetter_get_user_settings( priv->getter ));
}

/*
 * myIDialog interface management
 */
static void
idialog_iface_init( myIDialogInterface *iface )
{
	static const gchar *thisfn = "ofa_backup_display_idialog_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->init = idialog_init;
}

static void
idialog_init( myIDialog *instance )
{
	static const gchar *thisfn = "ofa_backup_display_idialog_init";
	ofaBackupDisplayPrivate *priv;
	GtkWidget *box;

	g_debug( "%s: instance=%p", thisfn, ( void * ) instance );

	priv = ofa_backup_display_get_instance_private( OFA_BACKUP_DISPLAY( instance ));

	box = my_utils_container_get_child_by_name( GTK_CONTAINER( instance ), "top" );
	g_return_if_fail( box && GTK_IS_BOX( box ));

	priv->book = gtk_notebook_new();
	gtk_container_add( GTK_CONTAINER( box ), priv->book );

	read_archive( OFA_BACKUP_DISPLAY( instance ));
}

static void
read_archive( ofaBackupDisplay *self )
{
	static const gchar *thisfn = "ofa_backup_display_read_archive";
	ofaBackupDisplayPrivate *priv;
	GFile *file;
	gchar *pathname;
	struct archive *archive;
	struct archive_entry *entry;
	const gchar *cname;

	priv = ofa_backup_display_get_instance_private( self );

	file = g_file_new_for_uri( priv->uri );
	pathname = g_file_get_path( file );

	archive = archive_read_new();
	archive_read_support_filter_all( archive );
	archive_read_support_format_all( archive );

	if( archive_read_open_filename( archive, pathname, 16384 ) != ARCHIVE_OK ){
		g_warning( "%s: archive_read_open_filename: path=%s, %s", thisfn, pathname, archive_error_string( archive ));

	} else {
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
			cname = archive_entry_pathname( entry );
			if( g_str_has_prefix( cname, OFA_BACKUP_HEADER_HEADER )){
				add_archive_header( self, archive, entry, cname );
				continue;
			}
			archive_read_data_skip( archive );
		}
	}

	archive_read_close( archive );
	archive_read_free( archive );
	g_free( pathname );
	g_object_unref( file );
}

static void
add_archive_header( ofaBackupDisplay *self, struct archive *archive, struct archive_entry *entry, const gchar *name )
{
	static const gchar *thisfn = "ofa_backup_display_add_archive_header";
	static size_t bufsize = 8192;
	ofaBackupDisplayPrivate *priv;
	gchar *data, *str;
	la_ssize_t read;
	GString *gstr;
	GtkWidget *scrolled, *text, *label;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	JsonParser *parser;
	JsonGenerator *generator;
	GError *error;

	priv = ofa_backup_display_get_instance_private( self );

	gstr = g_string_new( "" );
	data = g_new0( gchar, bufsize );

	while( TRUE ){
		read = archive_read_data( archive, data, bufsize );
		/* end of file */
		if( read == 0 ){
			break;
		}
		/* error */
		if( read < 0 ){
			g_warning( "%s: archive_read_data: %s", thisfn, archive_error_string( archive ));
			break;
		}
		/* something has been read */
		gstr = g_string_append( gstr, data );
	}

	/* we have got a json string */
	if( gstr->len > 0 ){
		error = NULL;
		str = NULL;
		parser = json_parser_new();

		if( !json_parser_load_from_data( parser, gstr->str, -1, &error )){
			g_warning( "%s: json_parser_load_from_data: %s", thisfn, error->message );
			g_error_free( error );

		} else {
			generator = json_generator_new();
			json_generator_set_root( generator, json_parser_get_root( parser ));
			json_generator_set_pretty( generator, TRUE );
			str = json_generator_to_data( generator, NULL );
			g_object_unref( generator );
		}

		if( my_strlen( str )){
			scrolled = gtk_scrolled_window_new( NULL, NULL );
			text = gtk_text_view_new();
			gtk_widget_set_vexpand( text, TRUE );
			gtk_container_add( GTK_CONTAINER( scrolled ), text );
			buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( text ));
			gtk_text_buffer_get_iter_at_line( buffer, &iter, 0 );
			gtk_text_buffer_insert( buffer, &iter, str, -1 );
			label = gtk_label_new( name+my_strlen( OFA_BACKUP_HEADER_HEADER ));
			gtk_notebook_append_page( GTK_NOTEBOOK( priv->book ), scrolled, label );
		}

		g_object_unref( parser );
		g_free( str );
	}

	g_free( data );
	g_string_free( gstr, TRUE );
}
