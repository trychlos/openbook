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

#include "api/my-date.h"
#include "api/my-utils.h"
#include "api/ofa-hub.h"
#include "api/ofa-icollector.h"
#include "api/ofa-idbmeta.h"
#include "api/ofa-idbmodel.h"
#include "api/ofa-idbperiod.h"
#include "api/ofo-account.h"
#include "api/ofo-bat.h"
#include "api/ofo-class.h"
#include "api/ofo-concil.h"
#include "api/ofo-currency.h"
#include "api/ofo-dossier.h"
#include "api/ofo-entry.h"
#include "api/ofo-ledger.h"
#include "api/ofo-ope-template.h"
#include "api/ofo-rate.h"

/* private instance data
 */
struct _ofaHubPrivate {
	gboolean             dispose_has_run;

	/* initialization
	 */
	const ofaIDBConnect *connect;
	ofoDossier          *dossier;
};

/* signals defined here
 */
enum {
	NEW_OBJECT = 0,
	UPDATED_OBJECT,
	DELETED_OBJECT,
	RELOAD_DATASET,
	ENTRY_STATUS_CHANGED,
	EXE_DATES_CHANGED,
	N_SIGNALS
};

static gint st_signals[ N_SIGNALS ]     = { 0 };

static void    icollector_iface_init( ofaICollectorInterface *iface );
static guint   icollector_get_interface_version( const ofaICollector *instance );
static void    init_signaling_system( const ofaHub *hub );
static void    check_db_vs_settings( const ofaHub *hub );
static GSList *get_lines_from_csv( const gchar *uri, const ofaFileFormat *settings );
static void    free_fields( GSList *fields );
static void    free_lines( GSList *lines );

G_DEFINE_TYPE_EXTENDED( ofaHub, ofa_hub, G_TYPE_OBJECT, 0, \
		G_IMPLEMENT_INTERFACE( OFA_TYPE_ICOLLECTOR, icollector_iface_init ));

static void
hub_finalize( GObject *instance )
{
	static const gchar *thisfn = "ofa_hub_finalize";

	g_debug( "%s: instance=%p (%s)",
			thisfn, ( void * ) instance, G_OBJECT_TYPE_NAME( instance ));

	g_return_if_fail( instance && OFA_IS_HUB( instance ));

	/* free data members here */

	/* chain up to the parent class */
	G_OBJECT_CLASS( ofa_hub_parent_class )->finalize( instance );
}

static void
hub_dispose( GObject *instance )
{
	ofaHubPrivate *priv;

	g_return_if_fail( instance && OFA_IS_HUB( instance ));

	priv = OFA_HUB( instance )->priv;

	if( !priv->dispose_has_run ){

		priv->dispose_has_run = TRUE;

		/* unref object members here */
		g_clear_object( &priv->connect );
		g_clear_object( &priv->dossier );
	}

	G_OBJECT_CLASS( ofa_hub_parent_class )->dispose( instance );
}

static void
ofa_hub_init( ofaHub *self )
{
	static const gchar *thisfn = "ofa_hub_init";

	g_debug( "%s: self=%p (%s)",
			thisfn, ( void * ) self, G_OBJECT_TYPE_NAME( self ));

	g_return_if_fail( self && OFA_IS_HUB( self ));

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self, OFA_TYPE_HUB, ofaHubPrivate );
	self->priv->dispose_has_run = FALSE;
}

static void
ofa_hub_class_init( ofaHubClass *klass )
{
	static const gchar *thisfn = "ofa_hub_class_init";

	g_debug( "%s: klass=%p", thisfn, ( void * ) klass );

	G_OBJECT_CLASS( klass )->dispose = hub_dispose;
	G_OBJECT_CLASS( klass )->finalize = hub_finalize;

	g_type_class_add_private( klass, sizeof( ofaHubPrivate ));

	/**
	 * ofaHub::hub-object-new:
	 *
	 * The signal is emitted after a new object has been successfully
	 * inserted in the database. A connected handler may take advantage
	 * of this signal e.g. to update its own list of displayed objects.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub    *hub,
	 * 							ofoBase  *object,
	 * 							gpointer  user_data );
	 */
	st_signals[ NEW_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_NEW,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaHub::hub-object-updated:
	 *
	 * The signal is emitted just after an object has been successfully
	 * updated in the DBMS. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects,
	 * or for updating its internal links, or so.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub       *hub,
	 * 							ofoBase     *object,
	 * 							const gchar *prev_id,
	 * 							gpointer     user_data );
	 */
	st_signals[ UPDATED_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_UPDATED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_OBJECT, G_TYPE_STRING );

	/**
	 * ofaHub::hub-object-deleted:
	 *
	 * The signal is emitted just after an object has been successfully
	 * deleted from the DBMS. A connected handler may take advantage of
	 * this signal e.g. for updating its own list of displayed objects.
	 *
	 * Note that the emitter of this signal should attach a new reference
	 * to the deleted object in order to keep it alive during for the
	 * handlers execution.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub    *hub,
	 * 							ofoBase  *object,
	 * 							gpointer  user_data );
	 */
	st_signals[ DELETED_OBJECT ] = g_signal_new_class_handler(
				SIGNAL_HUB_DELETED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_OBJECT );

	/**
	 * ofaHub::hub-object-reload:
	 *
	 * The signal is emitted when such an update has been made in the
	 * DBMS that it is considered easier for a connected handler just
	 * to reload the whole dataset if this later whishes keep
	 * synchronized with the database.
	 *
	 * This signal is so less an information signal that an action hint.
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub   *hub,
	 * 							GType    type_class,
	 * 							gpointer user_data );
	 */
	st_signals[ RELOAD_DATASET ] = g_signal_new_class_handler(
				SIGNAL_HUB_RELOAD,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				1,
				G_TYPE_GTYPE );

	/**
	 * ofaHub::hub-entry-status-changed:
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub          *hub,
	 * 							const ofoEntry *entry
	 * 							gint            prev_status,
	 *							gint            new_status,
	 * 							gpointer        user_data );
	 */
	st_signals[ ENTRY_STATUS_CHANGED ] = g_signal_new_class_handler(
				SIGNAL_HUB_ENTRY_STATUS_CHANGED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				3,
				G_TYPE_OBJECT, G_TYPE_UINT, G_TYPE_UINT );

	/**
	 * ofaHub::hub-exe-dates-changed:
	 *
	 * Handler is of type:
	 * 		void user_handler( ofaHub       *hub
	 * 							const GDate *prev_begin,
	 * 							const GDate *prev_end,
	 * 							gpointer     user_data );
	 */
	st_signals[ EXE_DATES_CHANGED ] = g_signal_new_class_handler(
				SIGNAL_HUB_EXE_DATES_CHANGED,
				OFA_TYPE_HUB,
				G_SIGNAL_RUN_LAST,
				NULL,
				NULL,								/* accumulator */
				NULL,								/* accumulator data */
				NULL,
				G_TYPE_NONE,
				2,
				G_TYPE_POINTER, G_TYPE_POINTER );
}

/*
 * ofaICollector interface management
 */
static void
icollector_iface_init( ofaICollectorInterface *iface )
{
	static const gchar *thisfn = "ofa_hub_icollector_iface_init";

	g_debug( "%s: iface=%p", thisfn, ( void * ) iface );

	iface->get_interface_version = icollector_get_interface_version;
}

static guint
icollector_get_interface_version( const ofaICollector *instance )
{
	return( 1 );
}

/*
 * ofa_hub_new_with_connect:
 * @connect: a valid connection to the targeted database.
 *
 * Opens the dossier and exercice pointed to by the @connect connection.
 * On success, the new #ofaHub takes a reference on this @connect
 * connection, which thus may be then released by the caller.
 *
 * Returns: a new #ofaHub object, if the @connect connection was
 * eventually valid, %NULL else.
 *
 * This method is expected to be only called from #ofaIHubber::new_hub()
 * implementation.
 */
ofaHub *
ofa_hub_new_with_connect( const ofaIDBConnect *connect )
{
	ofaHub *hub;
	ofaHubPrivate *priv;
	gboolean ok;

	g_return_val_if_fail( connect && OFA_IS_IDBCONNECT( connect ), NULL );

	hub = g_object_new( OFA_TYPE_HUB, NULL );
	priv = hub->priv;
	priv->connect = g_object_ref(( gpointer ) connect );
	ok = FALSE;

	if( ofa_idbmodel_update( hub )){
		priv->dossier = ofo_dossier_new_with_hub( hub );
		if( priv->dossier ){
			init_signaling_system( hub );
			ok = TRUE;
		}
	}

	if( !ok ){
		g_clear_object( &hub );
	}

	return( hub );
}

/*
 * be sure object class handlers are connected to the dossier signaling
 * system, as they may be needed before the class has the opportunity
 * to initialize itself
 *
 * example of a use case: the intermediate closing by ledger may be run
 * without having first loaded the accounts, but the accounts should be
 * connected in order to update themselves.
 */
static void
init_signaling_system( const ofaHub *hub )
{
	static const gchar *thisfn = "ofa_hub_init_signaling_system";

	g_debug( "%s: hub=%p", thisfn, ( void * ) hub );

	ofo_account_connect_signaling_system( hub );
	ofo_bat_connect_signaling_system( hub );
	ofo_class_connect_signaling_system( hub );
	ofo_concil_connect_signaling_system( hub );
	ofo_currency_connect_signaling_system( hub );
	ofo_entry_connect_signaling_system( hub );
	ofo_ledger_connect_signaling_system( hub );
	ofo_ope_template_connect_signaling_system( hub );
	ofo_rate_connect_signaling_system( hub );

	ofa_idbmodel_init_hub_signaling_system( hub );
}

/*
 * ofa_hub_get_connect:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofaIDBConnect connection object.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
const ofaIDBConnect *
ofa_hub_get_connect( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = hub->priv;

	if( priv->dispose_has_run ){
		g_return_val_if_reached( NULL );
	}

	return( priv->connect );
}

/*
 * ofa_hub_get_dossier:
 * @hub: this #ofaHub instance.
 *
 * Returns: the #ofoDossier object.
 *
 * The returned reference is owned by the @hub object, and should
 * not be released by the caller.
 */
ofoDossier *
ofa_hub_get_dossier( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), NULL );

	priv = hub->priv;

	if( priv->dispose_has_run ){
		g_return_val_if_reached( NULL );
	}

	return( priv->dossier );
}

/*
 * ofa_hub_remediate_settings:
 * @hub: this #ofaHub instance.
 *
 * Make sure, and update them if needed, that dossier settings datas
 * are synchronized with the same data recorded in the database:
 * - opened or closed exercice,
 * - begin and end dates of the exercice.
 */
void
ofa_hub_remediate_settings( const ofaHub *hub )
{
	ofaHubPrivate *priv;

	g_return_if_fail( hub && OFA_IS_HUB( hub ));

	priv = hub->priv;

	if( priv->dispose_has_run ){
		g_return_if_reached();
	}

	check_db_vs_settings( hub );
}

/* when opening the dossier, make sure the settings are up to date
 * (this may not be the case when the dossier has just been restored
 *  or created)
 * The datas found in the dossier database take precedence over those
 * read from dossier settings. This is because dossier database is
 * (expected to be) updated via the Openbook software suite and so be
 * controlled, while the dossier settings may easily be tweaked by the
 * user.
 */
static void
check_db_vs_settings( const ofaHub *hub )
{
	static const gchar *thisfn = "ofa_hub_check_db_vs_settings";
	ofoDossier *dossier;
	gboolean db_current, settings_current;
	const GDate *db_begin, *db_end, *settings_begin, *settings_end;
	const ofaIDBConnect *cnx;
	ofaIDBPeriod *period;
	ofaIDBMeta *meta;
	gchar *sdbbegin, *sdbend, *ssetbegin, *ssetend;

	dossier = ofa_hub_get_dossier( hub );
	cnx = ofa_hub_get_connect( hub );

	/* data from db */
	db_current = ofo_dossier_is_current( dossier );
	db_begin = ofo_dossier_get_exe_begin( dossier );
	db_end = ofo_dossier_get_exe_end( dossier );

	/* data from settings */
	period = ofa_idbconnect_get_period( cnx );
	settings_current = ofa_idbperiod_get_current( period );
	settings_begin = ofa_idbperiod_get_begin_date( period );
	settings_end = ofa_idbperiod_get_end_date( period );

	/* update settings if not equal */
	if( db_current != settings_current ||
			my_date_compare_ex( db_begin, settings_begin, TRUE ) != 0 ||
			my_date_compare_ex( db_end, settings_end, FALSE ) != 0 ){

		sdbbegin = my_date_to_str( db_begin, MY_DATE_SQL );
		sdbend = my_date_to_str( db_end, MY_DATE_SQL );
		ssetbegin = my_date_to_str( settings_begin, MY_DATE_SQL );
		ssetend = my_date_to_str( settings_end, MY_DATE_SQL );

		g_debug( "%s: db_current=%s, db_begin=%s, db_end=%s, settings_current=%s, settings_begin=%s, settings_end=%s: updating settings",
				thisfn,
				db_current ? "True":"False", sdbbegin, sdbend,
				settings_current ? "True":"False", ssetbegin, ssetend );

		g_free( sdbbegin );
		g_free( sdbend );
		g_free( ssetbegin );
		g_free( ssetend );

		meta = ofa_idbconnect_get_meta( cnx );
		ofa_idbmeta_update_period( meta, period, db_current, db_begin, db_end );
		g_object_unref( meta );
	}

	g_object_unref( period );
}

/**
 * ofa_hub_import_csv:
 * @dossier: the target #ofoDossier
 * @object: an empty object of the class to be imported.
 * @uri: the input file
 * @settings: the #ofaFileFormat instance which describes the @uri file.
 * @caller: [allow-none]: the #ofaIImporter instance
 * @errors: [allow-none][out]: errors count
 *
 * Import a CSV file into the dossier.
 *
 * Returns: the count of imported lines.
 */
guint
ofa_hub_import_csv( ofaHub *hub, ofaIImportable *object, const gchar *uri, const ofaFileFormat *settings, void *caller, guint *errors )
{
	guint count, local_errors;
	GSList *lines;
	gchar *str;

	g_return_val_if_fail( hub && OFA_IS_HUB( hub ), 0 );
	g_return_val_if_fail( object && OFA_IS_IIMPORTABLE( object ), 0 );
	g_return_val_if_fail( my_strlen( uri ), 0 );
	g_return_val_if_fail( settings && OFA_IS_FILE_FORMAT( settings ), 0 );

	count = 0;
	local_errors = 0;
	lines = get_lines_from_csv( uri, settings );

	if( lines ){
		count = g_slist_length( lines ) - ofa_file_format_get_headers_count( settings );

		if( count > 0 ){
			local_errors = ofa_iimportable_import( object, lines, settings, hub, caller );

		} else if( count < 0 ){
			count = 0;
			local_errors += 1;
			str = g_strdup_printf(
					_( "Headers count=%u greater than count of lines read from '%s' file" ),
					ofa_file_format_get_headers_count( settings ), uri );
			my_utils_dialog_warning( str );
			g_free( str );
		}

		free_lines( lines );
	}

	if( errors ){
		*errors = local_errors;
	}

	return( count );
}

/*
 * Returns a GSList of lines, where each lines->data is a GSList of
 * fields
 */
static GSList *
get_lines_from_csv( const gchar *uri, const ofaFileFormat *settings )
{
	GFile *gfile;
	gchar *sysfname, *contents, *str;
	GError *error;
	gchar **lines, **iter_line;
	gchar **fields, **iter_field;
	GSList *s_fields, *s_lines;
	gchar *field, *field_sep;

	sysfname = my_utils_filename_from_utf8( uri );
	if( !sysfname ){
		str = g_strdup_printf( _( "Unable to get a system filename for '%s' URI" ), uri );
		my_utils_dialog_warning( str );
		g_free( str );
		return( NULL );
	}
	gfile = g_file_new_for_uri( sysfname );
	g_free( sysfname );

	error = NULL;
	contents = NULL;
	if( !g_file_load_contents( gfile, NULL, &contents, NULL, NULL, &error )){
		str = g_strdup_printf( _( "Unable to load content from '%s' file: %s" ), uri, error->message );
		my_utils_dialog_warning( str );
		g_free( str );
		g_error_free( error );
		g_free( contents );
		g_object_unref( gfile );
		return( NULL );
	}

	lines = g_strsplit( contents, "\n", -1 );

	g_free( contents );
	g_object_unref( gfile );

	s_lines = NULL;
	iter_line = lines;
	field_sep = g_strdup_printf( "%c", ofa_file_format_get_field_sep( settings ));

	while( *iter_line ){
		error = NULL;
		str = g_convert( *iter_line, -1,
								ofa_file_format_get_charmap( settings ),
								"UTF-8", NULL, NULL, &error );
		if( !str ){
			str = g_strdup_printf(
					_( "Charset conversion error: %s\nline='%s'" ), error->message, *iter_line );
			my_utils_dialog_warning( str );
			g_free( str );
			g_strfreev( lines );
			return( NULL );
		}
		if( my_strlen( *iter_line )){
			fields = g_strsplit(( const gchar * ) *iter_line, field_sep, -1 );
			s_fields = NULL;
			iter_field = fields;

			while( *iter_field ){
				field = g_strstrip( g_strdup( *iter_field ));
				s_fields = g_slist_prepend( s_fields, field );
				iter_field++;
			}

			g_strfreev( fields );
			s_lines = g_slist_prepend( s_lines, g_slist_reverse( s_fields ));
		}
		g_free( str );
		iter_line++;
	}

	g_free( field_sep );
	g_strfreev( lines );
	return( g_slist_reverse( s_lines ));
}

static void
free_fields( GSList *fields )
{
	g_slist_free_full( fields, ( GDestroyNotify ) g_free );
}

static void
free_lines( GSList *lines )
{
	g_slist_free_full( lines, ( GDestroyNotify ) free_fields );
}

/**
 * ofa_hub_disconnect_handlers:
 * @hub: this #ofaHub object.
 * @handlers: a #GList of the handler identifiers got when connecting
 *  to the hub signaling system
 *
 * Disconnect the specified @handlers from the signaling system.
 *
 * Rationale: an object should disconnect its signals when it disappears
 * while the signal emitter is still alive, i.e. to prevent the signal
 * emitter to keep sending signals to a now-disappeared object.
 */
void
ofa_hub_disconnect_handlers( ofaHub *hub, GList *handlers )
{
	static const gchar *thisfn = "ofa_hub_disconnect_handlers";
	ofaHubPrivate *priv;
	GList *it;

	g_debug( "%s: hub=%p, handlers=%p (count=%d)",
			thisfn, ( void * ) hub, ( void * ) handlers, g_list_length( handlers ));

	priv = hub->priv;

	if( !priv->dispose_has_run ){
		for( it=handlers ; it ; it=it->next ){
			g_signal_handler_disconnect( hub, ( gulong ) it->data );
		}
	}
}
