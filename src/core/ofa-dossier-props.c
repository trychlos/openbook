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

#include "api/ofa-dossier-props.h"
#include "api/ofa-hub.h"
#include "api/ofa-ijson.h"
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
}
	ofaDossierPropsPrivate;

static const gchar *st_current          = "current";
static const gchar *st_begin            = "begin";
static const gchar *st_end              = "end";
static const gchar *st_rpid             = "rpid";

static const gchar *st_props_title      = "DossierProps";

static ofaDossierProps *new_from_node( JsonNode *node );
static void             ijson_iface_init( ofaIJsonInterface *iface );
static guint            ijson_get_interface_version( void );
static gchar           *ijson_get_title( void );
static gchar           *ijson_get_as_string( ofaIJson *instance );

G_DEFINE_TYPE_EXTENDED( ofaDossierProps, ofa_dossier_props, G_TYPE_OBJECT, 0,
		G_ADD_PRIVATE( ofaDossierProps )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IJSON, ijson_iface_init ))

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
 * ofa_dossier_props_new_from_dossier:
 * @dossier: the currently opened #ofoDossier.
 *
 * Returns: a new #ofaDossierProps object initialized with the datas
 * from @dossier.
 */
ofaDossierProps *
ofa_dossier_props_new_from_dossier( ofoDossier *dossier )
{
	ofaDossierProps *props;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	props = ofa_dossier_props_new();

	ofa_dossier_props_set_is_current( props, ofo_dossier_is_current( dossier ));
	ofa_dossier_props_set_begin_date( props, ofo_dossier_get_exe_begin( dossier ));
	ofa_dossier_props_set_end_date( props, ofo_dossier_get_exe_end( dossier ));
	ofa_dossier_props_set_rpid( props, ofo_dossier_get_rpid( dossier ));

	return( props );
}

/**
 * ofa_dossier_props_new_from_string:
 * @string: a JSON string.
 *
 * Try to parse the provided JSON string.
 *
 * Returns: a new #ofaDossierProps object if the header has been
 * successfully parsed, or %NULL.
 */
ofaDossierProps *
ofa_dossier_props_new_from_string( const gchar *string )
{
	static const gchar *thisfn = "ofa_dossier_props_new_from_string";
	ofaDossierProps *props;
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

static ofaDossierProps *
new_from_node( JsonNode *root )
{
	static const gchar *thisfn = "ofa_dossier_props_new_from_node";
	ofaDossierProps *props;
	JsonNodeType root_type, node_type;
	JsonObject *object;
	JsonNode *node;
	GList *members, *itm;
	const gchar *cname, *cvalue;
	GDate date;

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
	ofaDossierPropsPrivate *priv;
	JsonBuilder *builder;
	JsonGenerator *generator;
	gchar *sdate;
	gchar *str;

	priv = ofa_dossier_props_get_instance_private( OFA_DOSSIER_PROPS( instance ));

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

	json_builder_set_member_name( builder, st_rpid );
	json_builder_add_string_value( builder, priv->rpid );

	json_builder_end_object( builder );

	generator = json_generator_new();
	json_generator_set_root( generator, json_builder_get_root( builder ));
	str = json_generator_to_data( generator, NULL );

	g_object_unref( generator );
	g_object_unref( builder );

	return( str );
}
