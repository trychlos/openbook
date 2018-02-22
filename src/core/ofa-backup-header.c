/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014-2018 Pierre Wieser (see AUTHORS)
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

#include <archive_entry.h>

#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-backup-header.h"
#include "api/ofa-backup-props.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-igetter.h"
#include "api/ofa-ijson.h"
#include "api/ofa-openbook-props.h"
#include "api/ofo-dossier.h"

static gboolean write_backup_props_header( struct archive *archive, struct archive_entry *entry, ofaIGetter *getter, const gchar *comment );
static gboolean write_dossier_props_header( struct archive *archive, struct archive_entry *entry, ofaIGetter *getter, const gchar *comment );
static gboolean write_openbook_props_header( struct archive *archive, struct archive_entry *entry, ofaIGetter *getter, const gchar *comment );
static gboolean write_header( struct archive *archive, struct archive_entry *entry, const gchar *title, const gchar *json );

/**
 * ofa_backup_header_write_headers:
 * @getter: a #ofaIGetter instance.
 * @comment: a user comment.
 * @archive: an archive file opened in write mode.
 *
 * Writes currently opened dossier, Openbook software and this backup
 * properties as headers in the @archive file.
 *
 * Returns: %TRUE if all the headers have been successfully written.
 */
gboolean
ofa_backup_header_write_headers( ofaIGetter *getter, const gchar *comment, struct archive *archive )
{
	static const gchar *thisfn = "ofa_backup_header_write_headers";
	struct archive_entry *entry;
	gboolean ok;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), FALSE );

	entry = archive_entry_new();
	if( !entry ){
		g_warning( "%s: unable to allocate new archive_entry object", thisfn );
		return( FALSE );
	}

	/* order is (must be) unimportant and unrelevant
	 * just for convenience, write the headers in alphabetical order
	 */
	ok = write_backup_props_header( archive, entry, getter, comment ) &&
			write_dossier_props_header( archive, entry, getter, comment ) &&
			write_openbook_props_header( archive, entry, getter, comment );

	archive_entry_free( entry );

	return( ok );
}

static gboolean
write_backup_props_header( struct archive *archive, struct archive_entry *entry, ofaIGetter *getter, const gchar *comment )
{
	ofaBackupProps *props;
	const ofaIDBConnect *connect;
	gchar *json, *title;
	gboolean ok;
	ofaHub *hub;

	hub = ofa_igetter_get_hub( getter );
	connect = ofa_hub_get_connect( hub );
	props = ofa_backup_props_new();
	ofa_backup_props_set_comment( props, comment);
	ofa_backup_props_set_userid( props, ofa_idbconnect_get_account( connect ));

	title = ofa_ijson_get_title( G_OBJECT_TYPE( props ));
	json = ofa_ijson_get_as_string( OFA_IJSON( props ));

	ok = write_header( archive, entry, title, json );

	g_free( json );
	g_free( title );
	g_object_unref( props );

	return( ok );
}

static gboolean
write_dossier_props_header( struct archive *archive, struct archive_entry *entry, ofaIGetter *getter, const gchar *comment )
{
	ofaDossierProps *props;
	ofoDossier *dossier;
	gchar *json, *title;
	gboolean ok;
	ofaHub *hub;

	hub = ofa_igetter_get_hub( getter );
	dossier = ofa_hub_get_dossier( hub );
	props = ofa_dossier_props_new_from_dossier( dossier );

	title = ofa_ijson_get_title( G_OBJECT_TYPE( props ));
	json = ofa_ijson_get_as_string( OFA_IJSON( props ));

	ok = write_header( archive, entry, title, json );

	g_free( json );
	g_free( title );
	g_object_unref( props );

	return( ok );
}

static gboolean
write_openbook_props_header( struct archive *archive, struct archive_entry *entry, ofaIGetter *getter, const gchar *comment )
{
	const ofaOpenbookProps *props;
	gchar *json, *title;
	gboolean ok;

	props = ofa_igetter_get_openbook_props( getter );

	title = ofa_ijson_get_title( G_OBJECT_TYPE( props ));
	json = ofa_ijson_get_as_string( OFA_IJSON( props ));

	ok = write_header( archive, entry, title, json );

	g_free( json );
	g_free( title );

	return( ok );
}

static gboolean
write_header( struct archive *archive, struct archive_entry *entry, const gchar *title, const gchar *json )
{
	static const gchar *thisfn = "ofa_backup_header_write_header";
	gchar *header_title;
	GTimeVal stamp;
	glong written;

	archive_entry_clear( entry );

	header_title = g_strdup_printf( "%s%s", OFA_BACKUP_HEADER_HEADER, title );
    archive_entry_set_pathname( entry, header_title );
    g_free( header_title );

    archive_entry_set_filetype( entry, AE_IFREG );
    archive_entry_set_perm( entry, 0644 );
	my_stamp_set_now( &stamp );
    archive_entry_set_mtime( entry, stamp.tv_sec, 0 );

    if( archive_write_header( archive, entry) != ARCHIVE_OK ){
    	g_warning( "%s: archive_write_header: %s", thisfn, archive_error_string( archive ));
    	archive_entry_free( entry );
    	return( FALSE );
    }

	written = archive_write_data( archive, json, my_strlen( json ));
	archive_write_finish_entry( archive );

	if( written < 0 ){
		g_warning( "%s: archive_write_data: %s", thisfn, archive_error_string( archive ));
	} else {
		g_debug( "%s: json=%s, written=%lu", thisfn, json, written );
    }

	return( written > 0 );
}

/**
 * ofa_backup_header_read_header:
 * @archive: an archive file opened in read mode.
 * @name: the (unprefixed) name of the searched header.
 *
 * Returns: a header as a newly allocated null-terminated string which
 * should be #g_free() by the caller.
 */
gchar *
ofa_backup_header_read_header( struct archive *archive, const gchar *name )
{
	static const gchar *thisfn = "ofa_backup_header_read_header";
	struct archive_entry *entry;
	gchar *string, *searched_name;
	const gchar *read_name;
	glong data_size;

	g_debug( "%s: archive=%p, name=%s", thisfn, ( void * ) archive, name );

	string = NULL;
	if( my_strlen( name )){
		searched_name = g_strdup_printf( "%s%s", OFA_BACKUP_HEADER_HEADER, name );
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
			read_name = archive_entry_pathname( entry );
			if( my_collate( read_name, searched_name ) == 0 ){
				data_size = archive_entry_size( entry );
				string = g_new0( gchar, 1+data_size );
				archive_read_data( archive, string, data_size );
				break;
			}
			archive_read_data_skip( archive );
		}
		g_free( searched_name );
	}

	return( string );
}

/**
 * ofa_backup_header_read_data:
 * @archive: an archive file opened in read mode.
 * @data_cb: a #ofaDataCb callback.
 * @user_data: user data to be passed to the callback.
 *
 * Read the data stream, passing it to the @data_cb callback.
 *
 * Returns: %TRUE if the read has terminated successfully.
 *
 * Please note that the data buffer provided to the @data_cb
 * callback is owned by this #ofaBackupHeader, and should not be
 * released by the callback.
 */
gboolean
ofa_backup_header_read_data( struct archive *archive, ofaDataCb data_cb, void *user_data )
{
	static const gchar *thisfn = "ofa_backup_header_read_data";
	struct archive_entry *entry;
	void *buffer;
	static gulong bufsize = 16384;
	glong bytes_read;
	const gchar *cname;
	gboolean found;

	g_debug( "%s: archive=%p ,data_cb=%p, user_data=%p",
			thisfn, ( void * ) archive, ( void * ) data_cb, user_data );

	found = FALSE;
	buffer = g_new0( gchar, bufsize );

	while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ){
		cname = archive_entry_pathname( entry );
		if( g_str_has_prefix( cname, OFA_BACKUP_HEADER_DATA )){
			found = TRUE;
			while(( bytes_read = archive_read_data( archive, buffer, bufsize )) > 0 ){
				data_cb( buffer, bytes_read, user_data );
			}
			break;
		}
		archive_read_data_skip( archive );
	}

	g_free( buffer );

	return( found );
}
