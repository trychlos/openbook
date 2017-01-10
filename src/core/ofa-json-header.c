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

#include <json-glib/json-glib.h>

#include "my/my-date.h"
#include "my/my-utils.h"

#include "api/ofa-json-header.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* header's data
	 */
	gboolean  is_current;
	GDate     begin_date;
	GDate     end_date;
	gchar    *openbook_version;
	GList    *plugins;
	GList    *dbmodels;
	gchar    *comment;
	GTimeVal  stamp;
	gchar    *userid;
}
	ofaJsonHeaderPrivate;

/* plugins data
 */
typedef struct {
	gchar    *canon_name;
	gchar    *display_name;
	gchar    *version;
}
	sPlugin;

/* DBModels data
 */
typedef struct {
	gchar    *id;
	gchar    *version;
}
	sDBModel;

static void free_plugin( sPlugin *sdata );
static void free_dbmodel( sDBModel *sdata );

G_DEFINE_TYPE_EXTENDED( ofaJsonHeader, ofa_json_header, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaJsonHeader ))

static void
json_header_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_json_header_finalize";
	ofaJsonHeaderPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_JSON_HEADER( instance ));

	/* free data members here */
	priv = ofa_json_header_get_instance_private( OFA_JSON_HEADER( instance ));

	g_free( priv->openbook_version );
	g_free( priv->comment );
	g_free( priv->userid );

	g_list_free_full( priv->plugins, ( GDestroyNotify ) free_plugin );
	g_list_free_full( priv->dbmodels, ( GDestroyNotify ) free_dbmodel );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_json_header_parent_class )->finalize( instance );
}

static void
json_header_dispose( GObject *instance )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( instance && OFA_IS_JSON_HEADER( instance ));

	priv = ofa_json_header_get_instance_private( OFA_JSON_HEADER( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_json_header_parent_class )->dispose( instance );
}

static void
ofa_json_header_init( ofaJsonHeader *self )
{
	static const gchar *thisfn = "ofa_json_header_init";
	ofaJsonHeaderPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_JSON_HEADER( self ));

	priv = ofa_json_header_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->openbook_version = g_strdup( PACKAGE_VERSION );
	my_utils_stamp_set_now( &priv->stamp );
}

static void
ofa_json_header_class_init( ofaJsonHeaderClass *klass )
{
	static const gchar *thisfn = "ofa_json_header_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = json_header_dispose;
	G_OBJECT_CLASS( klass )->finalize = json_header_finalize;
}

/**
 * ofa_json_header_new:
 *
 * Allocates and initializes a #ofaJsonHeader object.
 *
 * Returns: a new #ofaJsonHeader object.
 */
ofaJsonHeader *
ofa_json_header_new( void )
{
	ofaJsonHeader *header;

	header = g_object_new( OFA_TYPE_JSON_HEADER, NULL );

	return( header );
}

/**
 * ofa_json_header_get_is_current:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: %TRUE if the backup contains a current dossier.
 */
gboolean
ofa_json_header_get_is_current( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), FALSE );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->is_current );
}

/**
 * ofa_json_header_set_is_current:
 * @header: this #ofaJsonHeader object.
 * @is_current: whether the dossier is current.
 *
 * Set the @is_current flag.
 */
void
ofa_json_header_set_is_current( ofaJsonHeader *header, gboolean is_current )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	priv->is_current = is_current;
}

/**
 * ofa_json_header_get_begin_date:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: the beginning date from the backup'ed exercice, as a valid
 * date, or %NULL.
 */
const GDate *
ofa_json_header_get_begin_date( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), NULL );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( my_date_is_valid( &priv->begin_date ) ? ( const GDate * ) &priv->begin_date : NULL );
}

/**
 * ofa_json_header_set_begin_date:
 * @header: this #ofaJsonHeader object.
 * @date: [allow-none]: the beginning date of the exercice.
 *
 * Set the beginning @date.
 */
void
ofa_json_header_set_begin_date( ofaJsonHeader *header, const GDate *date )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_date_is_valid( date )){
		my_date_set_from_date( &priv->begin_date, date );
	}
}

/**
 * ofa_json_header_get_end_date:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: the ending date from the backup'ed exercice, as a valid
 * date, or %NULL.
 */
const GDate *
ofa_json_header_get_end_date( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), NULL );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( my_date_is_valid( &priv->end_date ) ? ( const GDate * ) &priv->end_date : NULL );
}

/**
 * ofa_json_header_set_end_date:
 * @header: this #ofaJsonHeader object.
 * @date: [allow-none]: the ending date of the exercice.
 *
 * Set the ending @date.
 */
void
ofa_json_header_set_end_date( ofaJsonHeader *header, const GDate *date )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_date_is_valid( date )){
		my_date_set_from_date( &priv->end_date, date );
	}
}

/**
 * ofa_json_header_get_openbook_version:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: the openbook version at time of the backup.
 *
 * The returned string is owned by the @header object, and should not
 * be released by the caller.
 */
const gchar *
ofa_json_header_get_openbook_version( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), NULL );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->openbook_version );
}

/**
 * ofa_json_header_set_openbook_version:
 * @header: this #ofaJsonHeader object.
 * @version: [allow-none]: the openbook version at time of the backup.
 *
 * Set the openbook version.
 *
 * The Openbook version defaults to the current version of the software.
 */
void
ofa_json_header_set_openbook_version( ofaJsonHeader *header, const gchar *version )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->openbook_version );
	priv->openbook_version = g_strdup( version );
}

/**
 * ofa_json_header_set_plugin:
 * @header: this #ofaJsonHeader object.
 * @canon_name: [allow-none]: the canonical name of the plugin.
 * @display_name: [allow-none]: the display name of the plugin.
 * @version: [allow-none]: the version of the plugin.
 *
 * Add these properties to the list of plugins.
 *
 * The code makes its best for keeping the plugins in the order they are
 * added.
 */
void
ofa_json_header_set_plugin( ofaJsonHeader *header, const gchar *canon_name, const gchar *display_name, const gchar *version )
{
	static const gchar *thisfn = "ofa_json_header_set_plugin";
	ofaJsonHeaderPrivate *priv;
	sPlugin *sdata;

	g_debug( "%s: header=%p, canon_name=%s, display_name=%s, version=%s",
			thisfn, ( void * ) header, canon_name, display_name, version );

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = g_new0( sPlugin, 1 );
	sdata->canon_name = g_strdup( canon_name );
	sdata->display_name = g_strdup( display_name );
	sdata->version = g_strdup( version );

	priv->plugins = g_list_append( priv->plugins, sdata );
}

/**
 * ofa_json_header_set_dbmodel:
 * @header: this #ofaJsonHeader object.
 * @id: [allow-none]: the identifier of the dbmodel.
 * @version: [allow-none]: the version of the dbmodel.
 *
 * Add these properties to the list of dbmodels.
 *
 * The code makes its best for keeping the dbmodels in the order they are
 * added.
 */
void
ofa_json_header_set_dbmodel( ofaJsonHeader *header, const gchar *id, const gchar *version )
{
	static const gchar *thisfn = "ofa_json_header_set_dbmodel";
	ofaJsonHeaderPrivate *priv;
	sDBModel *sdata;

	g_debug( "%s: header=%p, id=%s, version=%s",
			thisfn, ( void * ) header, id, version );

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = g_new0( sDBModel, 1 );
	sdata->id = g_strdup( id );
	sdata->version = g_strdup( version );

	priv->dbmodels = g_list_append( priv->dbmodels, sdata );
}

/**
 * ofa_json_header_get_comment:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: the user comment for this backup.
 *
 * The returned string is owned by the @header object, and should not
 * be released by the caller.
 */
const gchar *
ofa_json_header_get_comment( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), NULL );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->comment );
}

/**
 * ofa_json_header_set_comment:
 * @header: this #ofaJsonHeader object.
 * @comment: [allow-none]: the user comment for this backup.
 *
 * Set the user comment.
 */
void
ofa_json_header_set_comment( ofaJsonHeader *header, const gchar *comment )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->comment );
	priv->comment = g_strdup( comment );
}

/**
 * ofa_json_header_get_current_stamp:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: the current timestamp at backup time.
 */
const GTimeVal *
ofa_json_header_get_current_stamp( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), NULL );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const GTimeVal * ) &priv->stamp );
}

/**
 * ofa_json_header_set_current_stamp:
 * @header: this #ofaJsonHeader object.
 * @stamp: [allow-none]: a timestamp.
 *
 * Set the current timestamp.
 *
 * The current timestamp defaults to the timestamp at @header
 * instanciation time.
 */
void
ofa_json_header_set_current_stamp( ofaJsonHeader *header, const GTimeVal *stamp )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	my_utils_stamp_set_from_stamp( &priv->stamp, stamp );
}

/**
 * ofa_json_header_get_current_user:
 * @header: this #ofaJsonHeader object.
 *
 * Returns: the connected user identifier at time of the backup.
 *
 * The returned string is owned by the @header object, and should not
 * be released by the caller.
 */
const gchar *
ofa_json_header_get_current_user( ofaJsonHeader *header )
{
	ofaJsonHeaderPrivate *priv;

	g_return_val_if_fail( header && OFA_IS_JSON_HEADER( header ), NULL );

	priv = ofa_json_header_get_instance_private( header );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->userid );
}

/**
 * ofa_json_header_set_current_user:
 * @header: this #ofaJsonHeader object.
 * @userid: [allow-none]: a user identifier.
 *
 * Set the currently connected user.
 */
void
ofa_json_header_set_current_user( ofaJsonHeader *header, const gchar *userid )
{
	ofaJsonHeaderPrivate *priv;

	g_return_if_fail( header && OFA_IS_JSON_HEADER( header ));

	priv = ofa_json_header_get_instance_private( header );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->userid );
	priv->userid = g_strdup( userid );
}

static void
free_plugin( sPlugin *sdata )
{
	g_free( sdata->canon_name );
	g_free( sdata->display_name );
	g_free( sdata->version );

	g_free( sdata );
}

static void
free_dbmodel( sDBModel *sdata )
{
	g_free( sdata->id );
	g_free( sdata->version );

	g_free( sdata );
}
