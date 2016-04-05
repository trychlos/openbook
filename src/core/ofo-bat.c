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
#include <stdlib.h>
#include <string.h>

#include "my/my-date.h"
#include "my/my-double.h"
#include "my/my-utils.h"

#include "api/ofa-amount.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollectionable.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbconnect.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-bat.h"
#include "api/ofo-bat-line.h"

/* priv instance data
 */
typedef struct {

	/* dbms data
	 */
	ofxCounter id;						/* bat (imported file) id */
	gchar     *uri;
	gchar     *format;
	GDate      begin;
	GDate      end;
	gchar     *rib;
	gchar     *currency;
	ofxAmount  begin_solde;
	gboolean   begin_solde_set;
	ofxAmount  end_solde;
	gboolean   end_solde_set;
	gchar     *notes;
	gchar     *account;
	gchar     *upd_user;
	GTimeVal   upd_stamp;
}
	ofoBatPrivate;

static ofoBat     *bat_find_by_id( GList *set, ofxCounter id );
static void        bat_set_id( ofoBat *bat, ofxCounter id );
static void        bat_set_upd_user( ofoBat *bat, const gchar *upd_user );
static void        bat_set_upd_stamp( ofoBat *bat, const GTimeVal *upd_stamp );
static gboolean    bat_do_insert( ofoBat *bat, ofaHub *hub );
static gboolean    bat_insert_main( ofoBat *bat, ofaHub *hub );
static gboolean    bat_do_update( ofoBat *bat, const ofaIDBConnect *connect );
static gboolean    bat_do_delete_by_where( ofoBat *bat, const ofaIDBConnect *connect );
static gboolean    bat_do_delete_main( ofoBat *bat, const ofaIDBConnect *connect );
static gboolean    bat_do_delete_lines( ofoBat *bat, const ofaIDBConnect *connect );
static gint        bat_cmp_by_date_desc( const ofoBat *a, const ofoBat *b );
static gint        bat_cmp_by_id( const ofoBat *a, ofxCounter id );
static gint        bat_cmp_by_ptr( const ofoBat *a, const ofoBat *b );
static void        icollectionable_iface_init( ofaICollectionableInterface *iface );
static guint       icollectionable_get_interface_version( void );
static GList      *icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub );
static void        iimportable_iface_init( ofaIImportableInterface *iface );
static guint       iimportable_get_interface_version( void );
static gchar      *iimportable_get_label( const ofaIImportable *instance );
static guint       iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static GList      *iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines );
static ofoBat     *iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static ofoBatLine *iimportable_import_parse_line( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields );
static void        iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset );
static gboolean    bat_get_exists( const ofoBat *bat, const ofaIDBConnect *connect );
static gchar      *bat_get_where( const ofoBat *bat );
static ofxCounter  bat_get_id_by_where( const ofoBat *bat, const ofaIDBConnect *connect );
static gboolean    bat_drop_content( const ofaIDBConnect *connect );

G_DEFINE_TYPE_EXTENDED( ofoBat, ofo_bat, OFO_TYPE_BASE, 0,
		G_ADD_PRIVATE( ofoBat )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTIONABLE, icollectionable_iface_init )
		G_IMPLEMENT_INTERFACE( OFA_TYPE_IIMPORTABLE, iimportable_iface_init ))

static void
bat_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_bat_finalize";
	ofoBatPrivate *priv;

	priv = ofo_bat_get_instance_private( OFO_BAT( instance ));

	g_debug( "%s: instance=%p (%s): %s",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ), priv->uri );

	g_return_if_fail( instance && OFO_IS_BAT( instance ));

	/* free data members here */
	g_free( priv->uri );
	g_free( priv->format );
	g_free( priv->rib );
	g_free( priv->currency );
	g_free( priv->notes );
	g_free( priv->account );
	g_free( priv->upd_user );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->finalize( instance );
}

static void
bat_dispose( GObject *instance )
{
	g_return_if_fail( instance && OFO_IS_BAT( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_bat_parent_class )->dispose( instance );
}

static void
ofo_bat_init( ofoBat *self )
{
	static const gchar *thisfn = "ofo_bat_init";
	ofoBatPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	priv = ofo_bat_get_instance_private( self );

	priv->id = OFO_BASE_UNSET_ID;
	my_date_clear( &priv->begin );
	my_date_clear( &priv->end );
}

static void
ofo_bat_class_init( ofoBatClass *klass )
{
	static const gchar *thisfn = "ofo_bat_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = bat_dispose;
	G_OBJECT_CLASS( klass )->finalize = bat_finalize;
}

/**
 * ofo_bat_connect_to_hub_signaling_system:
 * @hub: the current #ofaHub object.
 *
 * Connect to the @hub signaling system.
 */
void
ofo_bat_connect_to_hub_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofo_bat_connect_to_hub_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	g_return_if_fail( hub && OFA_IS_HUB( hub ));
}

/**
 * ofo_bat_get_dataset:
 * @hub: the current #ofaHub object.
 *
 * Returns: the full #ofoBat dataset.
 *
 * The returned list is owned by the @hub collector, and should not
 * be released by the caller.
 */
GList *
ofo_bat_get_dataset( ofaHub *hub )
{
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	return( ofa_icollector_get_collection( OFA_ICOLLECTOR( hub ), hub, OFO_TYPE_BAT ));
}

/**
 * ofo_bat_get_by_id:
 * @hub: the current #ofaHub object.
 *
 * Returns: the searched BAT object, or %NULL.
 *
 * The returned object is owned by the #ofoBat class, and should
 * not be unreffed by the caller.
 */
ofoBat *
ofo_bat_get_by_id( ofaHub *hub, ofxCounter id )
{
	GList *dataset;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( id > 0, NULL );

	dataset = ofo_bat_get_dataset( hub );

	return( bat_find_by_id( dataset, id ));
}

static ofoBat *
bat_find_by_id( GList *set, ofxCounter id )
{
	GList *found;

	found = g_list_find_custom( set, ( gconstpointer ) id, ( GCompareFunc ) bat_cmp_by_id );
	if( found ){
		return( OFO_BAT( found->data ));
	}

	return( NULL );
}

/**
 * ofo_bat_get_most_recent_for_account:
 * @hub: the current #ofaHub object.
 * @account_id: the searched account identifier.
 *
 * Returns: the searched BAT object, or %NULL.
 *
 * The returned object is owned by the #ofoBat class, and should
 * not be unreffed by the caller.
 */
ofoBat *
ofo_bat_get_most_recent_for_account( ofaHub *hub, const gchar *account_id )
{
	GList *dataset, *acc_list, *it;
	ofoBat *bat;
	const gchar *bat_account;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );
	g_return_val_if_fail( my_strlen( account_id ), NULL );

	dataset = ofo_bat_get_dataset( hub );

	acc_list = NULL;
	for( it=dataset ; it ; it=it->next ){
		bat = OFO_BAT( it->data );
		bat_account = ofo_bat_get_account( bat );
		if( bat_account && !my_collate( bat_account, account_id )){
			acc_list = g_list_insert_sorted( acc_list, bat, ( GCompareFunc ) bat_cmp_by_date_desc );
		}
	}

	bat = acc_list ? ( ofoBat * ) acc_list->data : NULL;

	return( bat );
}

/**
 * ofo_bat_new:
 */
ofoBat *
ofo_bat_new( void )
{
	ofoBat *bat;

	bat = g_object_new( OFO_TYPE_BAT, NULL );

	return( bat );
}

/**
 * ofo_bat_get_id:
 */
ofxCounter
ofo_bat_get_id( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), OFO_BASE_UNSET_ID );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, OFO_BASE_UNSET_ID );

	priv = ofo_bat_get_instance_private( bat );

	return( priv->id );
}

/**
 * ofo_bat_get_uri:
 */
const gchar *
ofo_bat_get_uri( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->uri );
}

/**
 * ofo_bat_get_format:
 */
const gchar *
ofo_bat_get_format( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->format );
}

/**
 * ofo_bat_get_begin_date:
 */
const GDate *
ofo_bat_get_begin_date( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const GDate * ) &priv->begin );
}

/**
 * ofo_bat_get_begin_solde:
 */
ofxAmount
ofo_bat_get_begin_solde( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	priv = ofo_bat_get_instance_private( bat );

	return( priv->begin_solde );
}

/**
 * ofo_bat_get_begin_solde_set:
 */
gboolean
ofo_bat_get_begin_solde_set( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	priv = ofo_bat_get_instance_private( bat );

	return( priv->begin_solde_set );
}

/**
 * ofo_bat_get_end_date:
 */
const GDate *
ofo_bat_get_end_date( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const GDate * ) &priv->end );
}

/**
 * ofo_bat_get_end_solde:
 */
ofxAmount
ofo_bat_get_end_solde( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	priv = ofo_bat_get_instance_private( bat );

	return( priv->end_solde );
}

/**
 * ofo_bat_get_end_solde_set:
 */
gboolean
ofo_bat_get_end_solde_set( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	priv = ofo_bat_get_instance_private( bat );

	return( priv->end_solde_set );
}

/**
 * ofo_bat_get_rib:
 */
const gchar *
ofo_bat_get_rib( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->rib );
}

/**
 * ofo_bat_get_currency:
 */
const gchar *
ofo_bat_get_currency( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->currency );
}

/**
 * ofo_bat_get_notes:
 */
const gchar *
ofo_bat_get_notes( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->notes );
}

/**
 * ofo_bat_get_account:
 */
const gchar *
ofo_bat_get_account( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->account );
}

/**
 * ofo_bat_get_upd_user:
 */
const gchar *
ofo_bat_get_upd_user( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const gchar * ) priv->upd_user );
}

/**
 * ofo_bat_get_upd_stamp:
 */
const GTimeVal *
ofo_bat_get_upd_stamp( const ofoBat *bat )
{
	ofoBatPrivate *priv;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), NULL );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, NULL );

	priv = ofo_bat_get_instance_private( bat );

	return(( const GTimeVal * ) &priv->upd_stamp );
}

/**
 * ofo_bat_exists:
 * @hub: the current #ofaHub object.
 * @rib:
 * @begin:
 * @end:
 *
 * Returns %TRUE if the Bank Account Transaction file has already been
 * imported, and display a message dialog box in this case.
 */
gboolean
ofo_bat_exists( const ofaHub *hub, const gchar *rib, const GDate *begin, const GDate *end )
{
	gboolean exists;
	gchar *sbegin, *send;
	GString *query;
	gint count;
	gchar *primary, *secondary;
	GtkWidget *dialog;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	exists = FALSE;

	sbegin = my_date_to_str( begin, MY_DATE_SQL );
	send = my_date_to_str( end, MY_DATE_SQL );

	query = g_string_new( "SELECT COUNT(*) FROM OFA_T_BAT WHERE ");

	g_string_append_printf( query, "BAT_RIB='%s' ", rib );

	if( my_date_is_valid( begin )){
		g_string_append_printf( query, "AND BAT_BEGIN='%s' ", sbegin );
	} else {
		g_string_append_printf( query, "AND BAT_BEGIN IS NULL " );
	}

	if( my_date_is_valid( end )){
		g_string_append_printf( query, "AND BAT_END='%s'", send );
	} else {
		g_string_append_printf( query, "AND BAT_END IS NULL" );
	}

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query->str, &count, TRUE );

	if( count > 0 ){
		exists = TRUE;
		primary = g_strdup(
				_( "The candidate Bank Account Transaction file has already been imported.\n"
						"A new import is refused." ));
		secondary = g_strdup_printf(
				_( "\tRIB\t\t= '%s'\n"
						"\tBegin\t= '%s'\n"
						"\tEnd\t\t= '%s'" ), rib, sbegin, send );
		dialog = gtk_message_dialog_new(
						NULL,
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_WARNING,
						GTK_BUTTONS_CLOSE,
						"%s", primary );
		gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dialog ), "%s", secondary );
		gtk_dialog_run( GTK_DIALOG( dialog ));
		gtk_widget_destroy( dialog );
		g_free( secondary );
		g_free( primary );
	}

	g_string_free( query, TRUE );
	g_free( send );
	g_free( sbegin );

	return( exists );
}

/**
 * ofo_bat_is_deletable:
 *
 * An imported BAT file may be removed from the database if none of its
 * lines has been yet reconciliated.
 */
gboolean
ofo_bat_is_deletable( const ofoBat *bat )
{
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	count = ofo_bat_get_used_count( bat );

	return( count == 0 );
}

/**
 * ofo_bat_get_lines_count:
 * @bat: this #ofoBat instance.
 *
 * Returns: the count of lines in this BAT.
 */
gint
ofo_bat_get_lines_count( const ofoBat *bat )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	query = g_strdup_printf(
					"SELECT COUNT(*) FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld",
					ofo_bat_get_id( bat ));

	hub = ofo_base_get_hub( OFO_BASE( bat ));
	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );
	g_free( query );

	return( count );
}

/**
 * ofo_bat_get_used_count:
 * @bat: this #ofoBat instance.
 *
 * Returns the count of used lines from this BAT file, i.e. the count
 * of lines which belong to a conciliation group.
 */
gint
ofo_bat_get_used_count( const ofoBat *bat )
{
	gchar *query;
	ofaHub *hub;
	gint count;

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), 0 );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, 0 );

	query = g_strdup_printf(
			"SELECT COUNT(*) FROM OFA_T_CONCIL_IDS WHERE "
			"	REC_IDS_TYPE='%s' AND REC_IDS_OTHER IN "
			"		(SELECT BAT_LINE_ID FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld)",
			CONCIL_TYPE_BAT, ofo_bat_get_id( bat ));

	hub = ofo_base_get_hub( OFO_BASE( bat ));
	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );
	g_free( query );

	return( count );
}

/*
 * bat_set_id:
 */
static void
bat_set_id( ofoBat *bat, ofxCounter id )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	priv->id = id;
}

/**
 * ofo_bat_set_uri:
 */
void
ofo_bat_set_uri( ofoBat *bat, const gchar *uri )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->uri );
	priv->uri = g_strdup( uri );
}

/**
 * ofo_bat_set_format:
 */
void
ofo_bat_set_format( ofoBat *bat, const gchar *format )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->format );
	priv->format = g_strdup( format );
}

/**
 * ofo_bat_set_begin_date:
 */
void
ofo_bat_set_begin_date( ofoBat *bat, const GDate *date )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	my_date_set_from_date( &priv->begin, date );
}

/**
 * ofo_bat_set_begin_solde:
 */
void
ofo_bat_set_begin_solde( ofoBat *bat, ofxAmount solde )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	priv->begin_solde = solde;
}

/**
 * ofo_bat_set_begin_solde_set:
 */
void
ofo_bat_set_begin_solde_set( ofoBat *bat, gboolean set )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	priv->begin_solde_set = set;
}

/**
 * ofo_bat_set_end_date:
 */
void
ofo_bat_set_end_date( ofoBat *bat, const GDate *date )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	my_date_set_from_date( &priv->end, date );
}

/**
 * ofo_bat_set_end_solde:
 */
void
ofo_bat_set_end_solde( ofoBat *bat, ofxAmount solde )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	priv->end_solde = solde;
}

/**
 * ofo_bat_set_end_solde_set:
 */
void
ofo_bat_set_end_solde_set( ofoBat *bat, gboolean set )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	priv->end_solde_set = set;
}

/**
 * ofo_bat_set_rib:
 */
void
ofo_bat_set_rib( ofoBat *bat, const gchar *rib )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->rib );
	priv->rib = g_strdup( rib );
}

/**
 * ofo_bat_set_currency:
 */
void
ofo_bat_set_currency( ofoBat *bat, const gchar *currency )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->currency );
	priv->currency = g_strdup( currency );
}

/**
 * ofo_bat_set_notes:
 */
void
ofo_bat_set_notes( ofoBat *bat, const gchar *notes )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->notes );
	priv->notes = g_strdup( notes );
}

/**
 * ofo_bat_set_account:
 */
void
ofo_bat_set_account( ofoBat *bat, const gchar *account )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->account );
	priv->account = g_strdup( account );
}

/*
 * ofo_bat_set_upd_user:
 */
static void
bat_set_upd_user( ofoBat *bat, const gchar *upd_user )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	g_free( priv->upd_user );
	priv->upd_user = g_strdup( upd_user );
}

/*
 * ofo_bat_set_upd_stamp:
 */
static void
bat_set_upd_stamp( ofoBat *bat, const GTimeVal *upd_stamp )
{
	ofoBatPrivate *priv;

	g_return_if_fail( bat && OFO_IS_BAT( bat ));
	g_return_if_fail( !OFO_BASE( bat )->prot->dispose_has_run );

	priv = ofo_bat_get_instance_private( bat );

	my_utils_stamp_set_from_stamp( &priv->upd_stamp, upd_stamp );
}

/**
 * ofo_bat_insert:
 * @bat: a new #ofoBat instance to be added to the database.
 * @hub: the current #ofaHub object.
 *
 * Insert the new object in the database, updating simultaneously the
 * global dataset.
 *
 * Returns: %TRUE if insertion is successful, %FALSE else.
 */
gboolean
ofo_bat_insert( ofoBat *bat, ofaHub *hub )
{
	static const gchar *thisfn = "ofo_bat_insert";
	ofoDossier *dossier;
	gboolean ok;

	g_debug( "%s: bat=%p, hub=%p",
			thisfn, ( void * ) bat, ( void * ) hub );

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	ok = FALSE;

	dossier = ofa_hub_get_dossier( hub );
	bat_set_id( bat, ofo_dossier_get_next_bat( dossier ));

	if( bat_do_insert( bat, hub )){
		ofo_base_set_hub( OFO_BASE( bat ), hub );
		ofa_icollector_add_object(
				OFA_ICOLLECTOR( hub ), hub, OFA_ICOLLECTIONABLE( bat ), ( GCompareFunc ) bat_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_NEW, bat );
		ok = TRUE;
	}

	return( ok );
}

static gboolean
bat_do_insert( ofoBat *bat, ofaHub *hub )
{
	return( bat_insert_main( bat, hub ));
}

static gboolean
bat_insert_main( ofoBat *bat, ofaHub *hub )
{
	GString *query;
	gchar *suri, *str, *userid;
	const GDate *begin, *end;
	gboolean ok;
	gchar *stamp_str;
	GTimeVal stamp;
	const gchar *cur_code;
	ofoCurrency *cur_obj;
	const ofaIDBConnect *connect;

	cur_code = ofo_bat_get_currency( bat );
	cur_obj = my_strlen( cur_code ) ? ofo_currency_get_by_code( hub, cur_code ) : NULL;
	g_return_val_if_fail( !cur_obj || OFO_IS_CURRENCY( cur_obj ), FALSE );

	connect = ofa_hub_get_connect( hub );

	ok = FALSE;
	my_utils_stamp_set_now( &stamp );
	suri = my_utils_quote_single( ofo_bat_get_uri( bat ));
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "INSERT INTO OFA_T_BAT" );

	g_string_append_printf( query,
			"	(BAT_ID,BAT_URI,BAT_FORMAT,BAT_BEGIN,BAT_END,"
			"	 BAT_RIB,BAT_CURRENCY,BAT_SOLDE_BEGIN,BAT_SOLDE_END,"
			"	 BAT_NOTES,BAT_UPD_USER,BAT_UPD_STAMP) VALUES (%ld,'%s',",
					ofo_bat_get_id( bat ),
					suri );

	g_free( suri );

	str = my_utils_quote_single( ofo_bat_get_format( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	begin = ofo_bat_get_begin_date( bat );
	if( my_date_is_valid( begin )){
		str = my_date_to_str( begin, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	end = ofo_bat_get_end_date( bat );
	if( my_date_is_valid( end )){
		str = my_date_to_str( end, MY_DATE_SQL );
		g_string_append_printf( query, "'%s',", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = ( gchar * ) ofo_bat_get_rib( bat );
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( my_strlen( cur_code )){
		g_string_append_printf( query, "'%s',", cur_code );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_bat_get_begin_solde_set( bat )){
		str = ofa_amount_to_sql( ofo_bat_get_begin_solde( bat ), cur_obj );
		g_string_append_printf( query, "%s,", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	if( ofo_bat_get_end_solde_set( bat )){
		str = ofa_amount_to_sql( ofo_bat_get_end_solde( bat ), cur_obj );
		g_string_append_printf( query, "%s,", str );
		g_free( str );
	} else {
		query = g_string_append( query, "NULL," );
	}

	str = my_utils_quote_single( ofo_bat_get_notes( bat ));
	if( my_strlen( str )){
		g_string_append_printf( query, "'%s',", str );
	} else {
		query = g_string_append( query, "NULL," );
	}
	g_free( str );

	g_string_append_printf( query, "'%s','%s')", userid, stamp_str );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		bat_set_upd_user( bat, userid );
		bat_set_upd_stamp( bat, &stamp );
		ok = TRUE;
	}

	g_free( userid );
	g_string_free( query, TRUE );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_update:
 */
gboolean
ofo_bat_update( ofoBat *bat )
{
	static const gchar *thisfn = "ofo_bat_update";
	ofaHub *hub;
	gboolean ok;

	g_debug( "%s: bat=%p", thisfn, ( void * ) bat );

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( bat ));

	if( bat_do_update( bat, ofa_hub_get_connect( hub ))){
		ofa_icollector_sort_collection(
				OFA_ICOLLECTOR( hub ), OFO_TYPE_BAT, ( GCompareFunc ) bat_cmp_by_ptr );
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, bat, NULL );
		ok = TRUE;
	}

	return( ok );
}

/*
 * only notes may be updated
 */
static gboolean
bat_do_update( ofoBat *bat, const ofaIDBConnect *connect )
{
	GString *query;
	gchar *notes, *userid;
	gboolean ok;
	GTimeVal stamp;
	gchar *stamp_str;
	const gchar *caccount;

	ok = FALSE;
	notes = my_utils_quote_single( ofo_bat_get_notes( bat ));
	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_BAT SET " );

	if( my_strlen( notes )){
		g_string_append_printf( query, "BAT_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "BAT_NOTES=NULL," );
	}

	caccount = ofo_bat_get_account( bat );
	if( my_strlen( caccount )){
		g_string_append_printf( query, "BAT_ACCOUNT='%s',", caccount );
	} else {
		query = g_string_append( query, "BAT_ACCOUNT=NULL," );
	}

	g_string_append_printf( query,
			"	BAT_UPD_USER='%s',BAT_UPD_STAMP='%s'"
			"	WHERE BAT_ID=%ld", userid, stamp_str, ofo_bat_get_id( bat ));

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		bat_set_upd_user( bat, userid );
		bat_set_upd_stamp( bat, &stamp );
		ok = TRUE;
	}

	g_free( userid );
	g_string_free( query, TRUE );
	g_free( notes );
	g_free( stamp_str );

	return( ok );
}

/**
 * ofo_bat_delete:
 */
gboolean
ofo_bat_delete( ofoBat *bat )
{
	static const gchar *thisfn = "ofo_bat_delete";
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gboolean ok;

	g_debug( "%s: bat=%p", thisfn, ( void * ) bat );

	g_return_val_if_fail( bat && OFO_IS_BAT( bat ), FALSE );
	g_return_val_if_fail( ofo_bat_is_deletable( bat ), FALSE );
	g_return_val_if_fail( !OFO_BASE( bat )->prot->dispose_has_run, FALSE );

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( bat ));
	connect = ofa_hub_get_connect( hub );

	if( bat_do_delete_main( bat, connect ) &&  bat_do_delete_lines( bat, connect )){
		g_object_ref( bat );
		ofa_icollector_remove_object( OFA_ICOLLECTOR( hub ), OFA_ICOLLECTIONABLE( bat ));
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_DELETED, bat );
		g_object_unref( bat );
		ok = TRUE;
	}

	return( ok );
}

/*
 * when the ofoBat does not yet have an identifier
 */
static gboolean
bat_do_delete_by_where( ofoBat *bat, const ofaIDBConnect *connect )
{
	ofxCounter bat_id;
	gchar *query;
	gboolean ok;

	ok = TRUE;

	bat_id = bat_get_id_by_where( bat, connect );

	if( bat_id > 0 ){
		query = g_strdup_printf( "DELETE FROM OFA_T_BAT WHERE BAT_ID=%ld", bat_id );
		ok &= ofa_idbconnect_query( connect, query, FALSE );
		g_free( query );

		query = g_strdup_printf( "DELETE FROM OFA_T_BAT_LINES WHERE BAT_ID=%ld", bat_id );
		ok &= ofa_idbconnect_query( connect, query, FALSE );
		g_free( query );
	}

	return( ok );
}

static gboolean
bat_do_delete_main( ofoBat *bat, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_BAT"
			"	WHERE BAT_ID=%ld",
					ofo_bat_get_id( bat ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gboolean
bat_do_delete_lines( ofoBat *bat, const ofaIDBConnect *connect )
{
	gchar *query;
	gboolean ok;

	query = g_strdup_printf(
			"DELETE FROM OFA_T_BAT_LINES"
			"	WHERE BAT_ID=%ld",
					ofo_bat_get_id( bat ));

	ok = ofa_idbconnect_query( connect, query, TRUE );

	g_free( query );

	return( ok );
}

static gint
bat_cmp_by_date_desc( const ofoBat *a, const ofoBat *b )
{
	const GDate *a_end, *b_end;

	a_end = ofo_bat_get_end_date( a );
	b_end = ofo_bat_get_end_date( b );

	return( -1*my_date_compare_ex( a_end, b_end, TRUE ));
}

static gint
bat_cmp_by_id( const ofoBat *a, ofxCounter id )
{
	ofxCounter a_id;

	a_id = ofo_bat_get_id( a );

	return( a_id < id ? -1 : ( a_id > id ? 1 : 0 ));
}

static gint
bat_cmp_by_ptr( const ofoBat *a, const ofoBat *b )
{
	gint id_a, id_b;

	id_a = ofo_bat_get_id( a );
	id_b = ofo_bat_get_id( b );

	return( id_a > id_b ? 1 : ( id_a < id_b ? -1 : 0 ));
}

/**
 * ofo_bat_import:
 * @importable:
 * @sbat:
 * @hub: the current #ofaHub object.
 *
 * Import the provided #ofsBat structure.
 */
gboolean
ofo_bat_import( ofaIImportable *importable, ofsBat *sbat, ofaHub *hub, ofxCounter *id )
{
	gboolean ok;
	ofoBat *bat;
	ofoBatLine *bline;
	ofsBatDetail *sdet;
	GList *it;

	g_return_val_if_fail( importable && OFA_IS_IIMPORTABLE( importable ), FALSE );
	g_return_val_if_fail( sbat, FALSE );
	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), FALSE );

	if( id ){
		*id = 0;
	}

	bat = ofo_bat_new();

	ofo_bat_set_uri( bat, sbat->uri );
	ofo_bat_set_format( bat, sbat->format );
	ofo_bat_set_begin_date( bat, &sbat->begin );
	ofo_bat_set_end_date( bat, &sbat->end );
	ofo_bat_set_rib( bat, sbat->rib );
	ofo_bat_set_currency( bat, sbat->currency );
	ofo_bat_set_begin_solde( bat, sbat->begin_solde );
	ofo_bat_set_begin_solde_set( bat, sbat->begin_solde_set );
	ofo_bat_set_end_solde( bat, sbat->end_solde );
	ofo_bat_set_end_solde_set( bat, sbat->end_solde_set );

	ok = ofo_bat_insert( bat, hub );
	if( ok ){
		for( it=sbat->details ; it ; it=it->next ){
			sdet = ( ofsBatDetail * ) it->data;
			bline = ofo_bat_line_new();

			ofo_bat_line_set_bat_id( bline, ofo_bat_get_id( bat ));
			ofo_bat_line_set_deffect( bline, &sdet->deffect );
			ofo_bat_line_set_dope( bline, &sdet->dope );
			ofo_bat_line_set_ref( bline, sdet->ref );
			ofo_bat_line_set_label( bline, sdet->label );
			ofo_bat_line_set_currency( bline, sdet->currency );
			ofo_bat_line_set_amount( bline, sdet->amount );

			ok &= ofo_bat_line_insert( bline, hub );
			ofa_iimportable_increment_progress( importable, IMPORTABLE_PHASE_INSERT, 1 );
			g_object_unref( bline );
		}
	}

	if( ok ){
		if( id ){
			*id = ofo_bat_get_id( bat );
		}
		g_signal_emit_by_name( G_OBJECT( hub ), SIGNAL_HUB_UPDATED, bat, NULL );
	}

	/* do not g_object_unref() the newly created BAT as the ownership
	 * has been given to the global dataset */

	return( ok );
}

/*
 * ofaICollectionable interface management
 */
static void
icollectionable_iface_init( ofaICollectionableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_icollectionable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollectionable_get_interface_version;
	iface->load_collection = icollectionable_load_collection;
}

static guint
icollectionable_get_interface_version( void )
{
	return( 1 );
}

static GList *
icollectionable_load_collection( const ofaICollectionable *instance, ofaHub *hub )
{
	const ofaIDBConnect *connect;
	GSList *result, *irow, *icol;
	ofoBat *bat;
	GList *dataset;
	GTimeVal timeval;
	GDate date;

	dataset = NULL;
	connect = ofa_hub_get_connect( hub );

	if( ofa_idbconnect_query_ex( connect,
			"SELECT BAT_ID,BAT_URI,BAT_FORMAT,BAT_ACCOUNT,"
			"	BAT_BEGIN,BAT_END,BAT_RIB,BAT_CURRENCY,BAT_SOLDE_BEGIN,BAT_SOLDE_END,"
			"	BAT_NOTES,BAT_UPD_USER,BAT_UPD_STAMP "
			"	FROM OFA_T_BAT "
			"	ORDER BY BAT_UPD_STAMP ASC", &result, TRUE )){

		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			bat = ofo_bat_new();
			bat_set_id( bat, atol(( gchar * ) icol->data ));
			icol = icol->next;
			ofo_bat_set_uri( bat, ( gchar * ) icol->data );
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_format( bat, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_account( bat, ( const gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				my_date_set_from_sql( &date, ( const gchar * ) icol->data );
				ofo_bat_set_begin_date( bat, &date );
			}
			icol = icol->next;
			if( icol->data ){
				my_date_set_from_sql( &date, ( const gchar * ) icol->data );
				ofo_bat_set_end_date( bat, &date );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_rib( bat, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_currency( bat, ( gchar * ) icol->data );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_begin_solde( bat,
						my_double_set_from_sql(( const gchar * ) icol->data ));
				ofo_bat_set_begin_solde_set( bat, TRUE );
			} else {
				ofo_bat_set_begin_solde_set( bat, FALSE );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_end_solde( bat,
						my_double_set_from_sql(( const gchar * ) icol->data ));
				ofo_bat_set_end_solde_set( bat, TRUE );
			} else {
				ofo_bat_set_end_solde_set( bat, FALSE );
			}
			icol = icol->next;
			if( icol->data ){
				ofo_bat_set_notes( bat, ( gchar * ) icol->data );
			}
			icol = icol->next;
			bat_set_upd_user( bat, ( gchar * ) icol->data );
			icol = icol->next;
			bat_set_upd_stamp( bat,
					my_utils_stamp_set_from_sql( &timeval, ( const gchar * ) icol->data ));

			ofo_base_set_hub( OFO_BASE( bat ), hub );
			dataset = g_list_prepend( dataset, bat );
		}

		ofa_idbconnect_free_results( result );
	}

	return( g_list_reverse( dataset ));
}

/*
 * ofaIImportable interface management
 */
static void
iimportable_iface_init( ofaIImportableInterface *iface )
{
	static const gchar *thisfn = "ofo_account_iimportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iimportable_get_interface_version;
	iface->get_label = iimportable_get_label;
	iface->import = iimportable_import;
}

static guint
iimportable_get_interface_version( void )
{
	return( 1 );
}

static gchar *
iimportable_get_label( const ofaIImportable *instance )
{
	return( g_strdup( _( "_Bank account transaction list" )));
}

/*
 * ofo_bat_iimportable_import:
 *
 * Receives a GSList of lines, where data are GSList of fields.
 *
 * Returns: the total count of errors.
 *
 * As the table may have been dropped between import phase and insert
 * phase, if an error occurs during insert phase, then the table is
 * changed and only contains the successfully inserted records.
 */
static guint
iimportable_import( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;

	dataset = iimportable_import_parse( importer, parms, lines );

	if( parms->parse_errs == 0 && parms->parsed_count > 0 ){
		iimportable_import_insert( importer, parms, dataset );

		if( parms->insert_errs == 0 ){
			ofa_icollector_free_collection( OFA_ICOLLECTOR( parms->hub ), OFO_TYPE_BAT );
			g_signal_emit_by_name( G_OBJECT( parms->hub ), SIGNAL_HUB_RELOAD, OFO_TYPE_BAT );
		}
	}

	if( dataset ){
		g_list_free_full( dataset, ( GDestroyNotify ) g_object_unref );
	}

	return( parms->parse_errs+parms->insert_errs );
}

/*
 * parse to a dataset
 */
static GList *
iimportable_import_parse( ofaIImporter *importer, ofsImporterParms *parms, GSList *lines )
{
	GList *dataset;
	GSList *itl, *fields, *itf;
	const gchar *cstr;
	guint numline, total, type;
	gchar *str;
	ofoBat *bat;
	ofoBatLine *line;

	numline = 0;
	dataset = NULL;
	total = g_slist_length( lines );

	ofa_iimporter_progress_start( importer, parms );
	//my_utils_dump_gslist( lines );

	for( itl=lines ; itl ; itl=itl->next ){

		if( parms->stop && parms->parse_errs > 0 ){
			break;
		}

		numline += 1;
		fields = ( GSList * ) itl->data;

		/* line type */
		itf = fields;
		cstr = itf ? ( const gchar * ) itf->data : NULL;
		type = cstr ? atoi( cstr ) : 0;

		switch( type ){
			case 1:
				bat = iimportable_import_parse_main( importer, parms, numline, itf );
				if( bat ){
					dataset = g_list_prepend( dataset, bat );
					parms->parsed_count += 1;
				}
				break;
			case 2:
				line = iimportable_import_parse_line( importer, parms, numline, itf );
				if( line ){
					dataset = g_list_prepend( dataset, line );
					parms->parsed_count += 1;
				}
				break;
			default:
				str = g_strdup_printf( _( "invalid line type: %s" ), cstr );
				ofa_iimporter_progress_num_text( importer, parms, numline, str );
				g_free( str );
				parms->parse_errs += 1;
				total -= 1;
				continue;
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->parsed_count, ( gulong ) total );
	}

	return( g_list_reverse( dataset ));
}

static ofoBat *
iimportable_import_parse_main( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr;
	GSList *itf;
	ofoBat *bat;
	GDate date;

	bat = ofo_bat_new();

	/* uri */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_uri( bat, cstr );
	}

	/* importer label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_format( bat, cstr );
	}

	/* rib */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_rib( bat, cstr );
	}

	/* currency */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_currency( bat, cstr );
	}

	/* begin date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_begin_date( bat, my_date_set_from_str( &date, cstr, ofa_stream_format_get_date_format( parms->format )));
	}

	/* begin solde */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_begin_solde( bat, my_double_set_from_str( cstr,
				ofa_stream_format_get_thousand_sep( parms->format ),
				ofa_stream_format_get_decimal_sep( parms->format )));
	}

	/* begin solde set */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_begin_solde_set( bat, my_utils_boolean_from_str( cstr ));
	}

	/* end date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_end_date( bat, my_date_set_from_str( &date, cstr, ofa_stream_format_get_date_format( parms->format )));
	}

	/* end solde */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_end_solde( bat, my_double_set_from_str( cstr,
				ofa_stream_format_get_thousand_sep( parms->format ),
				ofa_stream_format_get_decimal_sep( parms->format ) ));
	}

	/* end solde set */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_set_end_solde_set( bat, my_utils_boolean_from_str( cstr ));
	}

	return( bat );
}

static ofoBatLine *
iimportable_import_parse_line( ofaIImporter *importer, ofsImporterParms *parms, guint numline, GSList *fields )
{
	const gchar *cstr, *sref, *slabel;
	GSList *itf;
	GDate dope, deffect;
	ofoBatLine *batline;
	ofxAmount amount;

	batline = ofo_bat_line_new();

	sref = NULL;
	slabel = NULL;
	my_date_clear( &dope );
	my_date_clear( &deffect );

	/* operation date */
	itf = fields ? fields->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_dope( batline, my_date_set_from_str( &dope, cstr, ofa_stream_format_get_date_format( parms->format )));
	}

	/* effect date */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_deffect( batline, my_date_set_from_str( &deffect, cstr, ofa_stream_format_get_date_format( parms->format )));
	}

	/* effect date is mandatory */
	if( !my_date_is_valid( &deffect )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "effect date is not set" ));
		parms->parse_errs += 1;
		g_object_unref( batline );
		return( NULL );
	}

	/* reference */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_ref( batline, cstr );
		sref = cstr;
	}

	/* label */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_label( batline, cstr );
		slabel = cstr;
	}

	/* at least ref or label must be set */
	if( !my_strlen( sref ) && !my_strlen( slabel )){
		ofa_iimporter_progress_num_text( importer, parms, numline, _( "neither reference nor label are set" ));
		parms->parse_errs += 1;
		g_object_unref( batline );
		return( NULL );
	}

	/* amount */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		amount = my_double_set_from_str( cstr,
				ofa_stream_format_get_thousand_sep( parms->format ),
				ofa_stream_format_get_decimal_sep( parms->format ) );
		ofo_bat_line_set_amount( batline, amount );
	}

	/* currency */
	itf = itf ? itf->next : NULL;
	cstr = itf ? ( const gchar * ) itf->data : NULL;
	if( my_strlen( cstr )){
		ofo_bat_line_set_currency( batline, cstr );
	}

	return( batline );
}

static void
iimportable_import_insert( ofaIImporter *importer, ofsImporterParms *parms, GList *dataset )
{
	GList *it;
	const ofaIDBConnect *connect;
	gboolean insert, skipped;
	guint total;
	gchar *str, *sdate;
	ofoBase *object;
	const gchar *rib, *label;
	gchar *sdbegin, *sdend;
	ofxCounter bat_id;
	ofoDossier *dossier;

	bat_id = 0;
	skipped = FALSE;
	total = g_list_length( dataset );
	connect = ofa_hub_get_connect( parms->hub );
	ofa_iimporter_progress_start( importer, parms );

	if( parms->empty && total > 0 ){
		bat_drop_content( connect );
	}

	for( it=dataset ; it ; it=it->next ){

		if( parms->stop && parms->insert_errs > 0 ){
			break;
		}

		str = NULL;
		insert = TRUE;
		object = OFO_BASE( it->data );

		if( OFO_IS_BAT( object )){

			/* must only pass here once */
			g_return_if_fail( bat_id == 0 );

			if( bat_get_exists( OFO_BAT( object ), connect )){
				parms->duplicate_count += 1;

				rib = ofo_bat_get_rib( OFO_BAT( object ));
				sdbegin = my_date_to_str( ofo_bat_get_begin_date( OFO_BAT( object )), ofa_prefs_date_display());
				sdend = my_date_to_str( ofo_bat_get_end_date( OFO_BAT( object )), ofa_prefs_date_display());

				switch( parms->mode ){
					case OFA_IDUPLICATE_REPLACE:
						str = g_strdup_printf( _( "%s %s %s: duplicate BAT file, replacing previous one" ), rib, sdbegin, sdend );
						bat_do_delete_by_where( OFO_BAT( object ), connect );
						break;
					case OFA_IDUPLICATE_IGNORE:
						str = g_strdup_printf( _( "%s %s %s: duplicate BAT file, skipped" ), rib, sdbegin, sdend );
						skipped = TRUE;
						insert = FALSE;
						total -= 1;
						break;
					case OFA_IDUPLICATE_ABORT:
						str = g_strdup_printf( _( "%s %s %s: duplicate BAT file, making it erroneous" ), rib, sdbegin, sdend );
						insert = FALSE;
						total -= 1;
						parms->insert_errs += 1;
						break;
				}

				g_free( sdbegin );
				g_free( sdend );

				ofa_iimporter_progress_text( importer, parms, str );
				g_free( str );
			}

			if( insert ){
				dossier = ofa_hub_get_dossier( parms->hub );
				bat_set_id( OFO_BAT( object ), ofo_dossier_get_next_bat( dossier ));
				if( bat_do_insert( OFO_BAT( object ), parms->hub )){
					parms->inserted_count += 1;
					bat_id = ofo_bat_get_id( OFO_BAT( object ));
				} else {
					parms->insert_errs += 1;
				}
			}

		} else {
			g_return_if_fail( OFO_IS_BAT_LINE( it->data ));

			if( bat_id <= 0 ){
				sdate = my_date_to_str( ofo_bat_line_get_dope( OFO_BAT_LINE( it->data )), ofa_prefs_date_display());
				label = ofo_bat_line_get_label( OFO_BAT_LINE( it->data ));
				if( skipped ){
					str = g_strdup_printf( _( "%s %s: line ignored due to previous BAT being have been skipped" ), sdate, label );
				} else {
					str = g_strdup_printf( _( "%s %s: line ignored due to previous BAT being have been set erroneous" ), sdate, label );
				}
				ofa_iimporter_progress_text( importer, parms, str );
				g_free( str );
				g_free( sdate );
				total -= 1;

			} else {
				ofo_bat_line_set_bat_id( OFO_BAT_LINE( it->data ), bat_id );
				if( ofo_bat_line_insert( OFO_BAT_LINE( object ), parms->hub )){
					parms->inserted_count += 1;
				} else {
					parms->insert_errs += 1;
				}
			}
		}

		ofa_iimporter_progress_pulse( importer, parms, ( gulong ) parms->inserted_count, ( gulong ) total );
	}
}

/*
 * provided bat is an being-imported one: datas are set but the id
 */
static gboolean
bat_get_exists( const ofoBat *bat, const ofaIDBConnect *connect )
{
	gchar *query, *where;
	gint count;

	count = 0;
	where = bat_get_where( bat );
	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_BAT WHERE %s", where );

	ofa_idbconnect_query_int( connect, query, &count, TRUE );

	g_free( query );
	g_free( where );

	return( count > 0 );
}

/*
 * build a where clause without using the id counter
 */
static gchar *
bat_get_where( const ofoBat *bat )
{
	const GDate *dbegin, *dend;
	const gchar *rib;
	gchar *sdate;
	GString *query;

	dbegin = ofo_bat_get_begin_date( bat );
	dend = ofo_bat_get_end_date( bat );
	rib = ofo_bat_get_rib( bat );

	query = g_string_new( "");

	if( my_strlen( rib )){
		g_string_append_printf( query, "BAT_RIB='%s' ", rib );
	} else {
		g_string_append_printf( query, "BAT_RIB IS NULL " );
	}

	if( my_date_is_valid( dbegin )){
		sdate = my_date_to_str( dbegin, MY_DATE_SQL );
		g_string_append_printf( query, "AND BAT_BEGIN='%s' ", sdate );
		g_free( sdate );
	} else {
		g_string_append_printf( query, "AND BAT_BEGIN IS NULL " );
	}

	if( my_date_is_valid( dend )){
		sdate = my_date_to_str( dend, MY_DATE_SQL );
		g_string_append_printf( query, "AND BAT_END='%s'", sdate );
		g_free( sdate );
	} else {
		g_string_append_printf( query, "AND BAT_END IS NULL" );
	}

	return( g_string_free( query, FALSE ));
}

static ofxCounter
bat_get_id_by_where( const ofoBat *bat, const ofaIDBConnect *connect )
{
	gchar *query, *where;
	GSList *result, *irow, *icol;
	ofxCounter bat_id;

	bat_id = 0;
	where = bat_get_where( bat );
	query = g_strdup_printf( "SELECT BAT_ID FROM OFA_T_BAT WHERE %s", where );

	if( ofa_idbconnect_query_ex( connect, query, &result, FALSE )){
		irow = result;
		icol = ( GSList * ) irow->data;
		bat_id = atol(( gchar * ) icol->data );

		ofa_idbconnect_free_results( result );
	}

	return( bat_id );
}

static gboolean
bat_drop_content( const ofaIDBConnect *connect )
{
	return( ofa_idbconnect_query( connect, "DELETE FROM OFA_T_BAT", TRUE ) &&
			ofa_idbconnect_query( connect, "DELETE FROM OFA_T_BAT_LINES", TRUE ));
}
