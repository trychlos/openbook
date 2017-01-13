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

#include "my/my-stamp.h"
#include "my/my-utils.h"

#include "api/ofa-backup-props.h"
#include "api/ofa-ijson.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* props's data
	 */
	gchar    *comment;
	GTimeVal  stamp;
	gchar    *userid;
}
	ofaBackupPropsPrivate;

static const gchar *st_comment          = "comment";
static const gchar *st_stamp            = "stamp";
static const gchar *st_userid           = "userid";

static const gchar *st_props_title      = "BackupProps";

static ofaBackupProps *new_from_node( JsonNode *node );
static void            ijson_iface_init( ofaIJsonInterface *iface );
static guint           ijson_get_interface_version( void );
static gchar          *ijson_get_title( void );
static gchar          *ijson_get_as_string( ofaIJson *instance );

G_DEFINE_TYPE_EXTENDED( ofaBackupProps, ofa_backup_props, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaBackupProps )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IJSON, ijson_iface_init ))

static void
backup_props_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_backup_props_finalize";
	ofaBackupPropsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_BACKUP_PROPS( instance ));

	/* free data members here */
	priv = ofa_backup_props_get_instance_private( OFA_BACKUP_PROPS( instance ));

	g_free( priv->comment );
	g_free( priv->userid );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_backup_props_parent_class )->finalize( instance );
}

static void
backup_props_dispose( GObject *instance )
{
	ofaBackupPropsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_BACKUP_PROPS( instance ));

	priv = ofa_backup_props_get_instance_private( OFA_BACKUP_PROPS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_backup_props_parent_class )->dispose( instance );
}

static void
ofa_backup_props_init( ofaBackupProps *self )
{
	static const gchar *thisfn = "ofa_backup_props_init";
	ofaBackupPropsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_BACKUP_PROPS( self ));

	priv = ofa_backup_props_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	my_stamp_set_now( &priv->stamp );
}

static void
ofa_backup_props_class_init( ofaBackupPropsClass *klass )
{
	static const gchar *thisfn = "ofa_backup_props_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = backup_props_dispose;
	G_OBJECT_CLASS( klass )->finalize = backup_props_finalize;
}

/**
 * ofa_backup_props_new:
 *
 * Allocates and initializes a #ofaBackupProps object.
 *
 * Returns: a new #ofaBackupProps object.
 */
ofaBackupProps *
ofa_backup_props_new( void )
{
	ofaBackupProps *props;

	props = g_object_new( OFA_TYPE_BACKUP_PROPS, NULL );

	return( props );
}

/**
 * ofa_backup_props_new_from_string:
 * @string: a JSON string.
 *
 * Try to parse the provided JSON string.
 *
 * Returns: a new #ofaBackupProps object if the header has been
 * successfully parsed, or %NULL.
 */
ofaBackupProps *
ofa_backup_props_new_from_string( const gchar *string )
{
	static const gchar *thisfn = "ofa_backup_props_new_from_string";
	ofaBackupProps *props;
	JsonParser *parser;
	JsonNode *root;
	GError *error;

	error = NULL;
	parser = json_parser_new();

	if( !json_parser_load_from_data( parser, string, -1, &error )){
		g_warning( "%s: json_parser_load_from_data: %s", thisfn, error->message );
		return( NULL );
	}

	root = json_parser_get_root( parser );
	props = new_from_node( root );
	g_object_unref( parser );

	return( props );
}

static ofaBackupProps *
new_from_node( JsonNode *root )
{
	static const gchar *thisfn = "ofa_backup_props_new_from_node";
	ofaBackupProps *props;
	JsonNodeType root_type, node_type;
	JsonObject *object;
	JsonNode *node;
	GList *members, *itm;
	const gchar *cname, *cvalue;
	GTimeVal stamp;

	props = ofa_backup_props_new();
	root_type = json_node_get_node_type( root );

	switch( root_type ){
		case JSON_NODE_OBJECT:
			object = json_node_get_object( root );
			members = json_object_get_members( object );
			for( itm=members ; itm ; itm=itm->next ){
				cname = ( const gchar * ) itm->data;
				node = json_object_get_member( object, cname );
				node_type = json_node_get_node_type( node );

				switch( node_type ){
					case JSON_NODE_VALUE:
						cvalue = json_node_get_string( node );

						if( !my_collate( cname, st_comment )){
							ofa_backup_props_set_comment( props, cvalue );

						} else if( !my_collate( cname, st_stamp )){
							my_stamp_set_from_sql( &stamp, cvalue );
							ofa_backup_props_set_stamp( props, &stamp );

						} else if( !my_collate( cname, st_userid )){
							ofa_backup_props_set_userid( props, cvalue );

						} else {
							g_warning( "%s: unexpected member name=%s, value=%s", thisfn, cname, cvalue );
						}
						break;

					default:
						g_warning( "%s: unexpected node type %d", thisfn, ( gint ) node_type );
						break;
				}
			}
			g_list_free( members );
			break;

		default:
			g_warning( "%s: unexpected root node type %d", thisfn, ( gint ) root_type );
			break;
	}

	return( props );
}

/**
 * ofa_backup_props_get_comment:
 * @props: this #ofaBackupProps object.
 *
 * Returns: the user comment for this backup.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_backup_props_get_comment( ofaBackupProps *props )
{
	ofaBackupPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_BACKUP_PROPS( props ), NULL );

	priv = ofa_backup_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->comment );
}

/**
 * ofa_backup_props_set_comment:
 * @props: this #ofaBackupProps object.
 * @comment: [allow-none]: the user comment for this backup.
 *
 * Set the user comment.
 */
void
ofa_backup_props_set_comment( ofaBackupProps *props, const gchar *comment )
{
	ofaBackupPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_BACKUP_PROPS( props ));

	priv = ofa_backup_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->comment );
	priv->comment = g_strdup( comment );
}

/**
 * ofa_backup_props_get_stamp:
 * @props: this #ofaBackupProps object.
 *
 * Returns: the current timestamp at backup time.
 */
const GTimeVal *
ofa_backup_props_get_stamp( ofaBackupProps *props )
{
	ofaBackupPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_BACKUP_PROPS( props ), NULL );

	priv = ofa_backup_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const GTimeVal * ) &priv->stamp );
}

/**
 * ofa_backup_props_set_stamp:
 * @props: this #ofaBackupProps object.
 * @stamp: [allow-none]: a timestamp.
 *
 * Set the current timestamp.
 *
 * The current timestamp defaults to the timestamp at @props
 * instanciation time.
 */
void
ofa_backup_props_set_stamp( ofaBackupProps *props, const GTimeVal *stamp )
{
	ofaBackupPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_BACKUP_PROPS( props ));

	priv = ofa_backup_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	my_stamp_set_from_stamp( &priv->stamp, stamp );
}

/**
 * ofa_backup_props_get_userid:
 * @props: this #ofaBackupProps object.
 *
 * Returns: the connected user identifier at time of the backup.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_backup_props_get_userid( ofaBackupProps *props )
{
	ofaBackupPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_BACKUP_PROPS( props ), NULL );

	priv = ofa_backup_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->userid );
}

/**
 * ofa_backup_props_set_userid:
 * @props: this #ofaBackupProps object.
 * @userid: [allow-none]: a user identifier.
 *
 * Set the currently connected user.
 */
void
ofa_backup_props_set_userid( ofaBackupProps *props, const gchar *userid )
{
	ofaBackupPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_BACKUP_PROPS( props ));

	priv = ofa_backup_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->userid );
	priv->userid = g_strdup( userid );
}

/*
 * ofaIJson interface management
 */
static void
ijson_iface_init( ofaIJsonInterface *iface )
{
	static const gchar *thisfn = "ofa_dossier_props_ijson_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = ijson_get_interface_version;
	iface->get_title = ijson_get_title;
	iface->get_as_string = ijson_get_as_string;
}

static guint
ijson_get_interface_version( void )
{
	return( 1 );
}

static gchar *
ijson_get_title( void )
{
	return( g_strdup( st_props_title ));
}

static gchar *
ijson_get_as_string( ofaIJson *instance )
{
	ofaBackupPropsPrivate *priv;
	JsonBuilder *builder;
	JsonGenerator *generator;
	gchar *sdate;
	gchar *str;

	priv = ofa_backup_props_get_instance_private( OFA_BACKUP_PROPS( instance ));

	builder = json_builder_new();
	json_builder_begin_object( builder );

	json_builder_set_member_name( builder, st_comment );
	json_builder_add_string_value( builder, priv->comment ? priv->comment : "" );

	sdate = my_stamp_to_str( &priv->stamp, MY_STAMP_YYMDHMS );
	json_builder_set_member_name( builder, st_stamp );
	json_builder_add_string_value( builder, sdate );
	g_free( sdate );

	json_builder_set_member_name( builder, st_userid );
	json_builder_add_string_value( builder, priv->userid );

	json_builder_end_object( builder );

	generator = json_generator_new();
	json_generator_set_root( generator, json_builder_get_root( builder ));
	str = json_generator_to_data( generator, NULL );

	g_object_unref( generator );
	g_object_unref( builder );

	return( str );
}
