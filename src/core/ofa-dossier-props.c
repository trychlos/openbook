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

#include "my/my-date.h"
#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-dossier-props.h"
#include "api/ofa-extender-collection.h"
#include "api/ofa-extender-module.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-idbmodel.h"
#include "api/ofo-dossier.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* props's data
	 */
	gboolean  is_current;
	GDate     begin_date;
	GDate     end_date;
	gchar    *rpid;
	gchar    *openbook_version;
	GList    *plugins;
	GList    *dbmodels;
	gchar    *comment;
	GTimeVal  stamp;
	gchar    *userid;

	/* read from archived JSON header
	 */
	gchar    *name;
}
	ofaDossierPropsPrivate;

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

static const gchar *st_current          = "current";
static const gchar *st_begin            = "begin";
static const gchar *st_end              = "end";
static const gchar *st_rpid             = "rpid";
static const gchar *st_openbook         = "openbook";
static const gchar *st_plugins          = "plugins";
static const gchar *st_dbms             = "dbms";
static const gchar *st_comment          = "comment";
static const gchar *st_stamp            = "stamp";
static const gchar *st_userid           = "userid";
static const gchar *st_canon            = "canon";
static const gchar *st_display          = "display";
static const gchar *st_version          = "version";
static const gchar *st_id               = "id";

static const gchar *st_props_header     = "DossierPropsHeader";

static ofaDossierProps *new_from_string( const gchar *str );
static ofaDossierProps *new_from_node( JsonNode *node );
static void             set_plugins_from_array( ofaDossierProps *self, JsonArray *array );
static void             set_dbms_from_array( ofaDossierProps *self, JsonArray *array );
static void             free_plugin( sPlugin *sdata );
static void             free_dbmodel( sDBModel *sdata );

G_DEFINE_TYPE_EXTENDED( ofaDossierProps, ofa_dossier_props, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaDossierProps ))

static void
dossier_props_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_dossier_props_finalize";
	ofaDossierPropsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPS( instance ));

	/* free data members here */
	priv = ofa_dossier_props_get_instance_private( OFA_DOSSIER_PROPS( instance ));

	g_free( priv->rpid );
	g_free( priv->openbook_version );
	g_free( priv->comment );
	g_free( priv->userid );
	g_free( priv->name );

	g_list_free_full( priv->plugins, ( GDestroyNotify ) free_plugin );
	g_list_free_full( priv->dbmodels, ( GDestroyNotify ) free_dbmodel );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_dossier_props_parent_class )->finalize( instance );
}

static void
dossier_props_dispose( GObject *instance )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_DOSSIER_PROPS( instance ));

	priv = ofa_dossier_props_get_instance_private( OFA_DOSSIER_PROPS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_dossier_props_parent_class )->dispose( instance );
}

static void
ofa_dossier_props_init( ofaDossierProps *self )
{
	static const gchar *thisfn = "ofa_dossier_props_init";
	ofaDossierPropsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_DOSSIER_PROPS( self ));

	priv = ofa_dossier_props_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->openbook_version = g_strdup( PACKAGE_VERSION );
	my_utils_stamp_set_now( &priv->stamp );
}

static void
ofa_dossier_props_class_init( ofaDossierPropsClass *klass )
{
	static const gchar *thisfn = "ofa_dossier_props_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_props_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_props_finalize;
}

/**
 * ofa_dossier_props_get_title:
 *
 * Returns: a name for these dossier properties.
 *
 * This name is in particular used as a header in archive files.
 */
const gchar *
ofa_dossier_props_get_title( void )
{
	return( st_props_header );
}

/**
 * ofa_dossier_props_get_json_string_ex:
 * @hub: the @ofaHub object of the application.
 * @comment: [allow-none]: a user comment.
 *
 * Returns: the current properties as a newly allocated JSON string
 * which should be #g_free() by the caller.
 */
gchar *
ofa_dossier_props_get_json_string_ex( ofaHub *hub, const gchar *comment )
{
	static const gchar *thisfn = "ofa_dossier_props_get_json_string_ex";
	ofaDossierProps *props;
	ofoDossier *dossier;
	ofaExtenderCollection *extenders;
	ofaExtenderModule *plugin;
	const GList *modules, *itm;
	GList *dbmodels, *itb;
	gchar *canon, *display, *version, *id, *json;
	const ofaIDBConnect *connect;

	g_debug( "%s: hub=%p, comment=%s", thisfn, ( void * ) hub, comment );

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	props = ofa_dossier_props_new();
	dossier = ofa_hub_get_dossier( hub );
	ofa_dossier_props_set_is_current( props, ofo_dossier_is_current( dossier ));
	ofa_dossier_props_set_begin_date( props, ofo_dossier_get_exe_begin( dossier ));
	ofa_dossier_props_set_end_date( props, ofo_dossier_get_exe_end( dossier ));

	extenders = ofa_hub_get_extender_collection( hub );
	modules = ofa_extender_collection_get_modules( extenders );

	for( itm=modules ; itm ; itm=itm->next ){
		plugin = OFA_EXTENDER_MODULE( itm->data );
		canon = ofa_extender_module_get_canon_name( plugin );
		display = ofa_extender_module_get_display_name( plugin );
		version = ofa_extender_module_get_version( plugin );
		ofa_dossier_props_set_plugin( props, canon, display, version );
		g_free( version );
		g_free( display );
		g_free( canon );
	}

	dbmodels = ofa_hub_get_for_type( hub, OFA_TYPE_IDBMODEL );

	for( itb=dbmodels ; itb ; itb=itb->next ){
		if( MY_IS_IIDENT( itb->data )){
			id = my_iident_get_canon_name( MY_IIDENT( itb->data ), NULL );
			version = my_iident_get_version( MY_IIDENT( itb->data ), NULL );
			ofa_dossier_props_set_dbmodel( props, id, version );
			g_free( version );
			g_free( id );
		}
	}

	g_list_free_full( dbmodels, ( GDestroyNotify ) g_object_unref );

	ofa_dossier_props_set_comment( props, comment );
	connect = ofa_hub_get_connect( hub );
	ofa_dossier_props_set_current_user( props, ofa_idbconnect_get_account( connect ));

	/* get JSON data string */
	json = ofa_dossier_props_get_json_string( props );
	g_object_unref( props );

	return( json );
}

/**
 * ofa_dossier_props_new:
 *
 * Allocates and initializes a #ofaDossierProps object.
 *
 * Returns: a new #ofaDossierProps object.
 */
ofaDossierProps *
ofa_dossier_props_new( void )
{
	ofaDossierProps *props;

	props = g_object_new( OFA_TYPE_DOSSIER_PROPS, NULL );

	return( props );
}

/**
 * ofa_dossier_props_new_from_uri:
 *
 * Try to extract a DossierProperties JSON string from the provided
 * archive.
 *
 * Returns: a new #ofaDossierProps object if the header has been found,
 * or %NULL.
 */
ofaDossierProps *
ofa_dossier_props_new_from_uri( const gchar *uri )
{
	static const gchar *thisfn = "ofa_dossier_props_new_from_uri";
	ofaDossierProps *props;
	GFile *file;
	struct archive *a;
	struct archive_entry *entry;
	gchar *pathname, *buf, *name;
	gsize size, read;

	props = NULL;
	name = NULL;
	a = archive_read_new();
	archive_read_support_filter_all( a );
	archive_read_support_format_all( a );
	file = g_file_new_for_uri( uri );
	pathname = g_file_get_path( file );

	if( archive_read_open_filename( a, pathname, 16384 ) != ARCHIVE_OK ){
		g_warning( "%s: archive_read_open_filename: %s", thisfn, archive_error_string( a ));
		archive_read_free( a );
		g_free( pathname );
		g_object_unref( file );
		return( NULL );
	}

	while( archive_read_next_header( a, &entry ) == ARCHIVE_OK ){
		if( !my_collate( archive_entry_pathname( entry ), st_props_header )){
			size = archive_entry_size( entry );
			buf = g_new0( gchar, 1+size );
			read = archive_read_data( a, buf, size );
			g_debug( "%s: size=%lu, read=%lu", thisfn, size, read );
			props = new_from_string( buf );
			g_free( buf );
		} else {
			name = g_strdup( archive_entry_pathname( entry ));
			g_debug( "%s: name=%s", thisfn, name );
		}
	}

	if( props ){
		ofa_dossier_props_set_name( props, name );
	}

	g_free( name );
	g_free( pathname );
	g_object_unref( file );
	archive_read_close( a );
	archive_read_free( a );

	return( props );
}

static ofaDossierProps *
new_from_string( const gchar *str )
{
	static const gchar *thisfn = "ofa_dossier_props_new_from_string";
	ofaDossierProps *props;
	JsonParser *parser;
	JsonNode *root;
	GError *error;

	error = NULL;
	parser = json_parser_new();

	if( !json_parser_load_from_data( parser, str, -1, &error )){
		g_warning( "%s: json_parser_load_from_data: %s", thisfn, error->message );
		return( NULL );
	}

	root = json_parser_get_root( parser );
	props = new_from_node( root );
	g_object_unref( parser );

	return( props );
}

static ofaDossierProps *
new_from_node( JsonNode *root )
{
	static const gchar *thisfn = "ofa_dossier_props_new_from_node";
	ofaDossierProps *props;
	JsonNodeType root_type, node_type;
	JsonObject *object;
	JsonNode *node;
	JsonArray *array;
	GList *members, *itm;
	const gchar *cname, *cvalue;
	GDate date;
	GTimeVal stamp;

	props = ofa_dossier_props_new();
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
					case JSON_NODE_ARRAY:
						array = json_node_get_array( node );

						if( !my_collate( cname, st_plugins )){
							set_plugins_from_array( props, array );

						} else if( !my_collate( cname, st_dbms )){
							set_dbms_from_array( props, array );

						} else {
							g_warning( "%s: unexpected member name %s", thisfn, cname );
						}
						break;

					case JSON_NODE_VALUE:
						cvalue = json_node_get_string( node );

						if( !my_collate( cname, st_current )){
							ofa_dossier_props_set_is_current( props, my_utils_boolean_from_str( cvalue ));

						} else if( !my_collate( cname, st_begin )){
							my_date_set_from_str( &date, cvalue, MY_DATE_YYMD );
							ofa_dossier_props_set_begin_date( props, &date );

						} else if( !my_collate( cname, st_end )){
							my_date_set_from_str( &date, cvalue, MY_DATE_YYMD );
							ofa_dossier_props_set_end_date( props, &date );

						} else if( !my_collate( cname, st_rpid )){
							ofa_dossier_props_set_rpid( props, cvalue );

						} else if( !my_collate( cname, st_openbook )){
							ofa_dossier_props_set_openbook_version( props, cvalue );

						} else if( !my_collate( cname, st_comment )){
							ofa_dossier_props_set_comment( props, cvalue );

						} else if( !my_collate( cname, st_stamp )){
							my_utils_stamp_set_from_sql( &stamp, cvalue );
							ofa_dossier_props_set_current_stamp( props, &stamp );

						} else if( !my_collate( cname, st_userid )){
							ofa_dossier_props_set_current_user( props, cvalue );

						} else {
							g_warning( "%s: unexpected member name %s", thisfn, cname );
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

static void
set_plugins_from_array( ofaDossierProps *self, JsonArray *array )
{
	static const gchar *thisfn = "ofa_dossier_props_set_plugins_from_array";
	GList *elements, *ite, *members, *itm;
	JsonNode *node, *member_node;
	JsonNodeType node_type, member_type;
	JsonObject *object;
	const gchar *cname, *cvalue;
	gchar *canon, *display, *version;

	elements = json_array_get_elements( array );
	for( ite=elements ; ite ; ite=ite->next ){
		node = ( JsonNode * ) ite->data;
		node_type = json_node_get_node_type( node );
		canon = NULL;
		display = NULL;
		version = NULL;
		switch( node_type ){
			case JSON_NODE_OBJECT:
				object = json_node_get_object( node );
				members = json_object_get_members( object );
				for( itm=members ; itm ; itm=itm->next ){
					cname = ( const gchar * ) itm->data;
					member_node = json_object_get_member( object, cname );
					member_type = json_node_get_node_type( member_node );

					switch( member_type ){
						case JSON_NODE_VALUE:
							cvalue = json_node_get_string( member_node );

							if( !my_collate( cname, st_canon )){
								canon = g_strdup( cvalue );

							} else if( !my_collate( cname, st_display )){
								display = g_strdup( cvalue );

							} else if( !my_collate( cname, st_version )){
								version = g_strdup( cvalue );

							} else {
								g_warning( "%s: unexpected member name %s", thisfn, cname );
							}
							break;

						default:
							g_warning( "%s: unexpected member node type %d", thisfn, ( gint ) member_type );
							break;
					}
				}
				g_list_free( members );
				break;

			default:
				g_warning( "%s: unexpected element node type %d", thisfn, ( gint ) node_type );
				break;
		}
		ofa_dossier_props_set_plugin( self, canon, display, version );
		g_free( canon );
		g_free( display );
		g_free( version );
	}
}

static void
set_dbms_from_array( ofaDossierProps *self, JsonArray *array )
{
	static const gchar *thisfn = "ofa_dossier_props_set_dbms_from_array";
	GList *elements, *ite, *members, *itm;
	JsonNode *node, *member_node;
	JsonNodeType node_type, member_type;
	JsonObject *object;
	const gchar *cname, *cvalue;
	gchar *id, *version;

	elements = json_array_get_elements( array );
	for( ite=elements ; ite ; ite=ite->next ){
		node = ( JsonNode * ) ite->data;
		node_type = json_node_get_node_type( node );
		id = NULL;
		version = NULL;
		switch( node_type ){
			case JSON_NODE_OBJECT:
				object = json_node_get_object( node );
				members = json_object_get_members( object );
				for( itm=members ; itm ; itm=itm->next ){
					cname = ( const gchar * ) itm->data;
					member_node = json_object_get_member( object, cname );
					member_type = json_node_get_node_type( member_node );

					switch( member_type ){
						case JSON_NODE_VALUE:
							cvalue = json_node_get_string( member_node );

							if( !my_collate( cname, st_id )){
								id = g_strdup( cvalue );

							} else if( !my_collate( cname, st_version )){
								version = g_strdup( cvalue );

							} else {
								g_warning( "%s: unexpected member name %s", thisfn, cname );
							}
							break;

						default:
							g_warning( "%s: unexpected member node type %d", thisfn, ( gint ) member_type );
							break;
					}
				}
				g_list_free( members );
				break;

			default:
				g_warning( "%s: unexpected element node type %d", thisfn, ( gint ) node_type );
				break;
		}
		ofa_dossier_props_set_dbmodel( self, id, version );
		g_free( id );
		g_free( version );
	}
}

/**
 * ofa_dossier_props_get_is_current:
 * @props: this #ofaDossierProps object.
 *
 * Returns: %TRUE if the backup contains a current dossier.
 */
gboolean
ofa_dossier_props_get_is_current( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), FALSE );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, FALSE );

	return( priv->is_current );
}

/**
 * ofa_dossier_props_set_is_current:
 * @props: this #ofaDossierProps object.
 * @is_current: whether the dossier is current.
 *
 * Set the @is_current flag.
 */
void
ofa_dossier_props_set_is_current( ofaDossierProps *props, gboolean is_current )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	priv->is_current = is_current;
}

/**
 * ofa_dossier_props_get_begin_date:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the beginning date from the backup'ed exercice, as a valid
 * date, or %NULL.
 */
const GDate *
ofa_dossier_props_get_begin_date( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( my_date_is_valid( &priv->begin_date ) ? ( const GDate * ) &priv->begin_date : NULL );
}

/**
 * ofa_dossier_props_set_begin_date:
 * @props: this #ofaDossierProps object.
 * @date: [allow-none]: the beginning date of the exercice.
 *
 * Set the beginning @date.
 */
void
ofa_dossier_props_set_begin_date( ofaDossierProps *props, const GDate *date )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_date_is_valid( date )){
		my_date_set_from_date( &priv->begin_date, date );
	}
}

/**
 * ofa_dossier_props_get_end_date:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the ending date from the backup'ed exercice, as a valid
 * date, or %NULL.
 */
const GDate *
ofa_dossier_props_get_end_date( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( my_date_is_valid( &priv->end_date ) ? ( const GDate * ) &priv->end_date : NULL );
}

/**
 * ofa_dossier_props_set_end_date:
 * @props: this #ofaDossierProps object.
 * @date: [allow-none]: the ending date of the exercice.
 *
 * Set the ending @date.
 */
void
ofa_dossier_props_set_end_date( ofaDossierProps *props, const GDate *date )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	if( my_date_is_valid( date )){
		my_date_set_from_date( &priv->end_date, date );
	}
}

/**
 * ofa_dossier_props_get_rpid:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the rpid of the archived dossier.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_dossier_props_get_rpid( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->rpid );
}

/**
 * ofa_dossier_props_set_rpid:
 * @props: this #ofaDossierProps object.
 * @rpid: [allow-none]: the rpid of the dossier.
 *
 * Set the rpid.
 */
void
ofa_dossier_props_set_rpid( ofaDossierProps *props, const gchar *rpid )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->rpid );
	priv->rpid = g_strdup( rpid );
}

/**
 * ofa_dossier_props_get_openbook_version:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the openbook version at time of the backup.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_dossier_props_get_openbook_version( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->openbook_version );
}

/**
 * ofa_dossier_props_set_openbook_version:
 * @props: this #ofaDossierProps object.
 * @version: [allow-none]: the openbook version at time of the backup.
 *
 * Set the openbook version.
 *
 * The Openbook version defaults to the current version of the software.
 */
void
ofa_dossier_props_set_openbook_version( ofaDossierProps *props, const gchar *version )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->openbook_version );
	priv->openbook_version = g_strdup( version );
}

/**
 * ofa_dossier_props_set_plugin:
 * @props: this #ofaDossierProps object.
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
ofa_dossier_props_set_plugin( ofaDossierProps *props, const gchar *canon_name, const gchar *display_name, const gchar *version )
{
	static const gchar *thisfn = "ofa_dossier_props_set_plugin";
	ofaDossierPropsPrivate *priv;
	sPlugin *sdata;

	g_debug( "%s: props=%p, canon_name=%s, display_name=%s, version=%s",
			thisfn, ( void * ) props, canon_name, display_name, version );

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = g_new0( sPlugin, 1 );
	sdata->canon_name = g_strdup( canon_name );
	sdata->display_name = g_strdup( display_name );
	sdata->version = g_strdup( version );

	priv->plugins = g_list_append( priv->plugins, sdata );
}

/**
 * ofa_dossier_props_set_dbmodel:
 * @props: this #ofaDossierProps object.
 * @id: [allow-none]: the identifier of the dbmodel.
 * @version: [allow-none]: the version of the dbmodel.
 *
 * Add these properties to the list of dbmodels.
 *
 * The code makes its best for keeping the dbmodels in the order they are
 * added.
 */
void
ofa_dossier_props_set_dbmodel( ofaDossierProps *props, const gchar *id, const gchar *version )
{
	static const gchar *thisfn = "ofa_dossier_props_set_dbmodel";
	ofaDossierPropsPrivate *priv;
	sDBModel *sdata;

	g_debug( "%s: props=%p, id=%s, version=%s",
			thisfn, ( void * ) props, id, version );

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = g_new0( sDBModel, 1 );
	sdata->id = g_strdup( id );
	sdata->version = g_strdup( version );

	priv->dbmodels = g_list_append( priv->dbmodels, sdata );
}

/**
 * ofa_dossier_props_get_comment:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the user comment for this backup.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_dossier_props_get_comment( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->comment );
}

/**
 * ofa_dossier_props_set_comment:
 * @props: this #ofaDossierProps object.
 * @comment: [allow-none]: the user comment for this backup.
 *
 * Set the user comment.
 */
void
ofa_dossier_props_set_comment( ofaDossierProps *props, const gchar *comment )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->comment );
	priv->comment = g_strdup( comment );
}

/**
 * ofa_dossier_props_get_current_stamp:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the current timestamp at backup time.
 */
const GTimeVal *
ofa_dossier_props_get_current_stamp( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const GTimeVal * ) &priv->stamp );
}

/**
 * ofa_dossier_props_set_current_stamp:
 * @props: this #ofaDossierProps object.
 * @stamp: [allow-none]: a timestamp.
 *
 * Set the current timestamp.
 *
 * The current timestamp defaults to the timestamp at @props
 * instanciation time.
 */
void
ofa_dossier_props_set_current_stamp( ofaDossierProps *props, const GTimeVal *stamp )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	my_utils_stamp_set_from_stamp( &priv->stamp, stamp );
}

/**
 * ofa_dossier_props_get_current_user:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the connected user identifier at time of the backup.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_dossier_props_get_current_user( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->userid );
}

/**
 * ofa_dossier_props_set_current_user:
 * @props: this #ofaDossierProps object.
 * @userid: [allow-none]: a user identifier.
 *
 * Set the currently connected user.
 */
void
ofa_dossier_props_set_current_user( ofaDossierProps *props, const gchar *userid )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->userid );
	priv->userid = g_strdup( userid );
}

/**
 * ofa_dossier_props_get_json_string:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the JSON datas as a null-terminated string, in a newly
 * allocated buffer which should be #g_free() by the caller.
 */
gchar *
ofa_dossier_props_get_json_string( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;
	JsonBuilder *builder;
	JsonGenerator *generator;
	gchar *sdate;
	sPlugin *splugin;
	sDBModel *sdbmodel;
	GList *it;
	gchar *str;

g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	builder = json_builder_new();
	json_builder_begin_object( builder );

	json_builder_set_member_name( builder, st_current );
	json_builder_add_string_value( builder, priv->is_current ? "Y":"N" );

	sdate = my_date_is_valid( &priv->begin_date ) ? my_date_to_str( &priv->begin_date, MY_DATE_YYMD ) : g_strdup( "" );
	json_builder_set_member_name( builder, st_begin );
	json_builder_add_string_value( builder, sdate );
	g_free( sdate );

	sdate = my_date_is_valid( &priv->end_date ) ? my_date_to_str( &priv->end_date, MY_DATE_YYMD ) : g_strdup( "" );
	json_builder_set_member_name( builder, st_end );
	json_builder_add_string_value( builder, sdate );
	g_free( sdate );

	json_builder_set_member_name( builder, st_openbook );
	json_builder_add_string_value( builder, priv->openbook_version );

	json_builder_set_member_name( builder, st_plugins );
	json_builder_begin_array( builder );
	for( it=priv->plugins ; it ; it=it->next ){
		splugin = ( sPlugin * ) it->data;
		json_builder_begin_object( builder );
		json_builder_set_member_name( builder, st_canon );
		json_builder_add_string_value( builder, splugin->canon_name ? splugin->canon_name : "" );
		json_builder_set_member_name( builder, st_display );
		json_builder_add_string_value( builder, splugin->display_name ? splugin->display_name : "" );
		json_builder_set_member_name( builder, st_version );
		json_builder_add_string_value( builder, splugin->version ? splugin->version : "" );
		json_builder_end_object( builder );
	}
	json_builder_end_array( builder );

	json_builder_set_member_name( builder, st_dbms );
	json_builder_begin_array( builder );
	for( it=priv->dbmodels ; it ; it=it->next ){
		sdbmodel = ( sDBModel * ) it->data;
		json_builder_begin_object( builder );
		json_builder_set_member_name( builder, st_id );
		json_builder_add_string_value( builder, sdbmodel->id ? sdbmodel->id : "" );
		json_builder_set_member_name( builder, st_version );
		json_builder_add_string_value( builder, sdbmodel->version ? sdbmodel->version : "" );
		json_builder_end_object( builder );
	}
	json_builder_end_array( builder );

	json_builder_set_member_name( builder, st_comment );
	json_builder_add_string_value( builder, priv->comment ? priv->comment : "" );

	sdate = my_utils_stamp_to_str( &priv->stamp, MY_STAMP_YYMDHMS );
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

/**
 * ofa_dossier_props_get_name:
 * @props: this #ofaDossierProps object.
 *
 * Returns: the name found in the header of the archive.
 *
 * This name is only set when reading an archive, and is expected to be
 * the pathname of the second data stream (the DBMS itself).
 *
 * Even if set, it is not written to the DossierProps header when
 * writing an archive.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_dossier_props_get_name( ofaDossierProps *props )
{
	ofaDossierPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_DOSSIER_PROPS( props ), NULL );

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return( priv->name );
}

/**
 * ofa_dossier_props_set_name:
 * @props: this #ofaDossierProps object.
 * @name: [allow-none]: the name of the second datas stream.
 *
 * Set the name.
 *
 * This name is only set when reading an archive, and is expected to be
 * the pathname of the second data stream (the DBMS itself).
 *
 * Even if set, it is not written to the DossierProps header when
 * writing an archive.
 */
void
ofa_dossier_props_set_name( ofaDossierProps *props, const gchar *name )
{
	ofaDossierPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_DOSSIER_PROPS( props ));

	priv = ofa_dossier_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->name );
	priv->name = g_strdup( name );
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
