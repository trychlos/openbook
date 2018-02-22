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

#include <json-glib/json-glib.h>

#include "my/my-iident.h"
#include "my/my-utils.h"

#include "api/ofa-extender-collection.h"
#include "api/ofa-extender-module.h"
#include "api/ofa-igetter.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-ijson.h"
#include "api/ofa-openbook-props.h"

/* private instance data
 */
typedef struct {
	gboolean  dispose_has_run;

	/* props's data
	 */
	gchar    *openbook_version;
	GList    *plugins;
	GList    *dbmodels;
}
	ofaOpenbookPropsPrivate;

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

static const gchar *st_openbook         = "openbook";
static const gchar *st_plugins          = "plugins";
static const gchar *st_dbms             = "dbms";
static const gchar *st_canon            = "canon";
static const gchar *st_display          = "display";
static const gchar *st_version          = "version";
static const gchar *st_id               = "id";

static const gchar *st_props_title      = "OpenbookProps";

static ofaOpenbookProps *new_from_node( ofaIGetter *getter, JsonNode *node );
static void              set_plugins_from_array( ofaOpenbookProps *self, JsonArray *array );
static void              set_dbms_from_array( ofaOpenbookProps *self, JsonArray *array );
static void              free_plugin( sPlugin *sdata );
static void              free_dbmodel( sDBModel *sdata );
static void              ijson_iface_init( ofaIJsonInterface *iface );
static guint             ijson_get_interface_version( void );
static gchar            *ijson_get_title( void );
static gchar            *ijson_get_as_string( ofaIJson *instance );

G_DEFINE_TYPE_EXTENDED( ofaOpenbookProps, ofa_openbook_props, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaOpenbookProps )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IJSON, ijson_iface_init ))

static void
openbook_props_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_openbook_props_finalize";
	ofaOpenbookPropsPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_OPENBOOK_PROPS( instance ));

	/* free data members here */
	priv = ofa_openbook_props_get_instance_private( OFA_OPENBOOK_PROPS( instance ));

	g_free( priv->openbook_version );

	g_list_free_full( priv->plugins, ( GDestroyNotify ) free_plugin );
	g_list_free_full( priv->dbmodels, ( GDestroyNotify ) free_dbmodel );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_openbook_props_parent_class )->finalize( instance );
}

static void
openbook_props_dispose( GObject *instance )
{
	ofaOpenbookPropsPrivate *priv;

	g_return_if_fail( instance && OFA_IS_OPENBOOK_PROPS( instance ));

	priv = ofa_openbook_props_get_instance_private( OFA_OPENBOOK_PROPS( instance ));

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
	}

	G_OBJECT_CLASS( ofa_openbook_props_parent_class )->dispose( instance );
}

static void
ofa_openbook_props_init( ofaOpenbookProps *self )
{
	static const gchar *thisfn = "ofa_openbook_props_init";
	ofaOpenbookPropsPrivate *priv;

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_OPENBOOK_PROPS( self ));

	priv = ofa_openbook_props_get_instance_private( self );

	priv->dispose_has_run = FALSE;
	priv->openbook_version = g_strdup( PACKAGE_VERSION );
}

static void
ofa_openbook_props_class_init( ofaOpenbookPropsClass *klass )
{
	static const gchar *thisfn = "ofa_openbook_props_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = openbook_props_dispose;
	G_OBJECT_CLASS( klass )->finalize = openbook_props_finalize;
}

/**
 * ofa_openbook_props_new:
 * @getter: an #ofaIGetter instance.
 *
 * Allocates and initializes a #ofaOpenbookProps object.
 *
 * Returns: a new #ofaOpenbookProps object.
 */
ofaOpenbookProps *
ofa_openbook_props_new( ofaIGetter *getter )
{
	ofaOpenbookProps *props;
	ofaExtenderCollection *extenders;
	ofaExtenderModule *plugin;
	const GList *modules, *itm;
	GList *dbmodels, *itb;
	gchar *canon, *display, *version, *id;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	props = g_object_new( OFA_TYPE_OPENBOOK_PROPS, NULL );

	extenders = ofa_igetter_get_extender_collection( getter );
	modules = ofa_extender_collection_get_modules( extenders );

	for( itm=modules ; itm ; itm=itm->next ){
		plugin = OFA_EXTENDER_MODULE( itm->data );
		canon = ofa_extender_module_get_canon_name( plugin );
		display = ofa_extender_module_get_display_name( plugin );
		version = ofa_extender_module_get_version( plugin );
		ofa_openbook_props_set_plugin( props, canon, display, version );
		g_free( version );
		g_free( display );
		g_free( canon );
	}

	dbmodels = ofa_igetter_get_for_type( getter, OFA_TYPE_IDBMODEL );

	for( itb=dbmodels ; itb ; itb=itb->next ){
		if( MY_IS_IIDENT( itb->data )){
			id = my_iident_get_canon_name( MY_IIDENT( itb->data ), NULL );
			version = my_iident_get_version( MY_IIDENT( itb->data ), NULL );
			ofa_openbook_props_set_dbmodel( props, id, version );
			g_free( version );
			g_free( id );
		}
	}

	g_list_free( dbmodels );

	return( props );
}

/**
 * ofa_openbook_props_new_from_string:
 * @getter: an #ofaIGetter instance.
 * @string: a JSON string.
 *
 * Try to parse the provided JSON string.
 *
 * Returns: a new #ofaOpenbookProps object if the header has been
 * successfully parsed, or %NULL.
 */
ofaOpenbookProps *
ofa_openbook_props_new_from_string( ofaIGetter *getter, const gchar *string )
{
	static const gchar *thisfn = "ofa_openbook_props_new_from_string";
	ofaOpenbookProps *props;
	JsonParser *parser;
	JsonNode *root;
	GError *error;

	g_return_val_if_fail( getter && OFA_IS_IGETTER( getter ), NULL );

	error = NULL;
	parser = json_parser_new();

	if( !json_parser_load_from_data( parser, string, -1, &error )){
		g_warning( "%s: json_parser_load_from_data: %s", thisfn, error->message );
		return( NULL );
	}

	root = json_parser_get_root( parser );
	props = new_from_node( getter, root );
	g_object_unref( parser );

	return( props );
}

static ofaOpenbookProps *
new_from_node( ofaIGetter *getter, JsonNode *root )
{
	static const gchar *thisfn = "ofa_openbook_props_new_from_node";
	ofaOpenbookProps *props;
	JsonNodeType root_type, node_type;
	JsonObject *object;
	JsonNode *node;
	JsonArray *array;
	GList *members, *itm;
	const gchar *cname, *cvalue;

	props = ofa_openbook_props_new( getter );
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

						if( !my_collate( cname, st_openbook )){
							ofa_openbook_props_set_openbook_version( props, cvalue );

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

static void
set_plugins_from_array( ofaOpenbookProps *self, JsonArray *array )
{
	static const gchar *thisfn = "ofa_openbook_props_set_plugins_from_array";
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
		ofa_openbook_props_set_plugin( self, canon, display, version );
		g_free( canon );
		g_free( display );
		g_free( version );
	}
}

static void
set_dbms_from_array( ofaOpenbookProps *self, JsonArray *array )
{
	static const gchar *thisfn = "ofa_openbook_props_set_dbms_from_array";
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
		ofa_openbook_props_set_dbmodel( self, id, version );
		g_free( id );
		g_free( version );
	}
}

/**
 * ofa_openbook_props_get_openbook_version:
 * @props: this #ofaOpenbookProps object.
 *
 * Returns: the openbook version at time of the backup.
 *
 * The returned string is owned by the @props object, and should not
 * be released by the caller.
 */
const gchar *
ofa_openbook_props_get_openbook_version( ofaOpenbookProps *props )
{
	ofaOpenbookPropsPrivate *priv;

	g_return_val_if_fail( props && OFA_IS_OPENBOOK_PROPS( props ), NULL );

	priv = ofa_openbook_props_get_instance_private( props );

	g_return_val_if_fail( !priv->dispose_has_run, NULL );

	return(( const gchar * ) priv->openbook_version );
}

/**
 * ofa_openbook_props_set_openbook_version:
 * @props: this #ofaOpenbookProps object.
 * @version: [allow-none]: the openbook version at time of the backup.
 *
 * Set the openbook version.
 *
 * The Openbook version defaults to the current version of the software.
 */
void
ofa_openbook_props_set_openbook_version( ofaOpenbookProps *props, const gchar *version )
{
	ofaOpenbookPropsPrivate *priv;

	g_return_if_fail( props && OFA_IS_OPENBOOK_PROPS( props ));

	priv = ofa_openbook_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	g_free( priv->openbook_version );
	priv->openbook_version = g_strdup( version );
}

/**
 * ofa_openbook_props_set_plugin:
 * @props: this #ofaOpenbookProps object.
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
ofa_openbook_props_set_plugin( ofaOpenbookProps *props, const gchar *canon_name, const gchar *display_name, const gchar *version )
{
	static const gchar *thisfn = "ofa_openbook_props_set_plugin";
	ofaOpenbookPropsPrivate *priv;
	sPlugin *sdata;

	g_debug( "%s: props=%p, canon_name=%s, display_name=%s, version=%s",
			thisfn, ( void * ) props, canon_name, display_name, version );

	g_return_if_fail( props && OFA_IS_OPENBOOK_PROPS( props ));

	priv = ofa_openbook_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = g_new0( sPlugin, 1 );
	sdata->canon_name = g_strdup( canon_name );
	sdata->display_name = g_strdup( display_name );
	sdata->version = g_strdup( version );

	priv->plugins = g_list_append( priv->plugins, sdata );
}

/**
 * ofa_openbook_props_set_dbmodel:
 * @props: this #ofaOpenbookProps object.
 * @id: [allow-none]: the identifier of the dbmodel.
 * @version: [allow-none]: the version of the dbmodel.
 *
 * Add these properties to the list of dbmodels.
 *
 * The code makes its best for keeping the dbmodels in the order they are
 * added.
 */
void
ofa_openbook_props_set_dbmodel( ofaOpenbookProps *props, const gchar *id, const gchar *version )
{
	static const gchar *thisfn = "ofa_openbook_props_set_dbmodel";
	ofaOpenbookPropsPrivate *priv;
	sDBModel *sdata;

	g_debug( "%s: props=%p, id=%s, version=%s",
			thisfn, ( void * ) props, id, version );

	g_return_if_fail( props && OFA_IS_OPENBOOK_PROPS( props ));

	priv = ofa_openbook_props_get_instance_private( props );

	g_return_if_fail( !priv->dispose_has_run );

	sdata = g_new0( sDBModel, 1 );
	sdata->id = g_strdup( id );
	sdata->version = g_strdup( version );

	priv->dbmodels = g_list_append( priv->dbmodels, sdata );
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
	ofaOpenbookPropsPrivate *priv;
	JsonBuilder *builder;
	JsonGenerator *generator;
	gchar *str;
	sPlugin *splugin;
	sDBModel *sdbmodel;
	GList *it;

	priv = ofa_openbook_props_get_instance_private( OFA_OPENBOOK_PROPS( instance ));

	builder = json_builder_new();
	json_builder_begin_object( builder );

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

	json_builder_end_object( builder );

	generator = json_generator_new();
	json_generator_set_root( generator, json_builder_get_root( builder ));
	str = json_generator_to_data( generator, NULL );

	g_object_unref( generator );
	g_object_unref( builder );

	return( str );
}
