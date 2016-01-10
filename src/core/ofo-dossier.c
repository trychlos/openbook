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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-iexportable.h"
#include "api/ofa-preferences.h"
#include "api/ofo-base.h"
#include "api/ofo-base-prot.h"
#include "api/ofo-account.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"

/* priv instance data
 */
struct _ofoDossierPrivate {

	/* internals
	 */
	const ofaIDBConnect *connect;
	gchar         *userid;

	/* row id 1
	 */
	gchar         *currency;
	GDate          exe_begin;
	GDate          exe_end;
	gint           exe_length;			/* exercice length (in months) */
	gchar         *exe_notes;
	gchar         *forward_ope;
	gchar         *import_ledger;
	gchar         *label;				/* raison sociale */
	gchar         *notes;				/* notes */
	gchar         *siren;
	gchar         *siret;
	gchar         *sld_ope;
	gchar         *upd_user;
	GTimeVal       upd_stamp;
	ofxCounter     last_bat;
	ofxCounter     last_batline;
	ofxCounter     last_entry;
	ofxCounter     last_settlement;
	ofxCounter     last_concil;
	//gchar         *status;
	gboolean       current;
	GDate          last_closing;
	ofxCounter     prev_exe_last_entry;

	GList         *cur_details;			/* a list of details per currency */
	GList         *datasets;			/* a list of datasets managed by ofaIDataset */
};

typedef struct {
	gchar *currency;
	gchar *sld_account;					/* balance account number for the currency */
}
	sCurrency;

static ofoBaseClass *ofo_dossier_parent_class = NULL;

static GType       register_type( void );
static void        dossier_instance_init( ofoDossier *self );
static void        dossier_class_init( ofoDossierClass *klass );
static void        on_hub_updated_object( const ofaHub *hub, ofoBase *object, const gchar *prev_id, ofoDossier *dossier );
static void        on_updated_object_currency_code( const ofaHub *hub, const gchar *prev_id, const gchar *code );
static void        on_hub_exe_dates_changed( const ofaHub *hub, const GDate *prev_begin, const GDate *prev_end, ofoDossier *dossier );
static gint        dossier_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo );
static gint        dossier_cur_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo );
static void        dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number );
static sCurrency  *get_currency_detail( ofoDossier *dossier, const gchar *currency, gboolean create );
static gint        cmp_currency_detail( sCurrency *a, sCurrency *b );
static void        dossier_set_upd_user( ofoDossier *dossier, const gchar *user );
static void        dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp );
static void        dossier_set_last_bat( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_batline( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_entry( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_settlement( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_last_concil( ofoDossier *dossier, ofxCounter counter );
static void        dossier_set_prev_exe_last_entry( ofoDossier *dossier, ofxCounter counter );
static gboolean    dossier_do_read( ofoDossier *dossier, const ofaIDBConnect *connect );
static gboolean    dossier_read_properties( ofoDossier *dossier, const ofaIDBConnect *connect );
static gboolean    dossier_read_currencies( ofoDossier *dossier, const ofaIDBConnect *connect );
static gboolean    dossier_do_update( ofoDossier *dossier );
static gboolean    do_update_properties( ofoDossier *dossier );
static gboolean    dossier_do_update_currencies( ofoDossier *dossier );
static gboolean    do_update_currency_properties( ofoDossier *dossier );
static void        iexportable_iface_init( ofaIExportableInterface *iface );
static guint       iexportable_get_interface_version( const ofaIExportable *instance );
static gboolean    iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub );
static void        free_cur_detail( sCurrency *details );
static void        free_cur_details( GList *details );

GType
ofo_dossier_get_type( void )
{
	static GType type = 0;

	if( !type ){
		type = register_type();
	}

	return( type );
}

static GType
register_type( void )
{
	static const gchar *thisfn = "ofo_dossier_register_type";
	GType type;

	static GTypeInfo info = {
		sizeof( ofoDossierClass ),
		( GBaseInitFunc ) NULL,
		( GBaseFinalizeFunc ) NULL,
		( GClassInitFunc ) dossier_class_init,
		NULL,
		NULL,
		sizeof( ofoDossier ),
		0,
		( GInstanceInitFunc ) dossier_instance_init
	};

	static const GInterfaceInfo iexportable_iface_info = {
		( GInterfaceInitFunc ) iexportable_iface_init,
		NULL,
		NULL
	};

	g_debug( "%s", thisfn );

	type = g_type_register_static( OFO_TYPE_BASE, "ofoDossier", &info, 0 );

	g_type_add_interface_static( type, OFA_TYPE_IEXPORTABLE, &iexportable_iface_info );

	return( type );
}

static void
dossier_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_finalize";
	ofoDossierPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFO_IS_DOSSIER( instance ));

	/* free data members here */
	priv = OFO_DOSSIER( instance )->priv;

	g_free( priv->userid );
	g_free( priv->currency );
	g_free( priv->label );
	g_free( priv->forward_ope );
	g_free( priv->sld_ope );
	g_free( priv->import_ledger );
	g_free( priv->notes );
	g_free( priv->siren );
	g_free( priv->upd_user );
	g_free( priv->exe_notes );

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->finalize( instance );
}

static void
dossier_dispose( GObject *instance )
{
	static const gchar *thisfn = "ofo_dossier_dispose";
	ofoDossierPrivate *priv;

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	if( !OFO_BASE( instance )->prot->dispose_has_run ){

		/* unref object members here */
		priv = OFO_DOSSIER( instance )->priv;

		free_cur_details( priv->cur_details );
	}

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofo_dossier_parent_class )->dispose( instance );
}

static void
dossier_instance_init( ofoDossier *self )
{
	static const gchar *thisfn = "ofo_dossier_instance_init";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFO_TYPE_DOSSIER, ofoDossierPrivate );
}

static void
dossier_class_init( ofoDossierClass *klass )
{
	static const gchar *thisfn = "ofo_dossier_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	ofo_dossier_parent_class = g_type_class_peek_parent( klass );

	G_OBJECT_CLASS( klass )->dispose = dossier_dispose;
	G_OBJECT_CLASS( klass )->finalize = dossier_finalize;

	g_type_class_add_private( klass, sizeof( ofoDossierPrivate ));
}

/**
 * ofo_dossier_new_with_hub:
 * @hub: the #ofaHub object which will manage this dossier.
 *
 * Instanciates a new object, and initializes it with data read from
 * database.
 *
 * Returns: a newly allocated #ofoDossier object, or %NULL if an error
 * has occured.
 */
ofoDossier *
ofo_dossier_new_with_hub( ofaHub *hub )
{
	ofoDossier *dossier;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	dossier = g_object_new( OFO_TYPE_DOSSIER, NULL );

	dossier->priv->connect = ofa_hub_get_connect( hub );

	if( dossier_do_read( dossier, ofa_hub_get_connect( hub ))){
		ofo_base_set_hub( OFO_BASE( dossier ), hub );

		g_signal_connect( hub, SIGNAL_HUB_UPDATED, G_CALLBACK( on_hub_updated_object ), dossier );
		g_signal_connect( hub, SIGNAL_HUB_EXE_DATES_CHANGED, G_CALLBACK( on_hub_exe_dates_changed ), dossier );

	} else {
		g_clear_object( &dossier );
	}


	return( dossier );
}

/*
 * SIGNAL_HUB_UPDATED signal handler
 */
static void
on_hub_updated_object( const ofaHub *hub, ofoBase *object, const gchar *prev_id, ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_on_hub_updated_object";
	const gchar *code;

	g_debug( "%s: hub=%p, object=%p (%s), prev_id=%s, dossier=%p",
			thisfn,
			( void * ) hub,
			( void * ) object, G_OBJECT_TYPE_NAME( object ),
			prev_id,
			( void * ) dossier );

	if( OFO_IS_CURRENCY( object )){
		if( my_strlen( prev_id )){
			code = ofo_currency_get_code( OFO_CURRENCY( object ));
			if( g_utf8_collate( code, prev_id )){
				on_updated_object_currency_code( hub, prev_id, code );
			}
		}
	}
}

static void
on_updated_object_currency_code( const ofaHub *hub, const gchar *prev_id, const gchar *code )
{
	gchar *query;

	query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER "
					"	SET DOS_DEF_CURRENCY='%s' WHERE DOS_DEF_CURRENCY='%s'", code, prev_id );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}

/*
 * SIGNAL_HUB_EXE_DATES_CHANGED signal handler
 *
 * Changing beginning or ending exercice dates is only possible for
 * the current exercice.
 */
static void
on_hub_exe_dates_changed( const ofaHub *hub, const GDate *prev_begin, const GDate *prev_end, ofoDossier *dossier )
{
	const ofaIDBConnect *connect;
	ofaIDBMeta *meta;
	ofaIDBPeriod *period;

	connect = ofa_hub_get_connect( hub );
	meta = ofa_idbconnect_get_meta( connect );
	period = ofa_idbconnect_get_period( connect );

	ofa_idbmeta_update_period( meta, period,
			TRUE, ofo_dossier_get_exe_begin( dossier ), ofo_dossier_get_exe_end( dossier ));

	g_object_unref( period );
	g_object_unref( meta );
}

/**
 * ofo_dossier_get_user:
 *
 * Returns: the currently connected user identifier.
 */
const gchar *
ofo_dossier_get_user( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->userid );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_connect:
 * @dossier: this #ofoDossier object.
 *
 * Returns: the handle on the current DBMS connection.
 *
 * The returned reference is owned by the #ofoDossier object, and
 * should not be released by the caller.
 */
const ofaIDBConnect *
ofo_dossier_get_connect( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const ofaIDBConnect * ) dossier->priv->connect );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_use_account:
 *
 * Returns: %TRUE if the dossier makes use of this account, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_account( const ofoDossier *dossier, const gchar *account )
{
	gint count;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( OFO_BASE( dossier )->prot->dispose_has_run ){
		g_return_val_if_reached( FALSE );
	}

	count = dossier_cur_count_uses( dossier, "DOS_SLD_ACCOUNT", account );
	return( count > 0 );
}

/**
 * ofo_dossier_use_currency:
 *
 * Returns: %TRUE if the dossier makes use of this currency, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_currency( const ofoDossier *dossier, const gchar *currency )
{
	const gchar *default_dev;
	gint count;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( OFO_BASE( dossier )->prot->dispose_has_run ){
		g_return_val_if_reached( FALSE );
	}

	default_dev = ofo_dossier_get_default_currency( dossier );

	if( my_strlen( default_dev ) &&
			!g_utf8_collate( default_dev, currency )){
		return( TRUE );
	}

	count = dossier_cur_count_uses( dossier, "DOS_CURRENCY", currency );
	return( count > 0 );
}

/**
 * ofo_dossier_use_ledger:
 *
 * Returns: %TRUE if the dossier makes use of this ledger, thus
 * preventing its deletion.
 */
gboolean
ofo_dossier_use_ledger( const ofoDossier *dossier, const gchar *ledger )
{
	const gchar *import_ledger;
	gint cmp;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( OFO_BASE( dossier )->prot->dispose_has_run ){
		g_return_val_if_reached( FALSE );
	}

	import_ledger = ofo_dossier_get_import_ledger( dossier );
	if( my_strlen( import_ledger )){
		cmp = g_utf8_collate( ledger, import_ledger );
		return( cmp == 0 );
	}

	return( FALSE );
}

/**
 * ofo_dossier_use_ope_template:
 *
 * Returns: %TRUE if the dossier makes use of this operation template,
 * thus preventing its deletion.
 */
gboolean
ofo_dossier_use_ope_template( const ofoDossier *dossier, const gchar *ope_template )
{
	gint forward, solde;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( OFO_BASE( dossier )->prot->dispose_has_run ){
		g_return_val_if_reached( FALSE );
	}

	forward = dossier_count_uses( dossier, "DOS_FORW_OPE", ope_template );
	solde = dossier_count_uses( dossier, "DOS_SLD_OPE", ope_template );
	return( forward+solde > 0 );
}

static gint
dossier_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	hub = ofo_base_get_hub( OFO_BASE( dossier ));

	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DOSSIER WHERE %s='%s' AND DOS_ID=%d",
					field, mnemo, DOSSIER_ROW_ID );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count );
}

static gint
dossier_cur_count_uses( const ofoDossier *dossier, const gchar *field, const gchar *mnemo )
{
	ofaHub *hub;
	gchar *query;
	gint count;

	hub = ofo_base_get_hub( OFO_BASE( dossier ));

	query = g_strdup_printf( "SELECT COUNT(*) FROM OFA_T_DOSSIER_CUR WHERE %s='%s' AND DOS_ID=%d",
					field, mnemo, DOSSIER_ROW_ID );

	ofa_idbconnect_query_int( ofa_hub_get_connect( hub ), query, &count, TRUE );

	g_free( query );

	return( count );
}

/**
 * ofo_dossier_get_database_version:
 * @dossier:
 *
 * Returns the last complete version
 * i.e. a version where the version date is set
 */
gint
ofo_dossier_get_database_version( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	g_warning( "ofo_dossier_get_database_version: obsoleted" );

	return( 0 );
}

/**
 * ofo_dossier_get_default_currency:
 *
 * Returns: the default currency of the dossier.
 */
const gchar *
ofo_dossier_get_default_currency( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->currency );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_exe_begin:
 *
 * Returns: the date of the beginning of the exercice.
 */
const GDate *
ofo_dossier_get_exe_begin( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GDate * ) &dossier->priv->exe_begin );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_exe_end:
 *
 * Returns: the date of the end of the exercice.
 */
const GDate *
ofo_dossier_get_exe_end( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GDate * ) &dossier->priv->exe_end );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_exe_length:
 *
 * Returns: the length of the exercice, in months.
 */
gint
ofo_dossier_get_exe_length( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), -1 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->exe_length );
	}

	g_return_val_if_reached( -1 );
}

/**
 * ofo_dossier_get_exe_notes:
 *
 * Returns: the notes associated to the exercice.
 */
const gchar *
ofo_dossier_get_exe_notes( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->exe_notes );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_forward_ope:
 *
 * Returns: the forward ope of the dossier.
 */
const gchar *
ofo_dossier_get_forward_ope( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->forward_ope );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_import_ledger:
 *
 * Returns: the default import ledger of the dossier.
 */
const gchar *
ofo_dossier_get_import_ledger( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->import_ledger );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_label:
 *
 * Returns: the label of the dossier. This is the 'raison sociale' for
 * the dossier.
 */
const gchar *
ofo_dossier_get_label( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->label );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_notes:
 *
 * Returns: the notes attached to the dossier.
 */
const gchar *
ofo_dossier_get_notes( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->notes );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_siren:
 *
 * Returns: the siren of the dossier.
 */
const gchar *
ofo_dossier_get_siren( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->siren );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_siret:
 *
 * Returns: the siret of the dossier.
 */
const gchar *
ofo_dossier_get_siret( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->siret );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_sld_ope:
 *
 * Returns: the sld ope of the dossier.
 */
const gchar *
ofo_dossier_get_sld_ope( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->sld_ope );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_upd_user:
 *
 * Returns: the identifier of the user who has last updated the
 * properties of the dossier.
 */
const gchar *
ofo_dossier_get_upd_user( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const gchar * ) dossier->priv->upd_user );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_stamp:
 *
 * Returns: the timestamp when a user has last updated the properties
 * of the dossier.
 */
const GTimeVal *
ofo_dossier_get_upd_stamp( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return(( const GTimeVal * ) &dossier->priv->upd_stamp );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_status:
 *
 * Returns: the status of the dossier as a const string suitable for
 * display.
 */
const gchar *
ofo_dossier_get_status( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->current ? _( "Opened" ) : _( "Archived" ));
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_last_bat:
 *
 * Returns: the last bat number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_bat( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_bat );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_last_batline:
 *
 * Returns: the last bat_line number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_batline( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_batline );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_last_entry:
 *
 * Returns: the last entry number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_entry( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_entry );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_last_settlement:
 *
 * Returns: the last settlement number allocated in the exercice.
 */
ofxCounter
ofo_dossier_get_last_settlement( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_settlement );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_last_concil:
 *
 * Returns: the last reconciliation id. allocated.
 */
ofxCounter
ofo_dossier_get_last_concil( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->last_concil );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_next_bat:
 */
ofxCounter
ofo_dossier_get_next_bat( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_bat += 1;
		next = priv->last_bat;
		dossier_update_next( dossier, "DOS_LAST_BAT", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_next_batline:
 */
ofxCounter
ofo_dossier_get_next_batline( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_batline += 1;
		next = priv->last_batline;
		dossier_update_next( dossier, "DOS_LAST_BATLINE", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_next_entry:
 *
 * Returns: the next entry number to be allocated in the dossier.
 */
ofxCounter
ofo_dossier_get_next_entry( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_entry += 1;
		next = priv->last_entry;
		dossier_update_next( dossier, "DOS_LAST_ENTRY", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_next_settlement:
 */
ofxCounter
ofo_dossier_get_next_settlement( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_settlement += 1;
		next = priv->last_settlement;
		dossier_update_next( dossier, "DOS_LAST_SETTLEMENT", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_next_concil:
 */
ofxCounter
ofo_dossier_get_next_concil( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofxCounter next;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );
	g_return_val_if_fail( ofo_dossier_is_current( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		priv->last_concil += 1;
		next = priv->last_concil;
		dossier_update_next( dossier, "DOS_LAST_CONCIL", next );
		return( next );
	}

	g_return_val_if_reached( 0 );
}

/*
 * ofo_dossier_update_next_number:
 */
static void
dossier_update_next( const ofoDossier *dossier, const gchar *field, ofxCounter next_number )
{
	ofaHub *hub;
	gchar *query;

	hub = ofo_base_get_hub( OFO_BASE( dossier ));

	query = g_strdup_printf(
			"UPDATE OFA_T_DOSSIER "
			"	SET %s=%ld "
			"	WHERE DOS_ID=%d",
					field, next_number, DOSSIER_ROW_ID );

	ofa_idbconnect_query( ofa_hub_get_connect( hub ), query, TRUE );

	g_free( query );
}

/**
 * ofo_dossier_get_last_closing_date:
 * @dossier:
 *
 * Returns: the last period closing date.
 */
const GDate *
ofo_dossier_get_last_closing_date( const ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		return(( const GDate * ) &priv->last_closing );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_prev_exe_last_entry:
 * @dossier:
 *
 * Returns: the last entry number of the previous exercice.
 */
ofxCounter
ofo_dossier_get_prev_exe_last_entry( const ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		return( priv->prev_exe_last_entry );
	}

	g_return_val_if_reached( 0 );
}

/**
 * ofo_dossier_get_min_deffect:
 * @dossier: this dossier.
 * @ledger: [allow-none]: the imputed ledger.
 * @date: [out]: the #GDate date to be set.
 *
 * Computes the minimal effect date valid for the considered dossier and
 * ledger.
 *
 * This minimal effect date is:
 * - at least the begin of the exercice (if set)
 * - or the last ledger closing date (if set, and greater than the previous) + 1
 *
 * Returns: the @date #GDate pointer.
 */
GDate *
ofo_dossier_get_min_deffect( const ofoDossier *dossier, const ofoLedger *ledger, GDate *date )
{
	const GDate *last_clo;
	gint to_add;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( ledger ){
		g_return_val_if_fail( OFO_IS_LEDGER( ledger ), NULL );
	}

	if( OFO_BASE( dossier )->prot->dispose_has_run ){
		g_return_val_if_reached( NULL );
	}

	last_clo = ledger ? ofo_ledger_get_last_close( ledger ) : NULL;
	my_date_set_from_date( date, ofo_dossier_get_exe_begin( dossier ));
	to_add = 0;

	if( my_date_is_valid( date )){
		if( my_date_is_valid( last_clo )){
			if( my_date_compare( date, last_clo ) < 0 ){
				my_date_set_from_date( date, last_clo );
				to_add = 1;
			}
		}
	} else if( my_date_is_valid( last_clo )){
		my_date_set_from_date( date, last_clo );
		to_add = 1;
	}

	if( my_date_is_valid( date )){
		g_date_add_days( date, to_add );
	}

	return( date );
}

/**
 * ofo_dossier_get_currencies:
 * @dossier: this dossier.
 *
 * Returns: an alphabetically sorted #GSList of the currencies defined
 * in the subtable.
 *
 * The returned list should be #ofo_dossier_free_currencies() by the
 * caller.
 */
GSList *
ofo_dossier_get_currencies( const ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	sCurrency *sdet;
	GSList *list;
	GList *it;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		list = NULL;

		for( it=priv->cur_details ; it ; it=it->next ){
			sdet = ( sCurrency * ) it->data;
			list = g_slist_insert_sorted( list, g_strdup( sdet->currency ), ( GCompareFunc ) g_utf8_collate );
		}

		return( list );
	}

	g_return_val_if_reached( NULL );
}

/**
 * ofo_dossier_get_sld_account:
 * @dossier: this dossier.
 * @currency: the serched currency.
 *
 * Returns: the account configured as balancing account for this
 * currency.
 *
 * The returned string is owned by the @dossier, and must not be freed
 * by the caller.
 */
const gchar *
ofo_dossier_get_sld_account( ofoDossier *dossier, const gchar *currency )
{
	sCurrency *sdet;

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), NULL );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sdet = get_currency_detail( dossier, currency, FALSE );
		if( sdet ){
			return(( const gchar * ) sdet->sld_account );
		}
	}

	return( NULL );
}

static sCurrency *
get_currency_detail( ofoDossier *dossier, const gchar *currency, gboolean create )
{
	ofoDossierPrivate *priv;
	GList *it;
	sCurrency *sdet;

	priv = dossier->priv;
	sdet = NULL;

	for( it=priv->cur_details ; it ; it=it->next ){
		sdet = ( sCurrency * ) it->data;
		if( !g_utf8_collate( sdet->currency, currency )){
			return( sdet );
		}
	}

	if( create ){
		sdet = g_new0( sCurrency, 1 );
		sdet->currency = g_strdup( currency );
		priv->cur_details = g_list_insert_sorted( priv->cur_details, sdet, ( GCompareFunc ) cmp_currency_detail );
	}

	return( sdet );
}

static gint
cmp_currency_detail( sCurrency *a, sCurrency *b )
{
	return( g_utf8_collate( a->currency, b->currency ));
}

#if 0
/**
 * ofo_dossier_get_last_closed_exercice:
 *
 * Returns: the last exercice closing date, as a newly allocated
 * #GDate structure which should be g_free() by the caller,
 * or %NULL if the dossier has never been closed.
 */
GDate *
ofo_dossier_get_last_closed_exercice( const ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_get_last_closed_exercice";
	GList *exe;
	sDetailExe *sexe;
	GDate *dmax;
	gchar *str;

	g_return_val_if_fail( OFO_IS_DOSSIER( dossier ), NULL );

	dmax = NULL;

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		for( exe=dossier->priv->exes ; exe ; exe=exe->next ){
			sexe = ( sDetailExe * ) exe->data;
			if( sexe->status == DOS_STATUS_CLOSED ){
				g_return_val_if_fail( my_date_is_valid( &sexe->exe_end ), NULL );
				if( dmax ){
					g_return_val_if_fail( my_date_is_valid( dmax ), NULL );
					if( my_date_compare( &sexe->exe_end, dmax ) > 0 ){
						my_date_set_from_date( dmax, &sexe->exe_end );
					}
				} else {
					dmax = g_new0( GDate, 1 );
					my_date_set_from_date( dmax, &sexe->exe_end );
				}
			}
		}
	}

	str = my_date_to_str( dmax, ofa_prefs_date_display());
	g_debug( "%s: last_closed_exercice=%s", thisfn, str );
	g_free( str );

	return( dmax );
}
#endif

/**
 * ofo_dossier_is_current:
 *
 * Returns: %TRUE if the exercice if the current one.
 */
gboolean
ofo_dossier_is_current( const ofoDossier *dossier )
{
	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		return( dossier->priv->current );
	}

	g_return_val_if_reached( FALSE );
}

/**
 * ofo_dossier_is_valid:
 */
gboolean
ofo_dossier_is_valid( const gchar *label, gint nb_months, const gchar *currency,
								const GDate *begin, const GDate *end, gchar **msg )
{
	if( !my_strlen( label )){
		*msg = g_strdup( _( "Empty label" ));
		return( FALSE );
	}

	if( nb_months <= 0 ){
		*msg = g_strdup_printf( "Invalid length of exercice: %d", nb_months );
		return( FALSE );
	}

	if( !my_strlen( currency )){
		*msg = g_strdup( _( "Empty default currency"));
		return( FALSE );
	}

	if( my_date_is_valid( begin ) && my_date_is_valid( end )){
		if( my_date_compare( begin, end ) > 0 ){
			*msg = g_strdup( _( "Exercice is set to begin after it has ended" ));
			return( FALSE );
		}
	}

	return( TRUE );
}

/**
 * ofo_dossier_set_default_currency:
 */
void
ofo_dossier_set_default_currency( ofoDossier *dossier, const gchar *currency )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->currency );
		dossier->priv->currency = g_strdup( currency );
	}
}

/**
 * ofo_dossier_set_exe_begin:
 */
void
ofo_dossier_set_exe_begin( ofoDossier *dossier, const GDate *date )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		my_date_set_from_date( &dossier->priv->exe_begin, date );
	}
}

/**
 * ofo_dossier_set_exe_end:
 */
void
ofo_dossier_set_exe_end( ofoDossier *dossier, const GDate *date )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		my_date_set_from_date( &dossier->priv->exe_end, date );
	}
}

/**
 * ofo_dossier_set_exe_length:
 */
void
ofo_dossier_set_exe_length( ofoDossier *dossier, gint nb_months )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( nb_months > 0 );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->exe_length = nb_months;
	}
}

/**
 * ofo_dossier_set_exe_notes:
 * @dossier: this dossier
 * @notes : the notes to be set
 *
 * Attach the given @notes to the exercice.
 */
void
ofo_dossier_set_exe_notes( ofoDossier *dossier, const gchar *notes )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		g_free( priv->exe_notes );
		priv->exe_notes = g_strdup( notes );
	}
}

/**
 * ofo_dossier_set_forward_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_forward_ope( ofoDossier *dossier, const gchar *ope )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->forward_ope );
		dossier->priv->forward_ope = g_strdup( ope );
	}
}

/**
 * ofo_dossier_set_import_ledger:
 *
 * Not mandatory until importing entries.
 */
void
ofo_dossier_set_import_ledger( ofoDossier *dossier, const gchar *ledger )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->import_ledger );
		dossier->priv->import_ledger = g_strdup( ledger );
	}
}

/**
 * ofo_dossier_set_label:
 */
void
ofo_dossier_set_label( ofoDossier *dossier, const gchar *label )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( my_strlen( label ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->label );
		dossier->priv->label = g_strdup( label );
	}
}

/**
 * ofo_dossier_set_notes:
 */
void
ofo_dossier_set_notes( ofoDossier *dossier, const gchar *notes )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->notes );
		dossier->priv->notes = g_strdup( notes );
	}
}

/**
 * ofo_dossier_set_siren:
 */
void
ofo_dossier_set_siren( ofoDossier *dossier, const gchar *siren )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->siren );
		dossier->priv->siren = g_strdup( siren );
	}
}

/**
 * ofo_dossier_set_siret:
 */
void
ofo_dossier_set_siret( ofoDossier *dossier, const gchar *siret )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->siret );
		dossier->priv->siret = g_strdup( siret );
	}
}

/**
 * ofo_dossier_set_sld_ope:
 *
 * Not mandatory until closing the exercice.
 */
void
ofo_dossier_set_sld_ope( ofoDossier *dossier, const gchar *ope )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->sld_ope );
		dossier->priv->sld_ope = g_strdup( ope );
	}
}

/*
 * ofo_dossier_set_upd_user:
 */
static void
dossier_set_upd_user( ofoDossier *dossier, const gchar *user )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( my_strlen( user ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_free( dossier->priv->upd_user );
		dossier->priv->upd_user = g_strdup( user );
	}
}

/*
 * ofo_dossier_set_upd_stamp:
 */
static void
dossier_set_upd_stamp( ofoDossier *dossier, const GTimeVal *stamp )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( stamp );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		my_utils_stamp_set_from_stamp( &dossier->priv->upd_stamp, stamp );
	}
}

static void
dossier_set_last_bat( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_bat = counter;
	}
}

static void
dossier_set_last_batline( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_batline = counter;
	}
}

static void
dossier_set_last_entry( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_entry = counter;
	}
}

static void
dossier_set_last_settlement( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_settlement = counter;
	}
}

static void
dossier_set_last_concil( ofoDossier *dossier, ofxCounter counter )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->last_concil = counter;
	}
}

/**
 * ofo_dossier_set_current:
 * @dossier: this #ofoDossier instance.
 * @current: %TRUE if this @dossier period is opened, %FALSE if it is
 *  archived.
 *
 * Set the status of the financial period.
 */
void
ofo_dossier_set_current( ofoDossier *dossier, gboolean current )
{
	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		dossier->priv->current = current;
	}
}

/**
 * ofo_dossier_set_last_closing_date:
 * @dossier:
 * @last_closing: the last period closing date.
 */
void
ofo_dossier_set_last_closing_date( ofoDossier *dossier, const GDate *last_closing )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		my_date_set_from_date( &priv->last_closing, last_closing );
	}
}

/**
 * ofo_dossier_set_prev_exe_last_entry:
 * @dossier:
 */
void
ofo_dossier_set_prev_exe_last_entry( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		dossier_set_prev_exe_last_entry( dossier, priv->prev_exe_last_entry );
	}
}

/*
 * dossier_set_prev_exe_last_entry:
 * @dossier:
 * @last_entry:
 */
static void
dossier_set_prev_exe_last_entry( ofoDossier *dossier, ofxCounter last_entry )
{
	ofoDossierPrivate *priv;

	priv = dossier->priv;
	priv->prev_exe_last_entry = last_entry;
}

/**
 * ofo_dossier_reset_currencies:
 * @dossier: this dossier.
 *
 * Reset the currencies (free all).
 *
 * This function should be called when updating the currencies
 * properties, as we are only able to set new elements in the list.
 */
void
ofo_dossier_reset_currencies( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		priv = dossier->priv;
		free_cur_details( priv->cur_details );
		priv->cur_details = NULL;
	}
}

/**
 * ofo_dossier_set_sld_account:
 * @dossier: this dossier.
 * @currency: the currency.
 * @account: the account.
 *
 * Set the balancing account for the currency.
 */
void
ofo_dossier_set_sld_account( ofoDossier *dossier, const gchar *currency, const gchar *account )
{
	sCurrency *sdet;

	g_return_if_fail( dossier && OFO_IS_DOSSIER( dossier ));
	g_return_if_fail( my_strlen( currency ));
	g_return_if_fail( my_strlen( account ));

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		sdet = get_currency_detail( dossier, currency, TRUE );
		sdet->sld_account = g_strdup( account );
	}
}

static gboolean
dossier_do_read( ofoDossier *dossier, const ofaIDBConnect *connect )
{
	return( dossier_read_properties( dossier, connect ) &&
			dossier_read_currencies( dossier, connect ));
}

static gboolean
dossier_read_properties( ofoDossier *dossier, const ofaIDBConnect *connect )
{
	gchar *query;
	GSList *result, *icol;
	gboolean ok;
	const gchar *cstr;
	GTimeVal timeval;
	GDate date;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_DEF_CURRENCY,"
			"	DOS_EXE_BEGIN,DOS_EXE_END,DOS_EXE_LENGTH,DOS_EXE_NOTES,"
			"	DOS_FORW_OPE,DOS_IMPORT_LEDGER,"
			"	DOS_LABEL,DOS_NOTES,DOS_SIREN,DOS_SIRET,"
			"	DOS_SLD_OPE,DOS_UPD_USER,DOS_UPD_STAMP,"
			"	DOS_LAST_BAT,DOS_LAST_BATLINE,DOS_LAST_ENTRY,DOS_LAST_SETTLEMENT,"
			"	DOS_LAST_CONCIL,DOS_CURRENT,DOS_LAST_CLOSING,DOS_PREVEXE_ENTRY "
			"FROM OFA_T_DOSSIER "
			"WHERE DOS_ID=%d", DOSSIER_ROW_ID );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		icol = ( GSList * ) result->data;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_default_currency( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_exe_begin( dossier, my_date_set_from_sql( &date, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_exe_end( dossier, my_date_set_from_sql( &date, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_exe_length( dossier, atoi( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_exe_notes( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_forward_ope( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_import_ledger( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_label( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_notes( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_siren( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_siret( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_sld_ope( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_upd_user( dossier, cstr );
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_upd_stamp( dossier, my_utils_stamp_set_from_sql( &timeval, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_last_bat( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_last_batline( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_last_entry( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_last_settlement( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_last_concil( dossier, atol( cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		ofo_dossier_set_current( dossier, my_strlen( cstr ) && !g_utf8_collate( cstr, "Y" ));
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			ofo_dossier_set_last_closing_date( dossier, my_date_set_from_sql( &date, cstr ));
		}
		icol = icol->next;
		cstr = icol->data;
		if( my_strlen( cstr )){
			dossier_set_prev_exe_last_entry( dossier, atol( cstr ));
		}

		ok = TRUE;
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( ok );
}

static gboolean
dossier_read_currencies( ofoDossier *dossier, const ofaIDBConnect *connect )
{
	gchar *query;
	GSList *result, *irow, *icol;
	gboolean ok;
	const gchar *cstr, *currency;
	sCurrency *sdet;

	ok = FALSE;

	query = g_strdup_printf(
			"SELECT DOS_CURRENCY,DOS_SLD_ACCOUNT "
			"	FROM OFA_T_DOSSIER_CUR "
			"	WHERE DOS_ID=%d ORDER BY DOS_CURRENCY ASC", DOSSIER_ROW_ID );

	if( ofa_idbconnect_query_ex( connect, query, &result, TRUE )){
		for( irow=result ; irow ; irow=irow->next ){
			icol = ( GSList * ) irow->data;
			currency = icol->data;
			if( my_strlen( currency )){
				icol = icol->next;
				cstr = icol->data;
				if( my_strlen( cstr )){
					sdet = get_currency_detail( dossier, currency, TRUE );
					sdet->sld_account = g_strdup( cstr );
				}
			}
		}
		ok = TRUE;
		ofa_idbconnect_free_results( result );
	}

	g_free( query );

	return( ok );
}

/**
 * ofo_dossier_update:
 *
 * Update the properties of the dossier.
 */
gboolean
ofo_dossier_update( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update";

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( !OFO_BASE( dossier )->prot->dispose_has_run ){

		g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

		return( dossier_do_update( dossier ));
	}

	g_assert_not_reached();
	return( FALSE );
}

static gboolean
dossier_do_update( ofoDossier *dossier )
{
	return( do_update_properties( dossier ));
}

static gboolean
do_update_properties( ofoDossier *dossier )
{
	ofaHub *hub;
	const ofaIDBConnect *connect;
	GString *query;
	gchar *label, *notes, *stamp_str, *sdate, *userid;
	GTimeVal stamp;
	gboolean ok, current;
	const gchar *cstr;
	const GDate *date;
	ofxCounter number;

	ok = FALSE;
	hub = ofo_base_get_hub( OFO_BASE( dossier ));
	connect = ofa_hub_get_connect( hub );
	userid = ofa_idbconnect_get_account( connect );

	query = g_string_new( "UPDATE OFA_T_DOSSIER SET " );

	cstr = ofo_dossier_get_default_currency( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_DEF_CURRENCY='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_DEF_CURRENCY=NULL," );
	}

	date = ofo_dossier_get_exe_begin( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_EXE_BEGIN='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_EXE_BEGIN=NULL," );
	}

	date = ofo_dossier_get_exe_end( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_EXE_END='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_EXE_END=NULL," );
	}

	g_string_append_printf( query,
			"DOS_EXE_LENGTH=%d,", ofo_dossier_get_exe_length( dossier ));

	notes = my_utils_quote( ofo_dossier_get_exe_notes( dossier ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "DOS_EXE_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_EXE_NOTES=NULL," );
	}
	g_free( notes );

	cstr = ofo_dossier_get_forward_ope( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_FORW_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_FORW_OPE=NULL," );
	}

	cstr = ofo_dossier_get_import_ledger( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_IMPORT_LEDGER='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_IMPORT_LEDGER=NULL," );
	}

	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	if( my_strlen( label )){
		g_string_append_printf( query, "DOS_LABEL='%s',", label );
	} else {
		query = g_string_append( query, "DOS_LABEL=NULL," );
	}
	g_free( label );

	notes = my_utils_quote( ofo_dossier_get_notes( dossier ));
	if( my_strlen( notes )){
		g_string_append_printf( query, "DOS_NOTES='%s',", notes );
	} else {
		query = g_string_append( query, "DOS_NOTES=NULL," );
	}
	g_free( notes );

	cstr = ofo_dossier_get_siren( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SIREN='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SIREN=NULL," );
	}

	cstr = ofo_dossier_get_siret( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SIRET='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SIRET=NULL," );
	}

	cstr = ofo_dossier_get_sld_ope( dossier );
	if( my_strlen( cstr )){
		g_string_append_printf( query, "DOS_SLD_OPE='%s',", cstr );
	} else {
		query = g_string_append( query, "DOS_SLD_OPE=NULL," );
	}

	date = ofo_dossier_get_last_closing_date( dossier );
	if( my_date_is_valid( date )){
		sdate = my_date_to_str( date, MY_DATE_SQL );
		g_string_append_printf( query, "DOS_LAST_CLOSING='%s',", sdate );
		g_free( sdate );
	} else {
		query = g_string_append( query, "DOS_LAST_CLOSING=NULL," );
	}

	number = ofo_dossier_get_prev_exe_last_entry( dossier );
	if( number > 0 ){
		g_string_append_printf( query, "DOS_PREVEXE_ENTRY=%ld,", number );
	} else {
		query = g_string_append( query, "DOS_PREVEXE_ENTRY=NULL," );
	}

	current = ofo_dossier_is_current( dossier );
	g_string_append_printf( query, "DOS_CURRENT='%s',", current ? "Y":"N" );

	my_utils_stamp_set_now( &stamp );
	stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
	g_string_append_printf( query,
			"DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' ", userid, stamp_str );
	g_free( stamp_str );

	g_string_append_printf( query, "WHERE DOS_ID=%d", DOSSIER_ROW_ID );

	if( ofa_idbconnect_query( connect, query->str, TRUE )){
		dossier_set_upd_user( dossier, userid );
		dossier_set_upd_stamp( dossier, &stamp );
		ok = TRUE;
	}

	g_string_free( query, TRUE );
	g_free( userid );

	return( ok );
}

/**
 * ofo_dossier_update_currencies:
 *
 * Update the currency properties of the dossier.
 */
gboolean
ofo_dossier_update_currencies( ofoDossier *dossier )
{
	static const gchar *thisfn = "ofo_dossier_update_currencies";

	g_debug( "%s: dossier=%p", thisfn, ( void * ) dossier );

	g_return_val_if_fail( dossier && OFO_IS_DOSSIER( dossier ), FALSE );

	if( OFO_BASE( dossier )->prot->dispose_has_run ){
		g_return_val_if_reached( FALSE );
	}

	return( dossier_do_update_currencies( dossier ));
}

static gboolean
dossier_do_update_currencies( ofoDossier *dossier )
{
	return( do_update_currency_properties( dossier ));
}

static gboolean
do_update_currency_properties( ofoDossier *dossier )
{
	ofoDossierPrivate *priv;
	ofaHub *hub;
	const ofaIDBConnect *connect;
	gchar *query, *userid;
	sCurrency *sdet;
	gboolean ok;
	GList *it;
	GTimeVal stamp;
	gchar *stamp_str;
	gint count;

	priv = dossier->priv;
	hub = ofo_base_get_hub( OFO_BASE( dossier ));
	connect = ofa_hub_get_connect( hub );
	userid = ofa_idbconnect_get_account( connect );

	ok = ofa_idbconnect_query( connect, "DELETE FROM OFA_T_DOSSIER_CUR", TRUE );
	count = 0;

	if( ok ){
		for( it=priv->cur_details ; it && ok ; it=it->next ){
			sdet = ( sCurrency * ) it->data;
			query = g_strdup_printf(
					"INSERT INTO OFA_T_DOSSIER_CUR (DOS_ID,DOS_CURRENCY,DOS_SLD_ACCOUNT) VALUES "
					"	(%d,'%s','%s')", DOSSIER_ROW_ID, sdet->currency, sdet->sld_account );
			ok &= ofa_idbconnect_query( connect, query, TRUE );
			g_free( query );
			count += 1;
		}

		if( ok && count ){
			my_utils_stamp_set_now( &stamp );
			stamp_str = my_utils_stamp_to_str( &stamp, MY_STAMP_YYMDHMS );
			query = g_strdup_printf(
					"UPDATE OFA_T_DOSSIER SET "
					"	DOS_UPD_USER='%s',DOS_UPD_STAMP='%s' "
					"	WHERE DOS_ID=%d", userid, stamp_str, DOSSIER_ROW_ID );
			g_free( stamp_str );

			if( !ofa_idbconnect_query( connect, query, TRUE )){
				ok = FALSE;
			} else {
				dossier_set_upd_user( dossier, userid );
				dossier_set_upd_stamp( dossier, &stamp );
			}
		}
	}

	g_free( userid );

	return( ok );
}

/*
 * ofaIExportable interface management
 */
static void
iexportable_iface_init( ofaIExportableInterface *iface )
{
	static const gchar *thisfn = "ofo_dossier_iexportable_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = iexportable_get_interface_version;
	iface->export = iexportable_export;
}

static guint
iexportable_get_interface_version( const ofaIExportable *instance )
{
	return( 1 );
}

/*
 * iexportable_export:
 *
 * Exports the classes line by line.
 *
 * Returns: TRUE at the end if no error has been detected
 */
static gboolean
iexportable_export( ofaIExportable *exportable, const ofaFileFormat *settings, ofaHub *hub )
{
	ofoDossier *dossier;
	GSList *lines;
	gchar *str, *stamp;
	const gchar *currency, *muser, *siren, *siret;
	const gchar *fope;
	const gchar *bope;
	gchar *sbegin, *send, *notes, *exenotes, *label;
	gboolean ok, with_headers;
	gulong count;

	dossier = ofa_hub_get_dossier( hub );
	with_headers = ofa_file_format_has_headers( settings );

	count = ( gulong ) 1;
	if( with_headers ){
		count += 1;
	}
	ofa_iexportable_set_count( exportable, count );

	if( with_headers ){
		str = g_strdup_printf( "DefCurrency;ExeBegin;ExeEnd;ExeLength;ExeNotes;"
				"ForwardOpe;"
				"Label;Notes;Siren;Siret;"
				"SldOpe;"
				"MajUser;MajStamp;"
				"LastBat;LastBatLine;LastEntry;LastSettlement;"
				"Current" );
		lines = g_slist_prepend( NULL, str );
		ok = ofa_iexportable_export_lines( exportable, lines );
		g_slist_free_full( lines, ( GDestroyNotify ) g_free );
		if( !ok ){
			return( FALSE );
		}
	}

	sbegin = my_date_to_str( ofo_dossier_get_exe_begin( dossier ), MY_DATE_SQL );
	send = my_date_to_str( ofo_dossier_get_exe_end( dossier ), MY_DATE_SQL );
	exenotes = my_utils_export_multi_lines( ofo_dossier_get_exe_notes( dossier ));
	fope = ofo_dossier_get_forward_ope( dossier );
	notes = my_utils_export_multi_lines( ofo_dossier_get_notes( dossier ));
	label = my_utils_quote( ofo_dossier_get_label( dossier ));
	muser = ofo_dossier_get_upd_user( dossier );
	stamp = my_utils_stamp_to_str( ofo_dossier_get_upd_stamp( dossier ), MY_STAMP_YYMDHMS );
	currency = ofo_dossier_get_default_currency( dossier );
	siren = ofo_dossier_get_siren( dossier );
	siret = ofo_dossier_get_siret( dossier );
	bope = ofo_dossier_get_sld_ope( dossier );

	str = g_strdup_printf( "%s;%s;%s;%d;%s;%s;%s;%s;%s;%s;%s;%s;%s;%ld;%ld;%ld;%ld;%s",
			currency ? currency : "",
			sbegin,
			send,
			ofo_dossier_get_exe_length( dossier ),
			exenotes ? exenotes : "",
			fope ? fope : "",
			label ? label : "",
			notes ? notes : "",
			siren ? siren : "",
			siret ? siret : "",
			bope ? bope : "",
			muser ? muser : "",
			muser ? stamp : "",
			ofo_dossier_get_last_bat( dossier ),
			ofo_dossier_get_last_batline( dossier ),
			ofo_dossier_get_last_entry( dossier ),
			ofo_dossier_get_last_settlement( dossier ),
			ofo_dossier_is_current( dossier ) ? "Y":"N" );

	g_free( sbegin );
	g_free( send );
	g_free( label );
	g_free( exenotes );
	g_free( notes );
	g_free( stamp );

	lines = g_slist_prepend( NULL, str );
	ok = ofa_iexportable_export_lines( exportable, lines );
	g_slist_free_full( lines, ( GDestroyNotify ) g_free );
	if( !ok ){
		return( FALSE );
	}

	return( TRUE );
}

static void
free_cur_detail( sCurrency *details )
{
	g_free( details->currency );
	g_free( details->sld_account );
	g_free( details );
}

static void
free_cur_details( GList *details )
{
	if( details ){
		g_list_free_full( details, ( GDestroyNotify ) free_cur_detail );
	}
}
